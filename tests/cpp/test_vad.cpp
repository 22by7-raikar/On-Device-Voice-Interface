// Energy/ZCR frame classifier and offline region detection.

#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "odvi/segmentation/vad.h"

namespace {

constexpr int kRate = 16000;
constexpr float kPi = 3.14159265358979f;

std::vector<float> sine(int n, float freq_hz, float amplitude) {
    std::vector<float> v(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        v[static_cast<std::size_t>(i)] = amplitude * std::sin(2.0f * kPi * freq_hz * i / kRate);
    }
    return v;
}

// Deterministic white noise in [-amplitude, amplitude).
std::vector<float> white_noise(int n, float amplitude) {
    std::vector<float> v(static_cast<std::size_t>(n));
    uint32_t state = 0xdeadbeef;
    for (auto& s : v) {
        state = state * 1664525u + 1013904223u;
        s = amplitude * static_cast<float>(static_cast<int32_t>(state)) / 2147483648.0f;
    }
    return v;
}

// pre_sec of silence, speech_sec of a 200 Hz tone, post_sec of silence.
std::vector<float> tone_in_silence(float pre_sec, float speech_sec, float post_sec) {
    const auto pre = static_cast<std::size_t>(pre_sec * kRate);
    const auto tone = sine(static_cast<int>(speech_sec * kRate), 200.0f, 0.2f);
    std::vector<float> v(pre, 0.0f);
    v.insert(v.end(), tone.begin(), tone.end());
    v.resize(v.size() + static_cast<std::size_t>(post_sec * kRate), 0.0f);
    return v;
}

int frame_samples(const odvi::VadConfig& cfg) {
    return cfg.frame_ms * kRate / 1000;
}

}  // namespace

TEST(VadFrame, SilenceIsNotSpeech) {
    const odvi::VadConfig cfg;
    const std::vector<float> frame(static_cast<std::size_t>(frame_samples(cfg)), 0.0f);
    EXPECT_FALSE(odvi::vad_frame_is_speech(frame.data(), frame_samples(cfg), kRate, cfg));
}

TEST(VadFrame, VoicedToneIsSpeech) {
    const odvi::VadConfig cfg;
    const auto frame = sine(frame_samples(cfg), 200.0f, 0.2f);
    EXPECT_TRUE(odvi::vad_frame_is_speech(frame.data(), frame_samples(cfg), kRate, cfg));
}

TEST(VadFrame, HighZeroCrossingRateIsNotSpeech) {
    const odvi::VadConfig cfg;
    std::vector<float> frame(static_cast<std::size_t>(frame_samples(cfg)));
    for (std::size_t i = 0; i < frame.size(); ++i) frame[i] = (i % 2 == 0) ? 0.1f : -0.1f;
    EXPECT_FALSE(odvi::vad_frame_is_speech(frame.data(), frame_samples(cfg), kRate, cfg));
}

TEST(RunVad, AllSilenceHasNoRegions) {
    const std::vector<float> sig(kRate * 3, 0.0f);
    EXPECT_TRUE(odvi::run_vad(sig.data(), static_cast<int>(sig.size()), kRate).empty());
}

TEST(RunVad, ToneInSilenceIsOneRegion) {
    const auto sig = tone_in_silence(0.5f, 1.0f, 0.5f);
    const auto segs = odvi::run_vad(sig.data(), static_cast<int>(sig.size()), kRate);
    ASSERT_EQ(segs.size(), 1u);
    EXPECT_LT(segs[0].start_sec, 0.6);
    EXPECT_GT(segs[0].end_sec, 1.3);
    EXPECT_LE(segs[0].end_sec, 2.05);  // never past the end of the buffer
}

TEST(RunVad, BurstShorterThanMinSpeechIsDropped) {
    // 80 ms of tone plus 8 hangover frames at a 10 ms hop is about 160 ms < 200 ms.
    const auto sig = tone_in_silence(0.5f, 0.08f, 1.0f);
    EXPECT_TRUE(odvi::run_vad(sig.data(), static_cast<int>(sig.size()), kRate).empty());
}

TEST(RunVad, ContinuousToneCoversTheBuffer) {
    const auto sig = sine(kRate * 3, 200.0f, 0.2f);
    const auto segs = odvi::run_vad(sig.data(), static_cast<int>(sig.size()), kRate);
    ASSERT_EQ(segs.size(), 1u);
    EXPECT_LT(segs[0].start_sec, 0.1);
    EXPECT_GT(segs[0].duration_sec(), 2.5);
}

TEST(RunVad, WhiteNoiseIsRejectedByZeroCrossingRate) {
    // White noise crosses zero about 8000 times/s, far above zcr_max.
    const auto sig = white_noise(kRate * 3, 0.3f);
    EXPECT_TRUE(odvi::run_vad(sig.data(), static_cast<int>(sig.size()), kRate).empty());
}
