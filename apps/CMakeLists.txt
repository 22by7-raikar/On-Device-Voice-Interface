# odvi_listen: streaming PCM on stdin -> utterances -> ASR -> transcript events
# (JSON Lines on stdout).
add_executable(odvi_listen odvi_listen.cpp)
target_link_libraries(odvi_listen PRIVATE odvi::asr odvi::options)
