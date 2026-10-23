/// @file
/// @brief Energy/zero-crossing voice activity detection.
/// @ingroup segmentation

#pragma once

#include <vector>

namespace odvi {

/// @defgroup segmentation Segmentation
/// @brief Deciding which frames are speech and cutting a stream into utterances.
///
/// vad_frame_is_speech() is the per-frame energy/ZCR classifier; it is the
/// baseline the benchmark compares learned VADs against.
/// StreamingUtteranceSegmenter turns those decisions into bounded utterances.
/// @{

/// @brief Frame classifier and region post-processing parameters.
struct VadConfig {
    int frame_ms = 20;             ///< Analysis frame length (320 samples at 16 kHz).
    int hop_ms = 10;               ///< Frame hop.
    float energy_thresh = 0.005f;  ///< Minimum frame RMS for speech.
    /// Maximum zero-crossing rate for speech, in crossings/s. Voiced speech is
    /// roughly 60–700; white noise roughly 8000.
    float zcr_max = 3000.0f;
    int hangover_frames = 8;   ///< Frames speech is held after the last active frame.
    int min_speech_ms = 200;   ///< Regions shorter than this are dropped.
    int min_silence_ms = 100;  ///< Gaps shorter than this are merged.
};

/// @brief A detected speech region.
struct VadSegment {
    double start_sec = 0.0;     ///< Region start, seconds.
    double end_sec = 0.0;       ///< Region end, seconds.
    float speech_ratio = 0.0f;  ///< Fraction of frames in the region classified as speech.
    int frame_count = 0;        ///< Number of hop frames spanned.

    /// @brief Region length in seconds.
    double duration_sec() const { return end_sec - start_sec; }
};

/// @brief Classify one frame: speech iff RMS >= `energy_thresh` and ZCR < `zcr_max`.
///
/// This is the per-frame decision shared by run_vad() and
/// StreamingUtteranceSegmenter::push().
bool vad_frame_is_speech(const float* frame, int frame_samples, int sample_rate,
                         const VadConfig& cfg = VadConfig{});

/// @brief Detect speech regions in a complete buffer.
///
/// Classifies frames with vad_frame_is_speech(), applies hangover, merges gaps
/// shorter than `min_silence_ms`, and drops regions shorter than `min_speech_ms`.
///
/// @return Regions in chronological order; empty if no speech is found.
std::vector<VadSegment> run_vad(const float* samples, int n_samples, int sample_rate,
                                const VadConfig& cfg = VadConfig{});

/// @}

}  // namespace odvi
