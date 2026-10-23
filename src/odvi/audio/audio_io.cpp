// WAV loading (dr_wav) with channel downmix.

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "odvi/audio/audio_io.h"

#include <cstdint>
#include <cstring>

namespace odvi {

bool load_wav(const std::string& path, AudioBuffer& out, std::string& out_error) {
    drwav wav;
    if (!drwav_init_file(&wav, path.c_str(), nullptr)) {
        out_error = "dr_wav: failed to open: " + path;
        return false;
    }

    const uint32_t channels = wav.channels;
    const uint32_t sample_rate = wav.sampleRate;
    const uint64_t frame_count = wav.totalPCMFrameCount;

    if (frame_count == 0) {
        drwav_uninit(&wav);
        out_error = "WAV file has zero frames: " + path;
        return false;
    }

    // Read interleaved float32 samples (all channels)
    std::vector<float> raw(frame_count * channels);
    const uint64_t read = drwav_read_pcm_frames_f32(&wav, frame_count, raw.data());
    drwav_uninit(&wav);

    if (read != frame_count) {
        out_error = "dr_wav: expected " + std::to_string(frame_count) + " frames, got " +
                    std::to_string(read);
        return false;
    }

    out.sample_rate = static_cast<int>(sample_rate);
    out.channels = 1;
    out.samples.resize(frame_count);

    if (channels == 1) {
        out.samples = std::move(raw);
    } else {
        // Downmix: average across channels per frame
        const float inv_ch = 1.0f / static_cast<float>(channels);
        for (uint64_t i = 0; i < frame_count; ++i) {
            float sum = 0.0f;
            for (uint32_t c = 0; c < channels; ++c) {
                sum += raw[i * channels + c];
            }
            out.samples[i] = sum * inv_ch;
        }
    }

    return true;
}

}  // namespace odvi
