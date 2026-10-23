/// @file
/// @brief Streaming listening loop: PCM in, one transcript event per utterance out.
/// @ingroup pipeline

#pragma once

#include <string>
#include <vector>

#include "odvi/asr/transcription.h"
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

/// @brief ListenLoop settings.
struct ListenConfig {
    SegmenterConfig segmenter;               ///< Endpointing parameters and input rate.
    std::string capture_device = "unknown";  ///< Copied into every event.
    std::string transport = "stdin_pcm";     ///< Copied into every event.
};

/// @brief Segments a PCM stream and transcribes each utterance exactly once.
///
/// Per utterance: resample to 16 kHz if needed, call the injected Transcriber
/// once on the whole utterance, and queue one JSON event. All work happens on
/// the caller's thread inside push()/flush(), so ASR time delays the next push;
/// moving ASR to a worker thread is milestone M3.
///
/// The Transcriber seam lets tests run the whole loop without a model.
class ListenLoop {
public:
    /// @throws std::invalid_argument if @p transcribe is empty or the segmenter
    ///         config is invalid.
    ListenLoop(ListenConfig config, Transcriber transcribe);

    /// @brief Feed mono samples at `config.segmenter.sample_rate`.
    void push(const float* samples, int sample_count);

    /// @brief End of stream: close and transcribe any active utterance. Idempotent.
    void flush();

    /// @brief Take queued events (one JSON object per string, no newline).
    std::vector<std::string> pop_json_events();

    /// @brief Transcribe one already-segmented utterance and return its event.
    /// push() reaches the same code path; exposed for deterministic tests.
    TranscriptEvent process_utterance(Utterance utterance);

    /// @brief Segmenter counters.
    const SegmenterTelemetry& telemetry() const noexcept { return segmenter_.telemetry(); }

    /// @brief True if any utterance failed ASR.
    bool had_asr_error() const noexcept { return had_asr_error_; }

private:
    void drain_completed();

    ListenConfig config_;
    StreamingUtteranceSegmenter segmenter_;
    Transcriber transcribe_;
    std::vector<std::string> ready_json_;
    bool had_asr_error_ = false;
};

/// @}

}  // namespace odvi
