# odvi_core: audio I/O and resampling, segmentation, the listening loop and event encoding.
# Pure C++17 with no model runtime, so it is fully testable without weights.
add_library(odvi_core STATIC
    odvi/audio/audio_io.cpp
    odvi/audio/raw_pcm_decoder.cpp
    odvi/audio/resampler.cpp
    odvi/segmentation/vad.cpp
    odvi/segmentation/utterance_segmenter.cpp
    odvi/pipeline/transcript_event.cpp
    odvi/pipeline/listen_loop.cpp
    odvi/report/json.cpp)
add_library(odvi::core ALIAS odvi_core)
target_include_directories(odvi_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(odvi_core
    PUBLIC  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:m>
    PRIVATE odvi::options dr_wav)

# odvi_asr: the whisper.cpp adapter. Kept separate so the core library and its
# tests never link a model runtime.
add_library(odvi_asr STATIC odvi/asr/asr_engine.cpp)
add_library(odvi::asr ALIAS odvi_asr)
target_include_directories(odvi_asr PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(odvi_asr PUBLIC odvi_core PRIVATE odvi::options whisper)
if(GGML_CUDA)
    target_compile_definitions(odvi_asr PRIVATE ODVI_USE_GPU=1)
endif()
