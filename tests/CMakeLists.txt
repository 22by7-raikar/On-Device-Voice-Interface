# Unit and integration tests. None of them needs whisper.cpp or model weights:
# ASR is replaced by an injected Transcriber.

add_executable(odvi_tests
    cpp/test_audio_io.cpp
    cpp/test_raw_pcm_decoder.cpp
    cpp/test_resampler.cpp
    cpp/test_vad.cpp
    cpp/test_utterance_segmenter.cpp
    cpp/test_listen_loop.cpp)
target_link_libraries(odvi_tests PRIVATE odvi::core dr_wav GTest::gtest_main)
gtest_discover_tests(odvi_tests DISCOVERY_MODE PRE_TEST)

# End-to-end check of the odvi_listen stdout protocol: the driver runs the real
# ListenLoop with a fake transcriber; the Python script validates its JSON Lines
# and the CLI's argument validation.
add_executable(listen_protocol_driver cpp/listen_protocol_driver.cpp)
target_link_libraries(listen_protocol_driver PRIVATE odvi::core)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME listen_protocol
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/cpp/test_listen_protocol.py"
                $<TARGET_FILE:listen_protocol_driver> $<TARGET_FILE:odvi_listen>)
endif()
