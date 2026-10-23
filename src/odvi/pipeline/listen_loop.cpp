#include "odvi/pipeline/listen_loop.h"

#include <chrono>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

namespace odvi {
namespace {

int frames_to_ms(int frames, int frame_ms) {
    return frames * frame_ms;
}

const SegmenterConfig& checked_segmenter(const SegmenterConfig& cfg) {
    if (cfg.sample_rate != kInternalSampleRate) {
        throw std::invalid_argument("segmenter must run at the internal rate of " +
                                    std::to_string(kInternalSampleRate) + " Hz");
    }
    return cfg;
}

AsrResult call_transcriber(const Transcriber& transcribe, const std::vector<float>& audio) {
    if (audio.empty()) return AsrResult{"", 0.0, false, "empty utterance"};
    try {
        // Utterances are capped by SegmenterConfig::max_utterance_ms, so the
        // sample count always fits in an int.
        return transcribe(audio.data(), static_cast<int>(audio.size()));
    } catch (const std::exception& e) {
        return AsrResult{"", 0.0, false, std::string("transcriber threw: ") + e.what()};
    } catch (...) {
        return AsrResult{"", 0.0, false, "transcriber threw a non-standard exception"};
    }
}

}  // namespace

ListenLoop::ListenLoop(ListenConfig config, Transcriber transcribe)
    : config_(std::move(config)),
      resampler_(config_.input_rate, kInternalSampleRate),
      segmenter_(checked_segmenter(config_.segmenter)),
      transcribe_(std::move(transcribe)) {
    if (!transcribe_) {
        throw std::invalid_argument("listen loop needs a transcriber");
    }
}

void ListenLoop::push(const float* samples, int sample_count) {
    resampled_.clear();
    resampler_.process(samples, sample_count, resampled_);
    feed_segmenter(resampled_);
}

void ListenLoop::flush() {
    resampled_.clear();
    resampler_.flush(resampled_);
    feed_segmenter(resampled_);
    segmenter_.flush();
    drain_completed();
}

void ListenLoop::feed_segmenter(const std::vector<float>& samples) {
    if (samples.empty()) return;
    segmenter_.push(samples.data(), static_cast<int>(samples.size()));
    drain_completed();
}

std::vector<std::string> ListenLoop::pop_json_events() {
    std::vector<std::string> output;
    output.swap(ready_json_);
    return output;
}

TranscriptEvent ListenLoop::process_utterance(Utterance utterance) {
    const auto started = std::chrono::steady_clock::now();

    TranscriptEvent event;
    event.utterance_id = utterance.id;
    event.capture_device = config_.capture_device;
    event.transport = config_.transport;
    event.finalization_reason = utterance.reason;
    event.speech_ms = frames_to_ms(utterance.speech_frames, config_.segmenter.frame_ms);
    event.start_stream_frame = utterance.start_stream_frame;
    event.end_stream_frame = utterance.end_stream_frame;

    if (utterance.sample_rate != kInternalSampleRate) {
        throw std::invalid_argument("utterance must be at the internal sample rate");
    }
    event.duration_ms = static_cast<int>(
        std::llround(1000.0 * static_cast<double>(utterance.samples.size()) / kInternalSampleRate));

    const AsrResult asr = call_transcriber(transcribe_, utterance.samples);
    event.asr_ok = asr.ok;
    event.asr_ms = asr.inference_ms;
    if (asr.ok) {
        event.transcript = asr.text;
    } else {
        event.asr_error = asr.error.empty() ? "unknown ASR error" : asr.error;
        had_asr_error_ = true;
    }

    event.post_utterance_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    return event;
}

void ListenLoop::drain_completed() {
    for (auto& utterance : segmenter_.pop_utterances()) {
        ready_json_.push_back(serialize_transcript_event(process_utterance(std::move(utterance))));
    }
}

}  // namespace odvi
