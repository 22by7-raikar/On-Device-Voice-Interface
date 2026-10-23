/// @file
/// @brief The per-utterance event the listening loop emits, and its JSON encoding.
/// @ingroup pipeline

#pragma once

#include <cstdint>
#include <string>

#include "odvi/segmentation/utterance_segmenter.h"

namespace odvi {

/// @addtogroup pipeline
/// @{

/// @brief One completed utterance: where it came from, how it was cut, what
/// ASR heard, and how long each stage took.
///
/// Every utterance produces exactly one event, including ones whose ASR call
/// failed, so a consumer sees every decision. Field names match the JSON keys
/// written by serialize_transcript_event(); docs/architecture.md has the schema.
struct TranscriptEvent {
    uint32_t utterance_id = 0;               ///< From the segmenter, starting at 1.
    std::string capture_device = "unknown";  ///< Caller-supplied source label.
    std::string transport = "stdin_pcm";     ///< Caller-supplied transport label.
    int duration_ms = 0;                     ///< Utterance length, including pre-roll.
    int speech_ms = 0;                       ///< Frames the VAD classified as speech.
    SegmentReason finalization_reason = SegmentReason::end_silence;  ///< Why it closed.
    bool asr_ok = false;                                             ///< Whether ASR succeeded.
    std::string asr_error;                                           ///< ASR failure, if any.
    std::string transcript;          ///< ASR text (empty on failure).
    double asr_ms = 0.0;             ///< ASR decode time.
    double post_utterance_ms = 0.0;  ///< End of utterance to event, total.
    int64_t start_stream_frame = 0;  ///< Stream frame index of the first frame.
    int64_t end_stream_frame = 0;    ///< Stream frame index of the last frame.
};

/// @brief Lower-case name of a SegmentReason, e.g. `end_silence`.
const char* segment_reason_str(SegmentReason reason) noexcept;

/// @brief One JSON object on a single line (no trailing newline).
std::string serialize_transcript_event(const TranscriptEvent& event);

/// @}

}  // namespace odvi
