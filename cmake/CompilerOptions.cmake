# Compiler settings applied to first-party targets only (never to vendored code).

add_library(odvi_options INTERFACE)
add_library(odvi::options ALIAS odvi_options)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(odvi_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow)
    if(ODVI_WERROR)
        target_compile_options(odvi_options INTERFACE -Werror)
    endif()
    if(ODVI_NATIVE)
        target_compile_options(odvi_options INTERFACE -march=native -mtune=native)
    endif()
elseif(MSVC)
    target_compile_options(odvi_options INTERFACE /W4 /permissive-)
    if(ODVI_WERROR)
        target_compile_options(odvi_options INTERFACE /WX)
    endif()
endif()

# -ffast-math is deliberately not offered: DSP and evaluation results must be
# reproducible across compilers and build types.
