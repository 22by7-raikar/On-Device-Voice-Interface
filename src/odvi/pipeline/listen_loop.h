/// @file
/// @brief Streaming listening loop: PCM in, one transcript event per utterance out.
/// @ingroup pipeline

#pragma once

#include <string>
#include <vector>

#include "odvi/asr/transcription.h"
#include "odvi/audio/resampler.h"
#include "odvi/pipeline/transcript_event.h"
#include "odvi/segmentation/utterance_segmenter.h"

namespace odvi {

/// @defgroup pipeline Pipeline
/// @brief Wiring the stages together into the runtime loop.
///
/// ListenLoop is the first slice of the full pipeline: segmenter → ASR → one
/// TranscriptEvent per utterance. Later milestones insert the front end
/// (beamforming, denoising, echo cancellation) before the segmenter and the
/// agent after ASR.
/// @{

/// @brief Sample rate of everything after ingress: segmentation, ASR, events.
inline constexpr int kInternalSampleRate = 16000;

/// @brief ListenLoop settings.
struct ListenConfig {
    int input_rate = kInternalSampleRate;  ///< Rate of the samples given to push(), Hz.
    /// Endpointing parameters. `segmenter.sample_rate` must equal
    /// #kInternalSampleRate: the loop resamples at ingress, not per utterance.
    SegmenterConfig segmenter;
    std::string capture_device = "unknown";  ///< Copied into every event.
    std::string transport = "stdin_pcm";     ///< Copied into every event.
};

/// @brief Segments a PCM stream and transcribes each utterance exactly once.
///
/// Input is resampled once, at ingress, to #kInternalSampleRate with an
/// anti-aliased Resampler, so the VAD, the segmenter and ASR all see the same
/// band-limited 16 kHz signal. Per utterance, the injected Transcriber is
/// called once on the whole utterance and one JSON event is queued.
///
/// A Transcriber that throws is treated like one that returns an error: the
/// event carries the message and the loop keeps running. All work happens on
/// the caller's thread inside push()/flush(), so ASR time delays the next push;
/// moving ASR to a worker thread is milestone M3.
///
/// The Transcriber seam lets tests run the whole loop without a model.
class ListenLoop {
public:
    /// @throws std::invalid_argument if @p transcribe is empty, `input_rate` is
    ///         outside Resampler's range, or the segmenter config is invalid or
    ///         not at #kInternalSampleRate.
    ListenLoop(ListenConfig config, Transcriber transcribe);

    /// @brief Feed mono samples at `config.input_rate`. Ignored after flush().
    void push(const float* samples, int sample_count);

    /// @brief End of stream: close and transcribe any active utterance. Idempotent.
    void flush();

    /// @brief Take queued events (one JSON object per string, no newline).
    std::vector<std::string> pop_json_events();

    /// @brief Transcribe one already-segmented 16 kHz utterance and return its event.
    /// push() reaches the same code path; exposed for deterministic tests.
    /// @throws std::invalid_argument if the utterance is not at #kInternalSampleRate.
    TranscriptEvent process_utterance(Utterance utterance);

    /// @brief Segmenter counters.
    const SegmenterTelemetry& telemetry() const noexcept { return segmenter_.telemetry(); }

    /// @brief True if any utterance failed ASR.
    bool had_asr_error() const noexcept { return had_asr_error_; }

private:
    void drain_completed();
    void feed_segmenter(const std::vector<float>& samples);

    ListenConfig config_;
    Resampler resampler_;
    StreamingUtteranceSegmenter segmenter_;
    std::vector<float> resampled_;  // reused scratch buffer
    Transcriber transcribe_;
    std::vector<std::string> ready_json_;
    bool had_asr_error_ = false;
};

/// @}

}  // namespace odvi
