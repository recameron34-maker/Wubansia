# engine_tests: every .cpp under engine/tests/ except pic/, one Catch2 binary without JUCE. It has its
# own main (main.cpp) and its own allocation counter (alloc_counter.cpp replaces the global allocation
# functions in this binary only). ctest lists its cases as "engine: <name>" with the label "engine",
# so `ctest -L engine` runs them and the rules below.
#
# The tests can find files through:
#   WUBANSIA_SOURCE_DIR    the repository root
#   WUBANSIA_TEST_TMP_DIR  a scratch folder in the build tree for files a test writes

file(GLOB_RECURSE WUBANSIA_ENGINE_TEST_SOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" CONFIGURE_DEPENDS "*.cpp" "*.h")
list(FILTER WUBANSIA_ENGINE_TEST_SOURCES EXCLUDE REGEX "^pic/")
list(TRANSFORM WUBANSIA_ENGINE_TEST_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

add_executable(engine_tests ${WUBANSIA_ENGINE_TEST_SOURCES})
target_link_libraries(engine_tests PRIVATE wubansia_engine_testing wubansia_engine wubansia_core_warnings Catch2::Catch2)
# Catch2's headers as system headers, so our warning flags (and -Werror) judge only our code.
target_include_directories(engine_tests SYSTEM PRIVATE $<TARGET_PROPERTY:Catch2,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(engine_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(engine_tests PRIVATE
    WUBANSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    WUBANSIA_TEST_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}/tmp")

catch_discover_tests(engine_tests
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "engine: "
    PROPERTIES LABELS engine)

# The grep rules (rules.cmake): nothing under engine/ includes a JUCE header, names the juce namespace,
# or uses its vector or buffer arithmetic (FloatVectorOperations, AudioBuffer's addFrom and applyGain,
# dsp::AudioBlock).
add_test(NAME "engine: rules: engine/ uses no JUCE"
    COMMAND ${CMAKE_COMMAND} "-DROOT=${PROJECT_SOURCE_DIR}/engine" -P "${CMAKE_CURRENT_SOURCE_DIR}/rules.cmake")
set_tests_properties("engine: rules: engine/ uses no JUCE" PROPERTIES LABELS engine)

# Each rule catches what it is for: one folder per rule, holding one offending file, written here in the
# build tree so the scan of engine/ never sees it. The test passes only if the script fails naming it.
# (The fixtures are scanned, never compiled, so they leave out the semicolons CMake would split on.)
set(_rule_cases
    "include_juce|bad.h|#include <juce_audio_basics/juce_audio_basics.h>|includes a JUCE header"
    "include_juceheader|bad.cpp|#include \"JuceHeader.h\"|includes a JUCE header"
    "namespace|bad.cpp|static void f (juce::String) {}|names the juce namespace"
    "fvo|bad.cpp|void f (float* a, const float* b, int n) { FloatVectorOperations::add (a, b, n) }|uses FloatVectorOperations"
    "add_from|bad.h|template <class B> void f (B& b, const B& c) { b.addFrom (0, 0, c, 0, 0, 16) }|uses AudioBuffer's addFrom"
    "apply_gain|bad.cpp|template <class B> void f (B& b) { b.applyGain (0.5f) }|uses AudioBuffer's applyGain"
    "audio_block|bad.inl|using Block = dsp::AudioBlock<float>|uses dsp::AudioBlock")
foreach(_case IN LISTS _rule_cases)
    string(REPLACE "|" ";" _parts "${_case}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _file)
    list(GET _parts 2 _text)
    list(GET _parts 3 _says)
    set(_dir "${CMAKE_CURRENT_BINARY_DIR}/rules-fixtures/${_name}")
    file(REMOVE_RECURSE "${_dir}")
    file(WRITE "${_dir}/fine.cpp" "// a file that breaks no rule\nint fine() { return 0; }\n")
    file(WRITE "${_dir}/${_file}" "// line 1\n${_text}\n")
    add_test(NAME "engine: rules: a file that ${_says} fails (${_name})"
        COMMAND ${CMAKE_COMMAND} "-DROOT=${_dir}" -P "${CMAKE_CURRENT_SOURCE_DIR}/rules.cmake")
    string(REGEX REPLACE "([][+.*()^$])" "\\\\\\1" _says_regex "${_says}")
    set_tests_properties("engine: rules: a file that ${_says} fails (${_name})" PROPERTIES
        LABELS engine
        PASS_REGULAR_EXPRESSION "${_file}: ${_says_regex}")
endforeach()

# The plugin's VST3 and CLAP are shared objects that link wubansia_engine and, through it,
# wubansia_core. engine_pic_check is one too, so on Linux the build fails here, with lld's "recompile
# with -fPIC", if either library loses POSITION_INDEPENDENT_CODE. It links both archives whole: a
# non-PIC object only fails once something pulls it in (one that holds an inline constexpr table, such
# as gen::sectionKinds, or instantiates std::piecewise_construct), so the plugin could otherwise pass
# today and fail when a package first calls into such an object.
add_library(engine_pic_check MODULE pic/pic_check.cpp)
target_link_libraries(engine_pic_check PRIVATE wubansia_engine wubansia_core_warnings)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(engine_pic_check PRIVATE
        "LINKER:--whole-archive" "$<TARGET_FILE:wubansia_engine>" "$<TARGET_FILE:wubansia_core>" "LINKER:--no-whole-archive")
endif()
