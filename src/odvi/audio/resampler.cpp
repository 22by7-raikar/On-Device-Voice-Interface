// Kaiser-windowed sinc polyphase resampler.

#include "odvi/audio/resampler.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <string>

namespace odvi {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kStopbandDb = 80.0;        // minimum attenuation above the lower Nyquist
constexpr double kPassbandFraction = 0.85;  // passband edge as a fraction of that Nyquist

// Zeroth-order modified Bessel function of the first kind (power series).
double bessel_i0(double x) {
    double sum = 1.0;
    double term = 1.0;
    const double half_sq = 0.25 * x * x;
    for (int k = 1; k < 64; ++k) {
        term *= half_sq / (static_cast<double>(k) * k);
        sum += term;
        if (term < sum * 1e-17) break;
    }
    return sum;
}

double sinc(double x) {
    return x == 0.0 ? 1.0 : std::sin(kPi * x) / (kPi * x);
}

void check_rate(const char* name, int rate) {
    if (rate < Resampler::kMinRate || rate > Resampler::kMaxRate) {
        throw std::invalid_argument(
            std::string(name) + " must be in [" + std::to_string(Resampler::kMinRate) + ", " +
            std::to_string(Resampler::kMaxRate) + "] Hz, got " + std::to_string(rate));
    }
}

}  // namespace

Resampler::Resampler(int input_rate, int output_rate)
    : input_rate_(input_rate), output_rate_(output_rate) {
    check_rate("input rate", input_rate);
    check_rate("output rate", output_rate);
    const int g = std::gcd(input_rate, output_rate);
    up_ = output_rate / g;
    down_ = input_rate / g;

    if (up_ == 1 && down_ == 1) {
        // Equal rates: a single unit tap, so the stream passes through unchanged.
        taps_ = 1;
        delay_ = 0;
        lookahead_ = 0;
        phases_.assign(1, 1.0f);
        history_.assign(1, 0.0f);
        history_start_ = -1;
        return;
    }

    // Kaiser design (Kaiser 1974; Oppenheim & Schafer, 3rd ed., §7.6) at the
    // virtual rate L * input_rate, where the zero-stuffed signal lives.
    const double virtual_rate = static_cast<double>(up_) * input_rate;
    const double nyquist = 0.5 * std::min(input_rate, output_rate);
    const double pass_hz = kPassbandFraction * nyquist;
    const double transition = (nyquist - pass_hz) / virtual_rate;  // cycles/sample
    const double cutoff = 0.5 * (pass_hz + nyquist) / virtual_rate;
    const double beta = 0.1102 * (kStopbandDb - 8.7);
    const auto length_estimate =
        static_cast<int64_t>(std::ceil((kStopbandDb - 7.95) / (14.36 * transition))) + 1;

    taps_ = static_cast<int>((length_estimate + up_ - 1) / up_);
    const int64_t length = static_cast<int64_t>(taps_) * up_;
    delay_ = (length - 1) / 2;
    // Output n reads input up to floor((n*M + delay) / L); relative to its own
    // time n*M/L that is at most delay/L + 1 samples ahead.
    lookahead_ = static_cast<int>(delay_ / up_) + 1;

    // h[j], j = p + k*L, stored phase-major so each output reads K contiguous taps.
    // The filter is centred on the integer delay so the group delay is exactly
    // compensated; when K*L is even its last tap is zero. The gain of L
    // restores the amplitude lost to zero-stuffing.
    phases_.assign(static_cast<std::size_t>(length), 0.0f);
    const auto centre = static_cast<double>(delay_);
    const double i0_beta = bessel_i0(beta);
    for (int64_t j = 0; j <= 2 * delay_; ++j) {
        const double t = static_cast<double>(j) - centre;
        const double r = t / centre;
        const double window = bessel_i0(beta * std::sqrt(1.0 - r * r)) / i0_beta;
        const double h = up_ * 2.0 * cutoff * sinc(2.0 * cutoff * t) * window;
        phases_[static_cast<std::size_t>((j % up_) * taps_ + j / up_)] = static_cast<float>(h);
    }

    // Inputs before the stream starts are silence: index -K .. -1.
    history_.assign(static_cast<std::size_t>(taps_), 0.0f);
    history_start_ = -taps_;
}

float Resampler::input_at(int64_t index) const {
    if (index < history_start_ || index >= inputs_) return 0.0f;
    return history_[static_cast<std::size_t>(index - history_start_)];
}

float Resampler::compute(int64_t output_index) const {
    const int64_t position = output_index * down_ + delay_;
    const int64_t newest = position / up_;
    const int64_t phase = position % up_;
    const float* coeffs = phases_.data() + phase * taps_;
    double acc = 0.0;
    if (newest < inputs_ && newest - (taps_ - 1) >= history_start_) {
        const float* x = history_.data() + (newest - history_start_);
        for (int k = 0; k < taps_; ++k) acc += static_cast<double>(coeffs[k]) * x[-k];
    } else {
        for (int k = 0; k < taps_; ++k) {
            acc += static_cast<double>(coeffs[k]) * input_at(newest - k);
        }
    }
    return static_cast<float>(acc);
}

void Resampler::process(const float* samples, int count, std::vector<float>& out) {
    if (flushed_ || samples == nullptr || count <= 0) return;
    history_.insert(history_.end(), samples, samples + count);
    inputs_ += count;

    while ((outputs_ * down_ + delay_) / up_ < inputs_) {
        out.push_back(compute(outputs_));
        ++outputs_;
    }

    // Keep only the K-1 samples before the next output's newest input.
    const int64_t keep_from = (outputs_ * down_ + delay_) / up_ - (taps_ - 1);
    if (keep_from > history_start_) {
        const int64_t drop =
            std::min<int64_t>(keep_from - history_start_, static_cast<int64_t>(history_.size()));
        history_.erase(history_.begin(), history_.begin() + drop);
        history_start_ += drop;
    }
}

void Resampler::flush(std::vector<float>& out) {
    if (flushed_) return;
    flushed_ = true;
    const int64_t total = (inputs_ * up_ + down_ - 1) / down_;
    while (outputs_ < total) {
        out.push_back(compute(outputs_));
        ++outputs_;
    }
}

std::vector<float> resample(const std::vector<float>& samples, int input_rate, int output_rate) {
    Resampler resampler(input_rate, output_rate);  // validates both rates
    if (input_rate == output_rate) return samples;
    std::vector<float> out;
    out.reserve(static_cast<std::size_t>(
        (static_cast<int64_t>(samples.size()) * resampler.upsample_factor() +
         resampler.downsample_factor() - 1) /
        resampler.downsample_factor()));
    if (!samples.empty()) {
        // Feed in bounded blocks so the int count never overflows.
        constexpr std::size_t kBlock = 1 << 20;
        for (std::size_t i = 0; i < samples.size(); i += kBlock) {
            const std::size_t n = std::min(kBlock, samples.size() - i);
            resampler.process(samples.data() + i, static_cast<int>(n), out);
        }
    }
    resampler.flush(out);
    return out;
}

}  // namespace odvi
