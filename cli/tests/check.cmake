# groovecore metrics --check: the expected file and the result must hold the same sections, so a
# section the metrics stop computing, or one the expected file has and the code doesn't, fails the
# check instead of going uncompared. Only the expected file's header (the Node, manifest and core.js
# it was made with, as core/tests/legacy_expected.json has) and the result's "about" and "format" are
# left out.
#
#   cmake -DGROOVECORE=<exe> -DFIXTURE=<a .mid file> -DWORK=<scratch dir> -P check.cmake
#
# From the fixture's own --json result it writes four expected files and checks groovecore's verdict
# on each: the exact result and the result with a header pass; one with an extra section and one
# missing a section fail, naming the section.

cmake_minimum_required(VERSION 3.25)

foreach(_v GROOVECORE FIXTURE WORK)
    if("${${_v}}" STREQUAL "")
        message(FATAL_ERROR "check.cmake: -D${_v}=<path> is missing")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

execute_process(COMMAND "${GROOVECORE}" metrics --json "${FIXTURE}" RESULT_VARIABLE _rc OUTPUT_VARIABLE _json ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "check.cmake: groovecore metrics --json ${FIXTURE} exited ${_rc}:\n${_err}")
endif()

set(_header "${_json}")
foreach(_k node manifest manifest_sha256 manifest_node core_js_sha256 about)
    string(JSON _header SET "${_header}" ${_k} "\"from the header\"")
endforeach()
string(JSON _header SET "${_header}" format "1")
string(JSON _extra SET "${_json}" reported_v2 "{ \"x\": 1 }")
string(JSON _missing REMOVE "${_json}" invariants)

file(WRITE "${WORK}/exact.json" "${_json}")
file(WRITE "${WORK}/header.json" "${_header}")
file(WRITE "${WORK}/extra.json" "${_extra}")
file(WRITE "${WORK}/missing.json" "${_missing}")

set(_problems "")
# name, the exit code wanted, a regex the output (both streams) must match
foreach(_case "exact;0;check: all [0-9]+ values match" "header;0;check: all [0-9]+ values match" "extra;1;reported_v2: missing"
        "missing;1;invariants: not (in the )?expected")
    list(GET _case 0 _name)
    list(GET _case 1 _want)
    list(GET _case 2 _regex)
    execute_process(COMMAND "${GROOVECORE}" metrics --check "${WORK}/${_name}.json" "${FIXTURE}" RESULT_VARIABLE _code OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _code STREQUAL _want)
        string(APPEND _problems "${_name}: exit code ${_code}, expected ${_want}\n--- stdout ---\n${_out}--- stderr ---\n${_err}\n")
    elseif(NOT "${_out}${_err}" MATCHES "${_regex}")
        string(APPEND _problems "${_name}: the output doesn't match '${_regex}'\n--- stdout ---\n${_out}--- stderr ---\n${_err}\n")
    endif()
endforeach()
if(_problems)
    message(FATAL_ERROR "check.cmake: groovecore metrics --check gave the wrong verdict:\n${_problems}")
endif()
message(STATUS "check.cmake: --check passes the exact result and the header, and fails an extra and a missing section")
