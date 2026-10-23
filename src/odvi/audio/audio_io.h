/// @file
/// @brief Mono float PCM buffers and WAV loading.
/// @ingroup audio

#pragma once

#include <string>
#include <vector>

namespace odvi {

/// @defgroup audio Audio input
/// @brief Getting audio into the runtime as mono float PCM.
///
/// Two entry points: load_wav() for complete files and RawPcmDecoder for
/// unbounded S16LE streams. Both produce mono `float` samples in [-1, 1].
/// Resampler converts either to the 16 kHz the rest of the runtime uses.
/// @{

/// @brief A contiguous block of mono float32 PCM in [-1, 1].
struct AudioBuffer {
    std::vector<float> samples;  ///< Mono samples, normalised to [-1, 1].
    int sample_rate = 0;         ///< Sample rate in Hz; 0 means empty.
    int channels = 1;            ///< Always 1: multi-channel input is downmixed on load.

    /// @brief Duration in seconds, or 0 for an empty buffer.
    double duration_sec() const {
        if (sample_rate == 0 || samples.empty()) return 0.0;
        return static_cast<double>(samples.size()) / sample_rate;
    }
};

/// @brief Load a WAV file as mono float PCM.
///
/// Any integer or float WAV encoding supported by dr_wav is accepted.
/// Multi-channel audio is downmixed by averaging channels; the sample rate is
/// preserved; convert it with resample().
///
/// @param path  Path to the WAV file.
/// @param out   Receives the decoded audio on success; unspecified on failure.
/// @param error Receives a human-readable message on failure.
/// @return `true` on success.
bool load_wav(const std::string& path, AudioBuffer& out, std::string& error);

/// @}

}  // namespace odvi
