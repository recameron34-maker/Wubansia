# Build defaults for our targets (docs/decisions/0003-deterministic-floating-point.md).
# The same inputs must give the same bytes on every platform, so: no fast math, and no
# fused multiply-add contraction (arm64 would fuse a*b+c where x86_64 does not).
# JUCE module sources compile inside the targets that link this, so they get it too.

add_library(wubansia_build_defaults INTERFACE)

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC") # cl or clang-cl
    target_compile_options(wubansia_build_defaults INTERFACE
        /fp:precise
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(wubansia_build_defaults INTERFACE /clang:-ffp-contract=off)
    endif()
else()
    target_compile_options(wubansia_build_defaults INTERFACE -ffp-contract=off)
endif()
