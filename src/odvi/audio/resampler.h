/// @file
/// @brief Anti-aliased rational sample-rate conversion, streaming or one-shot.
/// @ingroup audio

#pragma once

#include <cstdint>
#include <vector>

namespace odvi {

/// @addtogroup audio
/// @{

/// @brief Polyphase resampler by a rational factor L/M with a Kaiser-windowed
/// sinc low-pass filter.
///
/// The conversion is `output_rate / input_rate = L / M` after reducing by the
/// greatest common divisor, so 48 kHz → 16 kHz is 1/3 and 44.1 kHz → 16 kHz is
/// 160/441. One prototype filter is designed at the virtual rate
/// `L * input_rate` and stored as L phases of K taps; each output sample costs
/// K multiply-adds.
///
/// The filter passes up to 85% of the lower Nyquist frequency and attenuates
/// at least 80 dB from that Nyquist frequency upwards, so content above the
/// output band cannot alias into it. At 16 kHz output the passband is 0–6.8 kHz
/// and the stopband starts at 8 kHz.
///
/// **Timing.** Output is time-aligned with the input: output sample `n`
/// corresponds to input time `n * M / L`, with the filter's group delay
/// compensated. The price is look-ahead: in streaming use, output sample `n`
/// is only emitted once the input reaches about `n * M / L + lookahead()`.
///
/// **Streaming equals one-shot.** Feeding the same input in any split, then
/// calling flush(), produces bit-identical output to a single process() of the
/// whole buffer followed by flush(). resample() is exactly that.
///
/// Equal input and output rates pass samples through unchanged.
///
/// Not thread-safe; use from one thread.
class Resampler {
public:
    static constexpr int kMinRate = 8000;    ///< Lowest supported rate, Hz.
    static constexpr int kMaxRate = 192000;  ///< Highest supported rate, Hz.

    /// @throws std::invalid_argument unless both rates are in [#kMinRate, #kMaxRate].
    Resampler(int input_rate, int output_rate);

    /// @brief Consume @p count samples and append every output sample that is
    /// now fully determined to @p out. Ignored after flush().
    void process(const float* samples, int count, std::vector<float>& out);

    /// @brief End of input: append the remaining output samples, treating
    /// input past the end as silence. Afterwards the total output length is
    /// `ceil(input_samples * L / M)`. Idempotent.
    void flush(std::vector<float>& out);

    int input_rate() const noexcept { return input_rate_; }    ///< Input rate, Hz.
    int output_rate() const noexcept { return output_rate_; }  ///< Output rate, Hz.
    int upsample_factor() const noexcept { return up_; }       ///< L.
    int downsample_factor() const noexcept { return down_; }   ///< M.
    int taps_per_phase() const noexcept { return taps_; }      ///< K.

    /// @brief Input samples needed beyond an output's own time before it can
    /// be emitted: the streaming latency, in input samples.
    int lookahead() const noexcept { return lookahead_; }

private:
    float compute(int64_t output_index) const;
    float input_at(int64_t index) const;

    int input_rate_;
    int output_rate_;
    int up_;    // L
    int down_;  // M
    int taps_;  // K
    int lookahead_;
    int64_t delay_;               // group delay at the virtual rate L * input_rate
    std::vector<float> phases_;   // L phases x K taps, phase-major
    std::vector<float> history_;  // input samples from history_start_ onwards
    int64_t history_start_ = 0;
    int64_t inputs_ = 0;   // input samples consumed
    int64_t outputs_ = 0;  // output samples emitted
    bool flushed_ = false;
};

/// @brief Resample a complete buffer: a Resampler fed the whole buffer, then flushed.
///
/// @param samples     Mono input.
/// @param input_rate  Input rate, Hz.
/// @param output_rate Output rate, Hz.
/// @return `ceil(samples.size() * output_rate / input_rate)` samples, after
///         reducing the ratio; a copy of @p samples if the rates match.
/// @throws std::invalid_argument for rates outside Resampler's supported range.
std::vector<float> resample(const std::vector<float>& samples, int input_rate, int output_rate);

/// @}

}  // namespace odvi
