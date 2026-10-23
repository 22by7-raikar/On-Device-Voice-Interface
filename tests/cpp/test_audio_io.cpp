// WAV loading with downmix.

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dr_wav.h"
#include "odvi/audio/audio_io.h"

namespace {

// Write interleaved S16 samples as a WAV file and return its path.
std::string write_wav(const std::string& name, const std::vector<int16_t>& interleaved,
                      int channels, int rate) {
    const std::string path = ::testing::TempDir() + name;
    drwav_data_format format{};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = static_cast<drwav_uint32>(channels);
    format.sampleRate = static_cast<drwav_uint32>(rate);
    format.bitsPerSample = 16;
    drwav wav;
    EXPECT_TRUE(drwav_init_file_write(&wav, path.c_str(), &format, nullptr));
    const auto frames = static_cast<drwav_uint64>(interleaved.size() / channels);
    EXPECT_EQ(drwav_write_pcm_frames(&wav, frames, interleaved.data()), frames);
    drwav_uninit(&wav);
    return path;
}

}  // namespace

TEST(LoadWav, StereoIsDownmixedAndRateKept) {
    // Left = +16384, right = 0 in every frame: the mono mean is 0.25.
    std::vector<int16_t> interleaved;
    for (int i = 0; i < 100; ++i) {
        interleaved.push_back(16384);
        interleaved.push_back(0);
    }
    const auto path = write_wav("odvi_stereo.wav", interleaved, 2, 48000);

    odvi::AudioBuffer audio;
    std::string error;
    ASSERT_TRUE(odvi::load_wav(path, audio, error)) << error;
    EXPECT_EQ(audio.sample_rate, 48000);
    EXPECT_EQ(audio.channels, 1);
    ASSERT_EQ(audio.samples.size(), 100u);
    for (const float s : audio.samples) EXPECT_NEAR(s, 0.25f, 1e-4f);
}

TEST(LoadWav, MissingFileReportsThePath) {
    odvi::AudioBuffer audio;
    std::string error;
    EXPECT_FALSE(odvi::load_wav("/nonexistent/odvi.wav", audio, error));
    EXPECT_NE(error.find("/nonexistent/odvi.wav"), std::string::npos);
}
