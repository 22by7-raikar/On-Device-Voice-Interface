// Resampler: ratio reduction, length, passband accuracy and timing, alias and
// image rejection, and streaming/one-shot equivalence.

#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "odvi/audio/resampler.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

std::vector<float> sine(int rate, double freq_hz, double seconds, double amplitude = 0.5) {
    const auto n = static_cast<std::size_t>(seconds * rate);
    std::vector<float> v(n);
    for (std::size_t i = 0; i < n; ++i) {
        v[i] = static_cast<float>(amplitude * std::sin(2.0 * kPi * freq_hz * i / rate));
    }
    return v;
}

// RMS over [begin, end) relative to the RMS of a full-scale-0.5 sine, in dB.
double level_db(const std::vector<float>& v, std::size_t begin, std::size_t end) {
    double sum = 0.0;
    for (std::size_t i = begin; i < end; ++i) sum += static_cast<double>(v[i]) * v[i];
    const double rms = std::sqrt(sum / static_cast<double>(end - begin));
    return 20.0 * std::log10(rms / (0.5 / std::sqrt(2.0)) + 1e-30);
}

// Error between @p v and the ideal sine at @p rate, over the steady-state middle,
// relative to the sine's RMS, in dB. Catches gain, phase (timing) and images.
double error_vs_ideal_db(const std::vector<float>& v, int rate, double freq_hz) {
    const std::size_t begin = v.size() / 4;
    const std::size_t end = 3 * v.size() / 4;
    double err = 0.0;
    for (std::size_t i = begin; i < end; ++i) {
        const double ideal = 0.5 * std::sin(2.0 * kPi * freq_hz * i / rate);
        err += (v[i] - ideal) * (v[i] - ideal);
    }
    const double rms = std::sqrt(err / static_cast<double>(end - begin));
    return 20.0 * std::log10(rms / (0.5 / std::sqrt(2.0)) + 1e-30);
}

std::vector<float> one_shot(int in_rate, int out_rate, const std::vector<float>& x) {
    odvi::Resampler r(in_rate, out_rate);
    std::vector<float> out;
    r.process(x.data(), static_cast<int>(x.size()), out);
    r.flush(out);
    return out;
}

}  // namespace

TEST(Resampler, ReducesTheRatio) {
    const odvi::Resampler down(48000, 16000);
    EXPECT_EQ(down.upsample_factor(), 1);
    EXPECT_EQ(down.downsample_factor(), 3);
    const odvi::Resampler cd(44100, 16000);
    EXPECT_EQ(cd.upsample_factor(), 160);
    EXPECT_EQ(cd.downsample_factor(), 441);
}

TEST(Resampler, RejectsUnsupportedRates) {
    EXPECT_THROW(odvi::Resampler(4000, 16000), std::invalid_argument);
    EXPECT_THROW(odvi::Resampler(16000, 384000), std::invalid_argument);
    EXPECT_THROW(odvi::resample({0.0f}, 1, 1), std::invalid_argument);
}

TEST(Resampler, OutputLengthIsTheCeilingOfTheScaledInput) {
    const std::vector<float> x(1001, 0.1f);
    EXPECT_EQ(odvi::resample(x, 48000, 16000).size(), 334u);   // ceil(1001 / 3)
    EXPECT_EQ(odvi::resample(x, 16000, 48000).size(), 3003u);  // 1001 * 3
    EXPECT_EQ(odvi::resample(x, 44100, 16000).size(), 364u);   // ceil(1001 * 160 / 441)
    EXPECT_TRUE(odvi::resample({}, 48000, 16000).empty());
}

TEST(Resampler, EqualRatesPassThroughUnchangedAndWithoutDelay) {
    const std::vector<float> x = {0.1f, -0.2f, 0.3f, 0.9f};
    EXPECT_EQ(odvi::resample(x, 16000, 16000), x);

    odvi::Resampler r(16000, 16000);
    EXPECT_EQ(r.lookahead(), 0);
    std::vector<float> out;
    r.process(x.data(), 2, out);
    EXPECT_EQ(out, std::vector<float>(x.begin(), x.begin() + 2));  // emitted immediately
}

// A 1 kHz tone must come out as the same tone at the new rate: same gain,
// same phase (the group delay is compensated), and no images.
TEST(Resampler, PassbandToneMatchesTheIdealSignal) {
    struct Case {
        int in, out;
    };
    for (const Case c :
         {Case{48000, 16000}, Case{44100, 16000}, Case{16000, 48000}, Case{8000, 16000}}) {
        const auto y = odvi::resample(sine(c.in, 1000.0, 0.5), c.in, c.out);
        EXPECT_LT(error_vs_ideal_db(y, c.out, 1000.0), -90.0) << c.in << " -> " << c.out;
    }
}

// The defect this resampler replaced: linear interpolation folds a 10 kHz
// tone at 48 kHz down to 6 kHz, inside the speech band, at full level.
// The design guarantees at least 80 dB of rejection from the output Nyquist up.
TEST(Resampler, ContentAboveTheOutputNyquistDoesNotAlias) {
    for (const double freq : {8500.0, 10000.0, 15000.0, 23000.0}) {
        const auto y = odvi::resample(sine(48000, freq, 0.5), 48000, 16000);
        EXPECT_LT(level_db(y, y.size() / 4, 3 * y.size() / 4), -80.0) << freq << " Hz";
    }
    const auto y = odvi::resample(sine(44100, 12000.0, 0.5), 44100, 16000);
    EXPECT_LT(level_db(y, y.size() / 4, 3 * y.size() / 4), -80.0);
}

TEST(Resampler, StreamingInRandomBlocksIsBitIdenticalToOneShot) {
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> sample(-0.5f, 0.5f);
    std::uniform_int_distribution<int> block(1, 997);
    std::vector<float> x(20000);
    for (auto& s : x) s = sample(rng);

    struct Case {
        int in, out;
    };
    for (const Case c : {Case{44100, 16000}, Case{48000, 16000}, Case{16000, 48000}}) {
        odvi::Resampler r(c.in, c.out);
        std::vector<float> streamed;
        for (std::size_t i = 0; i < x.size();) {
            const auto n =
                std::min<std::size_t>(static_cast<std::size_t>(block(rng)), x.size() - i);
            r.process(x.data() + i, static_cast<int>(n), streamed);
            i += n;
        }
        r.flush(streamed);
        EXPECT_EQ(streamed, one_shot(c.in, c.out, x)) << c.in << " -> " << c.out;
    }
}

TEST(Resampler, StreamingLatencyIsBoundedByTheLookahead) {
    odvi::Resampler r(48000, 16000);
    std::vector<float> out;
    const std::vector<float> x(48000, 0.1f);
    r.process(x.data(), static_cast<int>(x.size()), out);
    // Every output whose time is at least `lookahead` input samples in the past
    // has been emitted: n * 3 + lookahead <= 48000.
    EXPECT_GE(static_cast<int64_t>(out.size()), (48000 - r.lookahead()) / 3);
    EXPECT_LE(out.size(), 16000u);
}

TEST(Resampler, FlushIsIdempotentAndEndsTheStream) {
    odvi::Resampler r(48000, 16000);
    std::vector<float> out;
    const std::vector<float> x(300, 0.1f);
    r.process(x.data(), static_cast<int>(x.size()), out);
    r.flush(out);
    ASSERT_EQ(out.size(), 100u);
    r.flush(out);
    r.process(x.data(), static_cast<int>(x.size()), out);
    EXPECT_EQ(out.size(), 100u);
}
