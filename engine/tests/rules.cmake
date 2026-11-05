# The engine's grep rules (ADR 0017, plan §3.5): engine/ is plain C++ with no JUCE, and its mixing and
# gain are plain scalar loops, so renders don't depend on Accelerate or JUCE's SIMD paths.
#
#     cmake -DROOT=<folder> -P engine/tests/rules.cmake
#
# Scans every C and C++ source under ROOT (.h .hpp .inl .c .cc .cpp .mm) and fails, naming each file and
# rule, if one:
# - includes a JUCE header (juce_*, JuceHeader.h);
# - names the juce namespace;
# - uses FloatVectorOperations, AudioBuffer's addFrom or applyGain, or dsp::AudioBlock.
# Comments count too: the words simply don't appear under engine/. engine/tests/CMakeLists.txt runs it
# on engine/ and on one offending file per rule.

if(NOT DEFINED ROOT OR NOT IS_DIRECTORY "${ROOT}")
    message(FATAL_ERROR "engine/rules: pass -DROOT=<folder to scan>; got \"${ROOT}\"")
endif()

file(GLOB_RECURSE _files LIST_DIRECTORIES false
    "${ROOT}/*.h" "${ROOT}/*.hpp" "${ROOT}/*.inl" "${ROOT}/*.c" "${ROOT}/*.cc" "${ROOT}/*.cpp" "${ROOT}/*.mm")
list(LENGTH _files _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "engine/rules: no C or C++ sources under ${ROOT}")
endif()

# Pairs of (regex, what it means). CMake regexes have no line anchors, so each one matches anywhere.
set(_rules
    "#[ \t]*include[ \t]*[<\"](juce_[a-z_]+/|JuceHeader)" "includes a JUCE header"
    "juce::" "names the juce namespace"
    "FloatVectorOperations" "uses FloatVectorOperations"
    "addFrom" "uses AudioBuffer's addFrom"
    "applyGain" "uses AudioBuffer's applyGain"
    "AudioBlock" "uses dsp::AudioBlock")
list(LENGTH _rules _rule_items)
math(EXPR _last "${_rule_items} - 1")

set(_problems "")
foreach(_file IN LISTS _files)
    file(READ "${_file}" _text)
    foreach(_i RANGE 0 ${_last} 2)
        math(EXPR _j "${_i} + 1")
        list(GET _rules ${_i} _regex)
        list(GET _rules ${_j} _says)
        if(_text MATCHES "${_regex}")
            file(RELATIVE_PATH _rel "${ROOT}" "${_file}")
            string(APPEND _problems "\n  ${_rel}: ${_says} (\"${CMAKE_MATCH_0}\")")
        endif()
    endforeach()
endforeach()

if(_problems)
    message(FATAL_ERROR "engine/rules: these files break the engine's rules (ADR 0017):${_problems}")
endif()
message(STATUS "engine/rules: ${_count} files under ${ROOT} keep the rules")
