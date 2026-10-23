// Runs the real ListenLoop with a fake transcriber and prints its JSON Lines,
// so test_listen_protocol.py can check the stdout protocol end to end.

#include <cmath>
#include <iostream>
#include <vector>

#include "odvi/pipeline/listen_loop.h"

namespace {

constexpr int kRate = 16000;
constexpr int kFrameSamples = 320;

void push_frames(odvi::ListenLoop& loop, const std::vector<float>& frame, int count) {
    for (int i = 0; i < count; ++i) loop.push(frame.data(), static_cast<int>(frame.size()));
}

}  // namespace

int main() {
    odvi::ListenConfig config;
    config.segmenter.sample_rate = kRate;
    config.capture_device = "protocol_fixture";
    config.transport = "test_callback";

    int asr_calls = 0;
    odvi::ListenLoop loop(config, [&](const float*, int) {
        return odvi::AsrResult{asr_calls++ == 0 ? "Look left!" : "please stop", 1.25, true, ""};
    });

    const std::vector<float> silence(kFrameSamples, 0.0f);
    std::vector<float> speech(kFrameSamples);
    for (int i = 0; i < kFrameSamples; ++i) {
        speech[static_cast<std::size_t>(i)] =
            0.2f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * 1000.0 * i / kRate));
    }

    push_frames(loop, silence, 40);  // silence alone emits nothing
    push_frames(loop, speech, 15);   // utterance 1 closes on end silence
    push_frames(loop, silence, 30);
    push_frames(loop, silence, 12);
    push_frames(loop, speech, 15);  // utterance 2 closes on flush
    loop.flush();
    loop.flush();

    for (const auto& json : loop.pop_json_events()) std::cout << json << '\n';
    return asr_calls == 2 ? 0 : 1;
}
