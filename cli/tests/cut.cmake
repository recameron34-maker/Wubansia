# A --bars cut through groovecore itself (core/gen/cut.h): at an 8-bar line inside a section,
#     groovecore gen --kit <KIT> --seed <N> --bars <BARS>
# must write the head of the whole song, which groovecore_cut_check (cut_check.cpp) checks against
# the whole song's files, in both versions. Then the same cut written into the whole song's folder
# is refused (its shortened region isn't one of the whole song's), and nothing there changes.
#
#   cmake -DGROOVECORE=<exe> -DCHECKER=<groovecore_cut_check> -DCONTENT=<content dir> -DKIT=<id> -DSEED=<n> -DBARS=<n> -DWORK=<scratch dir>
#         [-DAMOUNT=<permille>] [-DEXTREME=ON] [-DCHECK_EXIT=<0 or 3>] [-DCHECK_REGEX=<regex>] -P cut.cmake
#
# AMOUNT is the feel's amount (--feel-amount) for both runs. EXTREME runs on a copy of the content
# at the loader's extremes, made with the edits of test_gen_feel.cpp's extremeBundle: every kit's
# swing 750, every lane's feel swing +250 on top of it, every clamp 60 and every after rule 30 ticks,
# the most the loader takes. CHECK_EXIT is the checker's expected exit code, 0 (the default) for the
# head or 3 for a cut the files can't tell (cut_check.cpp), and CHECK_REGEX, if given, must match
# its output.
#
# WORK is deleted first: it must be a folder only this test uses.

cmake_minimum_required(VERSION 3.25)

foreach(_v GROOVECORE CHECKER CONTENT KIT SEED BARS WORK)
    if("${${_v}}" STREQUAL "")
        message(FATAL_ERROR "cut.cmake: -D${_v}=<value> is missing")
    endif()
endforeach()
if("${CHECK_EXIT}" STREQUAL "")
    set(CHECK_EXIT 0)
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(_content "${CONTENT}")
if(EXTREME)
    set(_content "${WORK}/content-extreme")
    file(COPY "${CONTENT}/kits" "${CONTENT}/styles" "${CONTENT}/feels" DESTINATION "${_content}")
    file(GLOB _kits "${_content}/kits/fut*.json")
    file(GLOB _feels "${_content}/feels/*.json")
    if(NOT _kits OR NOT _feels)
        message(FATAL_ERROR "cut.cmake: no kits or feels under ${CONTENT}")
    endif()
    foreach(_f IN LISTS _kits _feels)
        file(READ "${_f}" _text)
        set(_was "${_text}")
        if(_f IN_LIST _kits)
            string(REGEX REPLACE "\"swing\": -?[0-9]+" "\"swing\": 750" _text "${_text}")
        else()
            string(REGEX REPLACE "\"swing\": \\[[-0-9, ]*\\]" "\"swing\": [250, 250]" _text "${_text}")
            string(REGEX REPLACE "\"clamp\": [0-9]+" "\"clamp\": 60" _text "${_text}")
            string(REGEX REPLACE "\"by\": \\[[-0-9, ]*\\]" "\"by\": [30, 30]" _text "${_text}")
        endif()
        if(_text STREQUAL _was)
            message(FATAL_ERROR "cut.cmake: the extreme edits change nothing in ${_f}")
        endif()
        file(WRITE "${_f}" "${_text}")
    endforeach()
endif()

set(_amount "")
if(NOT "${AMOUNT}" STREQUAL "")
    set(_amount --feel-amount "${AMOUNT}")
endif()
foreach(_run whole cut)
    set(_args gen --kit "${KIT}" --seed "${SEED}" ${_amount} --out "${WORK}/${_run}" --content "${_content}")
    if(_run STREQUAL "cut")
        list(APPEND _args --bars "${BARS}")
    endif()
    execute_process(COMMAND "${GROOVECORE}" ${_args} RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "cut.cmake: groovecore ${_args} failed (${_rc}):\n${_err}")
    endif()
endforeach()

string(REPLACE ";" " " _what "groovecore gen --kit ${KIT} --seed ${SEED} ${_amount} --bars ${BARS}")
if(EXTREME)
    string(APPEND _what " at the feel's extremes")
endif()
execute_process(COMMAND "${CHECKER}" "${WORK}/whole" "${WORK}/cut" "${BARS}" RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL "${CHECK_EXIT}")
    if(_rc EQUAL 1)
        message(FATAL_ERROR "cut.cmake: ${_what} isn't the head of the whole song (exit 1):\n${_err}${_out}")
    elseif(_rc EQUAL 3)
        message(FATAL_ERROR "cut.cmake: the files can't tell whether ${_what} is the head of the whole song (exit 3), so pick a cut they can:\n${_err}${_out}")
    elseif(_rc EQUAL 0)
        message(FATAL_ERROR "cut.cmake: ${_what} was picked as a cut the files can't tell, but the checker could (exit 0, not ${CHECK_EXIT}): the generator changed, so pick another cut for this case:\n${_out}")
    endif()
    message(FATAL_ERROR "cut.cmake: groovecore_cut_check exited ${_rc}, not ${CHECK_EXIT}:\n${_err}${_out}")
endif()
if(NOT "${CHECK_REGEX}" STREQUAL "" AND NOT "${_err}${_out}" MATCHES "${CHECK_REGEX}")
    message(FATAL_ERROR "cut.cmake: the checker's output doesn't match \"${CHECK_REGEX}\":\n${_err}${_out}")
endif()
message(STATUS "${_err}${_out}")

# The cut into the whole song's folder: refused before anything is written, and nothing changes.
file(GLOB_RECURSE _files LIST_DIRECTORIES false RELATIVE "${WORK}/whole" "${WORK}/whole/*")
list(SORT _files)
set(_before "")
foreach(_f IN LISTS _files)
    file(SHA256 "${WORK}/whole/${_f}" _h)
    string(APPEND _before "${_f} ${_h}\n")
endforeach()
execute_process(COMMAND "${GROOVECORE}" gen --kit "${KIT}" --seed "${SEED}" ${_amount} --bars "${BARS}" --out "${WORK}/whole" --content "${_content}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 1 OR NOT _err MATCHES "region file\\(s\\) this song doesn't write")
    message(FATAL_ERROR "cut.cmake: the cut written into the whole song's folder wasn't refused (exit ${_rc}):\n${_err}${_out}")
endif()
file(GLOB_RECURSE _files LIST_DIRECTORIES false RELATIVE "${WORK}/whole" "${WORK}/whole/*")
list(SORT _files)
set(_after "")
foreach(_f IN LISTS _files)
    file(SHA256 "${WORK}/whole/${_f}" _h)
    string(APPEND _after "${_f} ${_h}\n")
endforeach()
if(NOT _after STREQUAL _before)
    message(FATAL_ERROR "cut.cmake: the refused cut changed the whole song's folder")
endif()
message(STATUS "cut.cmake: the cut written into the whole song's folder is refused and changes nothing there")
