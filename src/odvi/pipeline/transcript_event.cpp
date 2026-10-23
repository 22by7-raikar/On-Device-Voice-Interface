#include "odvi/pipeline/transcript_event.h"

#include <cmath>
#include <iomanip>
#include <sstream>

#include "odvi/report/json.h"

namespace odvi {
namespace {

double finite_or_zero(double value) noexcept {
    return std::isfinite(value) ? value : 0.0;
}

}  // namespace

const char* segment_reason_str(SegmentReason reason) noexcept {
    switch (reason) {
        case SegmentReason::end_silence:
            return "end_silence";
        case SegmentReason::max_duration:
            return "max_duration";
        case SegmentReason::end_of_stream:
            return "end_of_stream";
    }
    return "end_of_stream";
}

std::string serialize_transcript_event(const TranscriptEvent& event) {
    std::ostringstream out;
    out << std::setprecision(17);
    out << "{\"event\":\"transcript\"";
    out << ",\"utterance_id\":" << event.utterance_id;
    out << ",\"capture_device\":" << json_quote(event.capture_device);
    out << ",\"transport\":" << json_quote(event.transport);
    out << ",\"duration_ms\":" << event.duration_ms;
    out << ",\"speech_ms\":" << event.speech_ms;
    out << ",\"finalization_reason\":\"" << segment_reason_str(event.finalization_reason) << '"';
    out << ",\"asr_ok\":" << (event.asr_ok ? "true" : "false");
    out << ",\"asr_error\":" << json_quote(event.asr_error);
    out << ",\"transcript\":" << json_quote(event.transcript);
    out << ",\"latency\":{\"asr_ms\":" << finite_or_zero(event.asr_ms)
        << ",\"post_utterance_ms\":" << finite_or_zero(event.post_utterance_ms) << '}';
    out << ",\"stream\":{\"start_frame\":" << event.start_stream_frame
        << ",\"end_frame\":" << event.end_stream_frame << "}}";
    return out.str();
}

}  // namespace odvi
