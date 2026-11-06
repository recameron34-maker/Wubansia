# golden_check: the legacy MIDI baseline (docs/plans/S1-futuristic-slice.md section 4.3, decision D2
# option A; ADR 0013).
#
# tools/export_golden.js runs frozen legacy/web-pack/core.js under the Node version in .nvmrc and writes
# the 516 legacy MIDI files (43 kits x 12) plus their manifest. content/golden/legacy-v21.sha256 is the
# committed manifest. Never edit or regenerate it to make this check pass (CLAUDE.md, "Determinism and
# golden files").
#
# This file works two ways.
#
# 1. include(WubansiaGolden) in the root CMakeLists.txt, anywhere after project(). At the end of the root
#    directory (cmake_language(DEFER), which is why the include can sit anywhere) it defines:
#    - golden_check, a custom target outside "all":  cmake --build --preset release --target golden_check
#        a. node tools/export_golden.js --out ${WUBANSIA_GOLDEN_DIR}
#        b. this file as a script (2. below), which checks the export against the committed manifest
#        c. the steps added with wubansia_golden_check_add_step(), in the order they were added
#    - two ctest tests, when WUBANSIA_BUILD_TESTS is on, each in temporary folders and skipped, with the
#      reason, without the Node version in .nvmrc (label golden):
#        golden.export_golden  node --test on tools/export_golden/export_golden.test.js: the exporter's
#                              and this checker's own tests
#        golden.legacy_audit   node --test on tools/legacy_audit/test/audit.test.js: the audit's fixtures
#                              and its result on a fresh export, against core/tests/legacy_expected.json
#    Cache variables:
#      WUBANSIA_NODE_EXECUTABLE  the Node to use. Empty (the default) means node on PATH, looked up again
#                                at every configure, so a configure after "nvm install" finds it.
#      WUBANSIA_GOLDEN_DIR       where golden_check exports: <repo>/build/golden, the exporter's default.
#
# 2. cmake -DGOLDEN_DIR=<dir> -DMANIFEST=<committed manifest> [-DFRESH_MANIFEST=<the export's manifest>]
#          -DCORE_JS=<core.js> -DNVMRC=<.nvmrc> -P cmake/WubansiaGolden.cmake
#    The checker on its own (golden_check runs it with the repo's files); it hashes the files itself and
#    shares no code with the exporter. Relative paths are from the folder cmake runs in. It lists every
#    problem and fails when:
#    - the manifest isn't LF-only printable ASCII made of "# ..." header lines and then one
#      "<sha256>  <KIT>/<file>.mid" line per file, sorted by path in byte order, each path once
#    - its header doesn't name the Node version in .nvmrc, the SHA-256 of core.js and its own file count
#    - a listed file is missing or has another SHA-256, or <dir> holds a .mid file the manifest doesn't list
#    - FRESH_MANIFEST, when given, differs from the manifest in any byte
#
# The metrics step: golden_check also runs the generator metrics on the legacy files and asserts that
# they FAIL the gating bands (plan sections 4.3 and 5.1). cli/CMakeLists.txt adds it, after the include
# (the function exists from the include on):
#     wubansia_golden_check_add_step(NAME metrics DEPENDS groovecore
#         COMMAND groovecore metrics --legacy --expect-fail --check core/tests/legacy_expected.json <export>)
# Without a "metrics" step (a tree without cli/), golden_check ends with a note saying it isn't wired.

if(CMAKE_SCRIPT_MODE_FILE)
    file(REAL_PATH "${CMAKE_SCRIPT_MODE_FILE}" _wubansia_golden_script)
    file(REAL_PATH "${CMAKE_CURRENT_LIST_FILE}" _wubansia_golden_self)
    if(NOT _wubansia_golden_script STREQUAL _wubansia_golden_self)
        return() # included from another script: there is nothing to define, and the checker isn't asked for
    endif()
else()
    # ---------------------------------------------------------------------------------------------
    # 1. Included from a CMakeLists.txt: golden_check and the test.
    # ---------------------------------------------------------------------------------------------
    include_guard(GLOBAL)

    get_filename_component(_wubansia_golden_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    set_property(GLOBAL PROPERTY WUBANSIA_GOLDEN_ROOT "${_wubansia_golden_root}")
    set_property(GLOBAL PROPERTY WUBANSIA_GOLDEN_SELF "${CMAKE_CURRENT_LIST_FILE}")
    set_property(GLOBAL PROPERTY WUBANSIA_GOLDEN_STEP_COUNT 0)

    set(WUBANSIA_NODE_EXECUTABLE "" CACHE FILEPATH
        "Node for golden_check and its test; empty: node on PATH. It must be the version in .nvmrc.")
    set(WUBANSIA_GOLDEN_DIR "${_wubansia_golden_root}/build/golden" CACHE PATH
        "Where golden_check exports the legacy MIDI baseline")

    # Editing .nvmrc reconfigures, so the Node check below runs again.
    if(EXISTS "${_wubansia_golden_root}/.nvmrc")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_wubansia_golden_root}/.nvmrc")
    endif()

    # wubansia_golden_check_add_step(NAME <name> COMMAND <command> [<arg>...] [DEPENDS <target>...])
    # Adds a step that golden_check runs after the manifest check, in the repo root, if the check passed.
    # COMMAND takes generator expressions, and an executable target's name stands for its file.
    # DEPENDS names targets to build first. No argument may contain a semicolon.
    function(wubansia_golden_check_add_step)
        cmake_parse_arguments(PARSE_ARGV 0 arg "" "NAME" "COMMAND;DEPENDS")
        if(arg_UNPARSED_ARGUMENTS OR arg_KEYWORDS_MISSING_VALUES OR "${arg_NAME}" STREQUAL "" OR NOT arg_COMMAND)
            message(FATAL_ERROR "Usage: wubansia_golden_check_add_step(NAME <name> COMMAND <command> [<arg>...] "
                "[DEPENDS <target>...])")
        endif()
        get_property(defined GLOBAL PROPERTY WUBANSIA_GOLDEN_DEFINED)
        if(defined)
            message(FATAL_ERROR "wubansia_golden_check_add_step(NAME ${arg_NAME}): golden_check is already defined. "
                "Add steps before the end of the root CMakeLists.txt.")
        endif()
        get_property(n GLOBAL PROPERTY WUBANSIA_GOLDEN_STEP_COUNT)
        set_property(GLOBAL PROPERTY WUBANSIA_GOLDEN_STEP_${n}_NAME "${arg_NAME}")
        set_property(GLOBAL PROPERTY WUBANSIA_GOLDEN_STEP_${n}_COMMAND "${arg_COMMAND}")
        set_property(GLOBAL PROPERTY WUBANSIA_GOLDEN_STEP_${n}_DEPENDS "${arg_DEPENDS}")
        math(EXPR n "${n} + 1")
        set_property(GLOBAL PROPERTY WUBANSIA_GOLDEN_STEP_COUNT ${n})
    endfunction()

    function(_wubansia_golden_define)
        set_property(GLOBAL PROPERTY WUBANSIA_GOLDEN_DEFINED TRUE)
        get_property(root GLOBAL PROPERTY WUBANSIA_GOLDEN_ROOT)
        get_property(self GLOBAL PROPERTY WUBANSIA_GOLDEN_SELF)
        set(nvmrc "${root}/.nvmrc")
        set(out "${WUBANSIA_GOLDEN_DIR}")

        # The version .nvmrc pins. The exporter enforces it on every run; this only reports early.
        set(want "")
        if(EXISTS "${nvmrc}")
            file(READ "${nvmrc}" want)
            string(STRIP "${want}" want)
            string(REGEX REPLACE "^v" "" want "${want}")
        endif()

        # Node: the cache variable if set, else node on PATH (not cached: every configure looks again).
        unset(_wubansia_node)
        if(NOT "${WUBANSIA_NODE_EXECUTABLE}" STREQUAL "")
            if(IS_ABSOLUTE "${WUBANSIA_NODE_EXECUTABLE}")
                if(EXISTS "${WUBANSIA_NODE_EXECUTABLE}" AND NOT IS_DIRECTORY "${WUBANSIA_NODE_EXECUTABLE}")
                    set(_wubansia_node "${WUBANSIA_NODE_EXECUTABLE}")
                endif()
            else()
                find_program(_wubansia_node NAMES "${WUBANSIA_NODE_EXECUTABLE}" NO_CACHE)
            endif()
            set(where "WUBANSIA_NODE_EXECUTABLE=${WUBANSIA_NODE_EXECUTABLE}")
        else()
            find_program(_wubansia_node NAMES node nodejs NO_CACHE)
            set(where "node on PATH")
        endif()
        set(node "${_wubansia_node}")
        set(have "")
        if(node)
            execute_process(COMMAND "${node}" --version RESULT_VARIABLE rc OUTPUT_VARIABLE have
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE TIMEOUT 60)
            if(NOT rc EQUAL 0)
                set(have "")
            endif()
            string(REGEX REPLACE "^v" "" have "${have}")
        endif()

        set(problem "")
        if(NOT want MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
            set(problem "${nvmrc} must hold one exact Node version, like 22.22.0")
        elseif(NOT node)
            set(problem "no Node found (${where}), and .nvmrc pins ${want}")
        elseif(have STREQUAL "")
            set(problem "${node} --version failed (${where}), and .nvmrc pins ${want}")
        elseif(NOT have STREQUAL want)
            set(problem "Node v${have} (${node}), but .nvmrc pins ${want}")
        endif()
        if(problem)
            message(STATUS "golden_check: ${problem}. golden_check will refuse to run, and the tests "
                "golden.export_golden and golden.legacy_audit are skipped. Install Node ${want} (nvm install, "
                "from the repo root) and configure again, or pass -DWUBANSIA_NODE_EXECUTABLE=<path to it>.")
        else()
            message(STATUS "golden_check: Node v${have} (${node}), the version in .nvmrc")
        endif()

        # The target. Without a Node it can only say so; with the wrong one, the exporter refuses and says why.
        if(node)
            set(commands
                COMMAND "${node}" "${root}/tools/export_golden.js" --out "${out}"
                COMMAND "${CMAKE_COMMAND}" "-DGOLDEN_DIR=${out}"
                    "-DMANIFEST=${root}/content/golden/legacy-v21.sha256"
                    "-DFRESH_MANIFEST=${out}/legacy-v21.sha256"
                    "-DCORE_JS=${root}/legacy/web-pack/core.js" "-DNVMRC=${nvmrc}" -P "${self}")
        else()
            string(CONCAT why "golden_check: ${problem}. Install Node ${want} (nvm install) and configure again, "
                "or pass -DWUBANSIA_NODE_EXECUTABLE=<path to it>.")
            set(commands COMMAND "${CMAKE_COMMAND}" -E echo "${why}" COMMAND "${CMAKE_COMMAND}" -E false)
        endif()

        get_property(count GLOBAL PROPERTY WUBANSIA_GOLDEN_STEP_COUNT)
        set(depends "")
        set(metrics FALSE)
        if(count GREATER 0)
            math(EXPR last "${count} - 1")
            foreach(i RANGE ${last})
                get_property(name GLOBAL PROPERTY WUBANSIA_GOLDEN_STEP_${i}_NAME)
                get_property(cmd GLOBAL PROPERTY WUBANSIA_GOLDEN_STEP_${i}_COMMAND)
                get_property(deps GLOBAL PROPERTY WUBANSIA_GOLDEN_STEP_${i}_DEPENDS)
                list(APPEND commands COMMAND "${CMAKE_COMMAND}" -E echo "golden_check: step ${name}" COMMAND ${cmd})
                list(APPEND depends ${deps})
                if(name STREQUAL "metrics")
                    set(metrics TRUE)
                endif()
            endforeach()
        endif()
        if(NOT metrics)
            string(CONCAT note "golden_check: note: the metrics step (groovecore metrics, asserting that the legacy "
                "files fail the gating bands) is not wired yet. See the hook in cmake/WubansiaGolden.cmake.")
            list(APPEND commands COMMAND "${CMAKE_COMMAND}" -E echo "${note}")
        endif()

        add_custom_target(golden_check ${commands}
            WORKING_DIRECTORY "${root}"
            COMMENT "golden_check: legacy MIDI baseline from frozen core.js against content/golden/legacy-v21.sha256"
            VERBATIM USES_TERMINAL)
        if(depends)
            list(REMOVE_DUPLICATES depends)
            add_dependencies(golden_check ${depends})
        endif()

        if(WUBANSIA_BUILD_TESTS)
            if(problem)
                add_test(NAME golden.export_golden COMMAND "${CMAKE_COMMAND}" -E echo "SKIPPED: ${problem}")
                set_tests_properties(golden.export_golden PROPERTIES SKIP_REGULAR_EXPRESSION "SKIPPED:" LABELS golden)
            else()
                add_test(NAME golden.export_golden
                    COMMAND "${node}" --test --test-reporter=spec "${root}/tools/export_golden/export_golden.test.js"
                    WORKING_DIRECTORY "${root}")
                set_tests_properties(golden.export_golden PROPERTIES
                    ENVIRONMENT "WUBANSIA_CMAKE=${CMAKE_COMMAND};WUBANSIA_CTEST=${CMAKE_CTEST_COMMAND}"
                    TIMEOUT 600 LABELS golden)
            endif()
            # The legacy audit (tools/legacy_audit), whose output on the baseline is legacy_expected.json.
            # Its fixture tests would run on any Node, but its baseline test needs the pinned one: one rule
            # for both tests keeps the skip honest.
            if(EXISTS "${root}/tools/legacy_audit/test/audit.test.js")
                if(problem)
                    add_test(NAME golden.legacy_audit COMMAND "${CMAKE_COMMAND}" -E echo "SKIPPED: ${problem}")
                    set_tests_properties(golden.legacy_audit PROPERTIES SKIP_REGULAR_EXPRESSION "SKIPPED:" LABELS golden)
                else()
                    add_test(NAME golden.legacy_audit
                        COMMAND "${node}" --test --test-reporter=spec "${root}/tools/legacy_audit/test/audit.test.js"
                        WORKING_DIRECTORY "${root}")
                    set_tests_properties(golden.legacy_audit PROPERTIES TIMEOUT 600 LABELS golden)
                endif()
            endif()
        endif()
    endfunction()

    cmake_language(DEFER CALL _wubansia_golden_define)
    return()
endif()

# -------------------------------------------------------------------------------------------------
# 2. cmake -P: the checker.
# -------------------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.25)

set(_usage "Usage: cmake -DGOLDEN_DIR=<dir> -DMANIFEST=<committed manifest> [-DFRESH_MANIFEST=<the export's manifest>] "
    "-DCORE_JS=<core.js> -DNVMRC=<.nvmrc> -P cmake/WubansiaGolden.cmake")
string(JOIN "" _usage ${_usage})
set(_advice
    "What to do:\n"
    "  - Never edit or regenerate content/golden/legacy-v21.sha256 to make this pass (CLAUDE.md).\n"
    "  - A file differs or is missing, or the header disagrees: something changed legacy/web-pack/core.js,\n"
    "    tools/export_golden.js or the Node version. Undo that change. If the change is deliberate and you\n"
    "    were asked to re-baseline, do it in its own commit with an ADR note:\n"
    "      cp build/golden/legacy-v21.sha256 content/golden/\n"
    "  - A .mid file it doesn't list, left by an older export: delete the export folder and run golden_check again.\n")
string(JOIN "" _advice ${_advice})

foreach(_v GOLDEN_DIR MANIFEST CORE_JS NVMRC)
    if("${${_v}}" STREQUAL "")
        message(FATAL_ERROR "golden_check: -D${_v}=<path> is missing.\n${_usage}")
    endif()
endforeach()
# A relative path is from the folder cmake runs in (CMAKE_CURRENT_SOURCE_DIR, in script mode), made
# absolute once, here: file(GLOB_RECURSE ... RELATIVE) below finds nothing under a relative folder,
# which would skip the unlisted-file check without a word.
foreach(_v GOLDEN_DIR MANIFEST FRESH_MANIFEST CORE_JS NVMRC)
    if(NOT "${${_v}}" STREQUAL "")
        get_filename_component(${_v} "${${_v}}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endforeach()

# Stops at once: problems with the manifest's own format make every other check meaningless.
# The arguments are joined; none may contain a semicolon.
function(_wubansia_golden_bad_manifest)
    string(JOIN "" why ${ARGN})
    message(NOTICE "golden_check: FAILED: the manifest ${MANIFEST} ${why}\n\n${_advice}")
    message(FATAL_ERROR "golden_check: FAILED (see above)")
endfunction()

# Up to 20 paths, one per line, then a count of the rest.
function(_wubansia_golden_paths out)
    list(LENGTH ARGN n)
    set(text "")
    set(i 0)
    foreach(p IN LISTS ARGN)
        if(i EQUAL 20)
            math(EXPR rest "${n} - 20")
            string(APPEND text "\n      ... and ${rest} more")
            break()
        endif()
        string(APPEND text "\n      ${p}")
        math(EXPR i "${i} + 1")
    endforeach()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# --- The manifest's format ---
if(NOT EXISTS "${MANIFEST}" OR IS_DIRECTORY "${MANIFEST}")
    _wubansia_golden_bad_manifest("does not exist.")
endif()
# Byte by byte first: file(READ) as text would hide some bytes (it drops the CR of each CRLF, and a NUL
# cuts the text short), so the manifest is read as hex, and every byte must be LF or printable ASCII.
file(READ "${MANIFEST}" _hex HEX)
string(REGEX MATCHALL "[0-9a-f][0-9a-f]" _bytes "${_hex}")
list(FILTER _bytes EXCLUDE REGEX "^(0a|[2-6][0-9a-f]|7[0-9a-e])$")
if(_bytes)
    list(REMOVE_DUPLICATES _bytes)
    list(TRANSFORM _bytes REPLACE "^0d$" "0d (CR: CRLF line endings?)")
    list(TRANSFORM _bytes REPLACE "^00$" "00 (NUL)")
    list(TRANSFORM _bytes REPLACE "^09$" "09 (tab)")
    list(JOIN _bytes ", " _bytes)
    _wubansia_golden_bad_manifest("has bytes other than printable ASCII and LF: ${_bytes}. It must be LF-only "
        "text: it is compared byte for byte, and .gitattributes keeps content/golden/** as -text.")
endif()
file(READ "${MANIFEST}" _text)
foreach(_c ";" "[" "]" "\\")
    string(FIND "${_text}" "${_c}" _at)
    if(NOT _at EQUAL -1)
        set(_name "${_c}")
        if(_c STREQUAL ";")
            set(_name "semicolon")
        elseif(_c STREQUAL "\\")
            set(_name "backslash")
        endif()
        _wubansia_golden_bad_manifest("has a ${_name}, which the checker can't read (CMake lists). The exporter never writes one.")
    endif()
endforeach()
string(LENGTH "${_text}" _len)
if(_len EQUAL 0)
    _wubansia_golden_bad_manifest("is empty.")
endif()
math(EXPR _at "${_len} - 1")
string(SUBSTRING "${_text}" ${_at} 1 _tail)
if(NOT _tail STREQUAL "\n")
    _wubansia_golden_bad_manifest("does not end with a newline.")
endif()

string(REPLACE "\n" ";" _lines "${_text}")
list(POP_BACK _lines) # the empty string after the last newline
set(_lineno 0)
set(_in_header TRUE)
set(_paths "")
set(_hashes "")
set(_prev "")
foreach(_line IN LISTS _lines)
    math(EXPR _lineno "${_lineno} + 1")
    if(_line MATCHES "^#")
        if(NOT _in_header)
            _wubansia_golden_bad_manifest("has a header line after the entries, line ${_lineno}: '${_line}'.")
        endif()
        if(_line MATCHES "^# node ")
            if(DEFINED _h_node)
                _wubansia_golden_bad_manifest("has a second '# node' header line, line ${_lineno}.")
            endif()
            if(NOT _line MATCHES "^# node v([0-9]+\\.[0-9]+\\.[0-9]+)$")
                _wubansia_golden_bad_manifest("line ${_lineno} is not '# node v<exact version>': '${_line}'.")
            endif()
            set(_h_node "${CMAKE_MATCH_1}")
        elseif(_line MATCHES "^# core\\.js ")
            if(DEFINED _h_core)
                _wubansia_golden_bad_manifest("has a second '# core.js' header line, line ${_lineno}.")
            endif()
            if(NOT _line MATCHES "^# core\\.js sha256 ([0-9a-f]+)$")
                _wubansia_golden_bad_manifest("line ${_lineno} is not '# core.js sha256 <sha256>': '${_line}'.")
            endif()
            set(_h_core "${CMAKE_MATCH_1}")
            string(LENGTH "${_h_core}" _n)
            if(NOT _n EQUAL 64)
                _wubansia_golden_bad_manifest("line ${_lineno} is not '# core.js sha256 <sha256>': '${_line}'.")
            endif()
        elseif(_line MATCHES "^# files ")
            if(DEFINED _h_files)
                _wubansia_golden_bad_manifest("has a second '# files' header line, line ${_lineno}.")
            endif()
            if(NOT _line MATCHES "^# files ([0-9]+)$")
                _wubansia_golden_bad_manifest("line ${_lineno} is not '# files <count>': '${_line}'.")
            endif()
            set(_h_files "${CMAKE_MATCH_1}")
        endif()
        continue()
    endif()
    set(_in_header FALSE)
    if(NOT _line MATCHES "^([0-9a-f]+)  ([A-Za-z0-9_-]+/[A-Za-z0-9_-]+\\.mid)$")
        _wubansia_golden_bad_manifest("line ${_lineno} is not '<sha256>  <KIT>/<file>.mid': '${_line}'.")
    endif()
    set(_hash "${CMAKE_MATCH_1}")
    set(_path "${CMAKE_MATCH_2}")
    string(LENGTH "${_hash}" _n)
    if(NOT _n EQUAL 64)
        _wubansia_golden_bad_manifest("line ${_lineno} is not '<sha256>  <KIT>/<file>.mid': '${_line}'.")
    endif()
    if(NOT _prev STREQUAL "" AND NOT _prev STRLESS _path)
        _wubansia_golden_bad_manifest("is out of order at line ${_lineno}: ${_path} comes after ${_prev}. "
            "Entries are sorted by path in byte order, each path once.")
    endif()
    list(APPEND _paths "${_path}")
    list(APPEND _hashes "${_hash}")
    set(_prev "${_path}")
endforeach()
list(LENGTH _paths _count)
if(_count EQUAL 0)
    _wubansia_golden_bad_manifest("lists no files.")
endif()
if(NOT DEFINED _h_node)
    _wubansia_golden_bad_manifest("has no '# node v<version>' header line.")
endif()
if(NOT DEFINED _h_core)
    _wubansia_golden_bad_manifest("has no '# core.js sha256 <sha256>' header line.")
endif()
if(NOT DEFINED _h_files)
    _wubansia_golden_bad_manifest("has no '# files <count>' header line.")
endif()
if(NOT _h_files EQUAL _count)
    _wubansia_golden_bad_manifest("says '# files ${_h_files}' in its header but lists ${_count} files.")
endif()

# --- Everything else: every problem is collected, then reported together ---
set(_problems 0)
set(_report "")
# The arguments are joined; none may contain a semicolon.
function(_wubansia_golden_problem)
    math(EXPR n "${_problems} + 1")
    string(JOIN "" msg ${ARGN})
    set(_problems ${n} PARENT_SCOPE)
    set(_report "${_report}\n  ${n}. ${msg}" PARENT_SCOPE)
endfunction()

# The Node version and core.js the manifest was made with.
set(_nvmrc "")
if(EXISTS "${NVMRC}" AND NOT IS_DIRECTORY "${NVMRC}")
    file(READ "${NVMRC}" _nvmrc)
endif()
string(STRIP "${_nvmrc}" _nvmrc)
string(REGEX REPLACE "^v" "" _nvmrc "${_nvmrc}")
if(NOT _nvmrc MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    _wubansia_golden_problem("${NVMRC} must hold one exact Node version, like ${_h_node}.")
elseif(NOT _nvmrc STREQUAL _h_node)
    _wubansia_golden_problem(".nvmrc pins Node ${_nvmrc}, but the manifest was made with Node v${_h_node}. Set .nvmrc "
        "back to ${_h_node}, or, if you were asked to move Node, re-baseline with Node ${_nvmrc} in its own commit with an ADR note.")
endif()
if(NOT EXISTS "${CORE_JS}" OR IS_DIRECTORY "${CORE_JS}")
    _wubansia_golden_problem("core.js is missing: ${CORE_JS}")
else()
    file(SHA256 "${CORE_JS}" _core)
    if(NOT _core STREQUAL _h_core)
        _wubansia_golden_problem("core.js (${CORE_JS}) has SHA-256 ${_core}, but the manifest was made from core.js "
            "${_h_core}. core.js is frozen (CLAUDE.md): undo the change.")
    endif()
endif()

# The files themselves.
if(NOT IS_DIRECTORY "${GOLDEN_DIR}")
    _wubansia_golden_problem("there is no export folder ${GOLDEN_DIR}. Export first: node tools/export_golden.js "
        "--out <that folder> (golden_check does this itself).")
else()
    set(_missing "")
    set(_changed "")
    foreach(_path _hash IN ZIP_LISTS _paths _hashes)
        if(NOT EXISTS "${GOLDEN_DIR}/${_path}" OR IS_DIRECTORY "${GOLDEN_DIR}/${_path}")
            list(APPEND _missing "${_path}")
        else()
            file(SHA256 "${GOLDEN_DIR}/${_path}" _got)
            if(NOT _got STREQUAL _hash)
                list(APPEND _changed "${_path}  (sha256 ${_got}, manifest ${_hash})")
            endif()
        endif()
    endforeach()
    file(GLOB_RECURSE _found LIST_DIRECTORIES false RELATIVE "${GOLDEN_DIR}" "${GOLDEN_DIR}/*.mid")
    list(REMOVE_ITEM _found ${_paths})
    foreach(_kind missing changed found)
        list(LENGTH _${_kind} _n)
        if(_n GREATER 0)
            _wubansia_golden_paths(_list ${_${_kind}})
            if(_kind STREQUAL "missing")
                _wubansia_golden_problem("${_n} of the ${_count} files are missing from ${GOLDEN_DIR}:${_list}")
            elseif(_kind STREQUAL "changed")
                _wubansia_golden_problem("${_n} of the ${_count} files differ from the manifest:${_list}")
            else()
                _wubansia_golden_problem("${_n} MIDI files in ${GOLDEN_DIR} are not in the manifest:${_list}")
            endif()
        endif()
    endforeach()
endif()

# The export's own manifest: byte for byte the committed one.
if(NOT "${FRESH_MANIFEST}" STREQUAL "")
    if(NOT EXISTS "${FRESH_MANIFEST}" OR IS_DIRECTORY "${FRESH_MANIFEST}")
        _wubansia_golden_problem("the export wrote no manifest: ${FRESH_MANIFEST} does not exist.")
    else()
        file(SHA256 "${FRESH_MANIFEST}" _fresh)
        file(SHA256 "${MANIFEST}" _committed)
        if(NOT _fresh STREQUAL _committed)
            _wubansia_golden_problem("the export's own manifest differs from the committed one. See how: "
                "diff -u ${MANIFEST} ${FRESH_MANIFEST}")
        endif()
    endif()
endif()

if(_problems GREATER 0)
    message(NOTICE "golden_check: FAILED, ${_problems} problem(s) with the legacy baseline in ${GOLDEN_DIR} "
        "against ${MANIFEST}:${_report}\n\n${_advice}")
    message(FATAL_ERROR "golden_check: FAILED (see above)")
endif()
message(STATUS "golden_check: ${_count} files match ${MANIFEST} (Node v${_h_node}, core.js sha256 ${_h_core})")
