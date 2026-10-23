#include "odvi/pipeline/listen_loop.h"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "odvi/audio/audio_io.h"

namespace odvi {
namespace {

constexpr int kAsrRate = 16000;  // whisper.cpp's only input rate

int frames_to_ms(int frames, int frame_ms) {
    return frames * frame_ms;
}

}  // namespace

ListenLoop::ListenLoop(ListenConfig config, Transcriber transcribe)
    : config_(std::move(config)),
      segmenter_(config_.segmenter),
      transcribe_(std::move(transcribe)) {
    if (!transcribe_) {
        throw std::invalid_argument("listen loop needs a transcriber");
    }
}

void ListenLoop::push(const float* samples, int sample_count) {
    segmenter_.push(samples, sample_count);
    drain_completed();
}

void ListenLoop::flush() {
    segmenter_.flush();
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

    AudioBuffer audio;
    audio.samples = std::move(utterance.samples);
    audio.sample_rate = utterance.sample_rate;
    event.duration_ms = static_cast<int>(std::llround(audio.duration_sec() * 1000.0));
    audio = resample(audio, kAsrRate);

    // Utterances are capped by SegmenterConfig::max_utterance_ms, so the
    // sample count always fits in an int.
    const AsrResult asr =
        audio.samples.empty()
            ? AsrResult{"", 0.0, false, "empty utterance"}
            : transcribe_(audio.samples.data(), static_cast<int>(audio.samples.size()));
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
