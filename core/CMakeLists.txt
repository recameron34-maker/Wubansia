# The groove core (ADR 0006): C++20, deterministic, integer-only decisions. It never links JUCE.
#
# Every .cpp and .h under core/, except core/tests/, goes into the static library wubansia_core:
# rng/, util/, midi/, gen/, metrics/, or any new folder. Add a file and the next build picks it up
# without editing this file (CONFIGURE_DEPENDS re-runs the glob on every build). Includes are
# relative to core/: #include "midi/writer.h".

# Our warning flags, the same lists as JUCE's juce_recommended_warning_flags (which the plugin's own
# code uses), kept here so core/ needs nothing from JUCE. GCC also gets -Wfloat-conversion: an
# implicit double-to-integer conversion in core/ is a determinism bug. WUBANSIA_WERROR turns them
# into errors, as it does for the plugin's sources.
add_library(wubansia_core_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC") # cl or clang-cl
    target_compile_options(wubansia_core_warnings INTERFACE /W4 $<$<BOOL:${WUBANSIA_WERROR}>:/WX>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    target_compile_options(wubansia_core_warnings INTERFACE
        -Wall -Wshadow-all -Wshorten-64-to-32 -Wstrict-aliasing -Wuninitialized -Wunused-parameter
        -Wconversion -Wsign-compare -Wint-conversion -Wconditional-uninitialized -Wconstant-conversion
        -Wsign-conversion -Wbool-conversion -Wextra-semi -Wunreachable-code -Wcast-align
        -Wshift-sign-overflow -Wmissing-prototypes -Wnullable-to-nonnull-conversion -Wno-ignored-qualifiers
        -Wswitch-enum -Wpedantic -Wdeprecated -Wfloat-equal -Wmissing-field-initializers
        -Wzero-as-null-pointer-constant -Wunused-private-field -Woverloaded-virtual -Wreorder
        -Winconsistent-missing-destructor-override
        $<$<BOOL:${WUBANSIA_WERROR}>:-Werror>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(wubansia_core_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wstrict-aliasing -Wuninitialized -Wunused-parameter -Wsign-compare
        -Wsign-conversion -Wunreachable-code -Wcast-align -Wno-implicit-fallthrough -Wno-maybe-uninitialized
        -Wno-ignored-qualifiers -Wno-multichar -Wswitch-enum -Wredundant-decls -Wno-strict-overflow -Wshadow
        -Wfloat-equal -Wmissing-field-initializers -Woverloaded-virtual -Wreorder -Wzero-as-null-pointer-constant
        -Wfloat-conversion
        $<$<BOOL:${WUBANSIA_WERROR}>:-Werror>)
endif()

file(GLOB_RECURSE WUBANSIA_CORE_SOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" CONFIGURE_DEPENDS "*.cpp" "*.h")
list(FILTER WUBANSIA_CORE_SOURCES EXCLUDE REGEX "^tests/")
list(TRANSFORM WUBANSIA_CORE_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

add_library(wubansia_core STATIC ${WUBANSIA_CORE_SOURCES})
target_include_directories(wubansia_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(wubansia_core PUBLIC cxx_std_20)
# The plugin links core (through wubansia_engine) into its VST3 and CLAP, which are shared objects. On
# Linux, Clang's default -fPIE code reaches inline constexpr tables (gen::sectionKinds...) through
# PC-relative relocations, which lld refuses in a shared object ("recompile with -fPIC") (ADR 0017).
# PIC changes no result: the generator snapshots stay byte-identical.
set_target_properties(wubansia_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
# PUBLIC: code that includes core's headers compiles them with the same floating-point rules (ADR 0003).
target_link_libraries(wubansia_core PUBLIC wubansia_build_defaults PRIVATE wubansia_core_warnings)

if(WUBANSIA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
