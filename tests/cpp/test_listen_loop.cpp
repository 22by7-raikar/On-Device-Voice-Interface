// ListenLoop: one ASR call per utterance, resampling, failure reporting, event JSON.

#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "odvi/pipeline/listen_loop.h"
#include "odvi/pipeline/transcript_event.h"

namespace {

constexpr float kPi = 3.14159265358979f;

std::vector<float> tone_frame(int samples, int rate) {
    std::vector<float> frame(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        frame[static_cast<std::size_t>(i)] = 0.2f * std::sin(2.0f * kPi * 200.0f * i / rate);
    }
    return frame;
}

void push_frames(odvi::ListenLoop& loop, const std::vector<float>& frame, int count) {
    for (int i = 0; i < count; ++i) loop.push(frame.data(), static_cast<int>(frame.size()));
}

struct FakeAsr {
    int calls = 0;
    int last_sample_count = 0;
    odvi::Transcriber transcriber() {
        return [this](const float*, int n) {
            ++calls;
            last_sample_count = n;
            return odvi::AsrResult{"turn left", 2.5, true, ""};
        };
    }
};

odvi::ListenConfig config_at(int rate) {
    odvi::ListenConfig cfg;
    cfg.segmenter.sample_rate = rate;
    return cfg;
}

}  // namespace

TEST(ListenLoop, RejectsAnEmptyTranscriber) {
    EXPECT_THROW(odvi::ListenLoop(config_at(16000), odvi::Transcriber{}), std::invalid_argument);
}

TEST(ListenLoop, SilenceProducesNoEventsAndNoAsrCalls) {
    FakeAsr asr;
    odvi::ListenLoop loop(config_at(16000), asr.transcriber());
    push_frames(loop, std::vector<float>(320, 0.0f), 200);
    loop.flush();
    EXPECT_TRUE(loop.pop_json_events().empty());
    EXPECT_EQ(asr.calls, 0);
}

TEST(ListenLoop, EachUtteranceIsTranscribedExactlyOnce) {
    FakeAsr asr;
    odvi::ListenLoop loop(config_at(16000), asr.transcriber());
    const std::vector<float> silence(320, 0.0f);
    const auto tone = tone_frame(320, 16000);
    for (int utterance = 0; utterance < 3; ++utterance) {
        push_frames(loop, tone, 20);
        push_frames(loop, silence, 40);
    }
    loop.flush();
    loop.flush();
    EXPECT_EQ(loop.pop_json_events().size(), 3u);
    EXPECT_EQ(asr.calls, 3);
    EXPECT_FALSE(loop.had_asr_error());
}

TEST(ListenLoop, ResamplesTo16kHzBeforeAsr) {
    FakeAsr asr;
    odvi::ListenLoop loop(config_at(48000), asr.transcriber());
    odvi::Utterance utterance;
    utterance.samples.assign(48000, 0.1f);  // 1 s at 48 kHz
    utterance.sample_rate = 48000;
    utterance.id = 7;
    const auto event = loop.process_utterance(std::move(utterance));
    EXPECT_EQ(asr.last_sample_count, 16000);
    EXPECT_EQ(event.duration_ms, 1000);
    EXPECT_EQ(event.utterance_id, 7u);
}

TEST(ListenLoop, AsrFailureIsReportedNotThrown) {
    odvi::ListenLoop loop(config_at(16000), [](const float*, int) {
        return odvi::AsrResult{"", 1.0, false, "decoder failed"};
    });
    odvi::Utterance utterance;
    utterance.samples.assign(8000, 0.1f);
    utterance.sample_rate = 16000;
    const auto event = loop.process_utterance(std::move(utterance));
    EXPECT_FALSE(event.asr_ok);
    EXPECT_EQ(event.asr_error, "decoder failed");
    EXPECT_TRUE(event.transcript.empty());
    EXPECT_TRUE(loop.had_asr_error());
}

TEST(TranscriptEvent, SerializesEscapedSingleLineJson) {
    odvi::TranscriptEvent event;
    event.utterance_id = 3;
    event.transcript = "say \"hi\"\nnow";
    event.asr_ok = true;
    event.finalization_reason = odvi::SegmentReason::max_duration;
    const std::string json = odvi::serialize_transcript_event(event);
    EXPECT_EQ(json.find('\n'), std::string::npos);
    EXPECT_NE(json.find("\"transcript\":\"say \\\"hi\\\"\\nnow\""), std::string::npos);
    EXPECT_NE(json.find("\"finalization_reason\":\"max_duration\""), std::string::npos);
    EXPECT_EQ(json.front(), '{');
    EXPECT_EQ(json.back(), '}');
}
