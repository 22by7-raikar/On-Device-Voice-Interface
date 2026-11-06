// whisper.cpp context lifecycle and greedy transcription.

#include "odvi/asr/asr_engine.h"
#include "whisper.h"

#ifdef ODVI_USE_GPU
#include "ggml-backend.h"
#endif

#include <chrono>
#include <cstring>
#include <sstream>

namespace odvi {

namespace {

#ifdef ODVI_USE_GPU
/// Find GPU `index` the way whisper.cpp does (the index-th device of type GPU
/// or IGPU) and check that it initialises. whisper.cpp falls back to the CPU
/// without failing when this fails, so the check is the only way to know which
/// backend the context will use. Returns nullptr and sets `why` on failure.
ggml_backend_dev_t usable_gpu(int index, std::string& why) {
    int seen = 0;
    for (size_t i = 0; i < ggml_backend_dev_count(); ++i) {
        ggml_backend_dev_t dev = ggml_backend_dev_get(i);
        const auto type = ggml_backend_dev_type(dev);
        if (type != GGML_BACKEND_DEVICE_TYPE_GPU && type != GGML_BACKEND_DEVICE_TYPE_IGPU) continue;
        if (seen++ != index) continue;
        ggml_backend_t backend = ggml_backend_dev_init(dev, nullptr);
        if (!backend) {
            why = std::string("cannot initialise GPU device ") + ggml_backend_dev_name(dev);
            return nullptr;
        }
        ggml_backend_free(backend);
        return dev;
    }
    why = "no GPU device found";
    return nullptr;
}
#endif

}  // namespace

AsrEngine::AsrEngine(const AsrConfig& cfg) : cfg_(cfg) {
    whisper_context_params cparams = whisper_context_default_params();
#ifdef ODVI_USE_GPU
    gpu_requested_ = true;
    cparams.gpu_device = 0;
    if (ggml_backend_dev_t dev = usable_gpu(cparams.gpu_device, fallback_reason_)) {
        cparams.use_gpu = true;
        gpu_device_ = ggml_backend_dev_description(dev);
    } else {
        cparams.use_gpu = false;
        cpu_fallback_ = true;
    }
#else
    gpu_requested_ = false;
    cparams.use_gpu = false;
#endif

    ctx_ = whisper_init_from_file_with_params(cfg.model_path.c_str(), cparams);
    if (!ctx_ && cparams.use_gpu) {
        // The GPU initialised but loading onto it failed (for example, out of
        // VRAM). Retry on the CPU; a bad model file fails again here.
        cpu_fallback_ = true;
        fallback_reason_ = "model load on GPU failed";
        gpu_device_.clear();
        cparams.use_gpu = false;
        ctx_ = whisper_init_from_file_with_params(cfg.model_path.c_str(), cparams);
    }

    if (!ctx_) {
        load_error_ = "whisper_init_from_file_with_params failed: " + cfg.model_path;
        backend_mode_ = "unavailable";
        gpu_enabled_ = false;
        return;
    }

    gpu_enabled_ = cparams.use_gpu;
    backend_mode_ = gpu_enabled_ ? "cuda" : "cpu";
}

AsrEngine::~AsrEngine() {
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

AsrResult AsrEngine::transcribe(const float* samples, int n_samples) {
    AsrResult result;

    if (!ctx_) {
        result.error = load_error_;
        return result;
    }

    if (n_samples <= 0) {
        result.error = "zero samples";
        return result;
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = cfg_.n_threads;
    params.translate = cfg_.translate;
    params.no_timestamps = cfg_.no_timestamps;
    params.single_segment = false;
    params.print_special = false;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;

    if (!cfg_.language.empty() && cfg_.language != "auto") {
        params.language = cfg_.language.c_str();
    }

    const auto t0 = std::chrono::steady_clock::now();
    const int rc = whisper_full(ctx_, params, samples, n_samples);
    const auto t1 = std::chrono::steady_clock::now();

    result.inference_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (rc != 0) {
        result.error = "whisper_full returned " + std::to_string(rc);
        return result;
    }

    std::ostringstream oss;
    const int nseg = whisper_full_n_segments(ctx_);
    for (int i = 0; i < nseg; ++i) {
        const char* txt = whisper_full_get_segment_text(ctx_, i);
        if (txt) {
            if (i > 0) oss << ' ';
            oss << txt;
        }
    }

    result.text = oss.str();
    // trim leading whitespace that whisper often adds
    const auto pos = result.text.find_first_not_of(" \t");
    if (pos != std::string::npos) result.text = result.text.substr(pos);

    result.ok = true;
    return result;
}

}  // namespace odvi
