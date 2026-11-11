# The engine (ADR 0017): the real-time player the plugin and groovecore render share. Plain C++20 with
# no JUCE; it links only our groove core. engine/tests/rules.cmake keeps JUCE out.
#
# Every .cpp and .h under engine/, except tests/ and testing/, goes into the static library
# wubansia_engine. Add a file and the next build picks it up (CONFIGURE_DEPENDS); a package replaces a
# stub by editing it in place (engine.cpp, mixer_passthrough.cpp, phrase_match.cpp, kit/manifest.cpp).
# Includes are relative to engine/: #include "kit/kit.h". Core's are relative to core/.

file(GLOB_RECURSE WUBANSIA_ENGINE_SOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" CONFIGURE_DEPENDS "*.cpp" "*.h")
list(FILTER WUBANSIA_ENGINE_SOURCES EXCLUDE REGEX "^(tests|testing)/")
list(TRANSFORM WUBANSIA_ENGINE_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

add_library(wubansia_engine STATIC ${WUBANSIA_ENGINE_SOURCES})
target_include_directories(wubansia_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(wubansia_engine PUBLIC cxx_std_20)
# The plugin's VST3 and CLAP are shared objects: on Linux every static library linked into them must be
# position-independent, or lld refuses it ("recompile with -fPIC").
set_target_properties(wubansia_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
# PUBLIC: core's headers (midi/reader.h, gen/json.h, util/result.h) are part of the engine's interface,
# and code that includes either compiles with the same floating-point rules (ADR 0003).
target_link_libraries(wubansia_engine PUBLIC wubansia_core wubansia_build_defaults PRIVATE wubansia_core_warnings)

if(WUBANSIA_BUILD_TESTS)
    # testing/: the synthetic kit, the SMF builder, the manifest writers and the fake host. For
    # engine_tests and the plugin's tests; never linked into the plugin.
    file(GLOB_RECURSE WUBANSIA_ENGINE_TESTING_SOURCES CONFIGURE_DEPENDS "testing/*.cpp" "testing/*.h")
    add_library(wubansia_engine_testing STATIC ${WUBANSIA_ENGINE_TESTING_SOURCES})
    target_link_libraries(wubansia_engine_testing PUBLIC wubansia_engine PRIVATE wubansia_core_warnings)

    add_subdirectory(tests)
endif()
