# The generator snapshots through groovecore itself (core/tests/snapshots/gen.sha256; the in-memory
# check is core/tests/test_gen_snapshots.cpp). For every kit and seed the manifest lists, it runs
#     groovecore gen --kit <KIT> --seed <N> --out <WORK>/<KIT>/seed<N> --content <CONTENT>
# and fails unless groovecore writes exactly the files the manifest lists, each with its SHA-256,
# which CMake computes on its own.
#
#   cmake -DGROOVECORE=<exe> -DCONTENT=<content dir> -DMANIFEST=<gen.sha256> -DWORK=<scratch dir> -P snapshots.cmake
#
# WORK is deleted first: it must be a folder only this test uses.

cmake_minimum_required(VERSION 3.25)

foreach(_v GROOVECORE CONTENT MANIFEST WORK)
    if("${${_v}}" STREQUAL "")
        message(FATAL_ERROR "snapshots.cmake: -D${_v}=<path> is missing")
    endif()
endforeach()
if(NOT EXISTS "${MANIFEST}")
    message(FATAL_ERROR "snapshots.cmake: no manifest at ${MANIFEST}")
endif()

# The manifest: "# ..." header lines, then "<sha256>  <KIT>/seed<N>/<path>" lines.
file(STRINGS "${MANIFEST}" _lines)
set(_paths "")
set(_hashes "")
set(_runs "")
foreach(_line IN LISTS _lines)
    if(_line MATCHES "^#")
        continue()
    endif()
    if(NOT _line MATCHES "^([0-9a-f]+)  (([A-Z0-9]+)/seed([0-9]+)/[A-Za-z0-9_./-]+)$")
        message(FATAL_ERROR "snapshots.cmake: ${MANIFEST}: not '<sha256>  <KIT>/seed<N>/<path>': '${_line}'")
    endif()
    list(APPEND _hashes "${CMAKE_MATCH_1}")
    list(APPEND _paths "${CMAKE_MATCH_2}")
    list(APPEND _runs "${CMAKE_MATCH_3}/${CMAKE_MATCH_4}")
endforeach()
list(LENGTH _paths _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "snapshots.cmake: ${MANIFEST} lists no files")
endif()
list(REMOVE_DUPLICATES _runs)

# Generate.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(_run IN LISTS _runs)
    string(REPLACE "/" ";" _parts "${_run}")
    list(GET _parts 0 _kit)
    list(GET _parts 1 _seed)
    execute_process(COMMAND "${GROOVECORE}" gen --kit "${_kit}" --seed "${_seed}" --out "${WORK}/${_kit}/seed${_seed}" --content "${CONTENT}"
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "snapshots.cmake: groovecore gen --kit ${_kit} --seed ${_seed} failed (${_rc}):\n${_err}")
    endif()
endforeach()

# Compare: every listed file with its hash, and nothing else.
set(_changed "")
set(_missing "")
foreach(_path _hash IN ZIP_LISTS _paths _hashes)
    if(NOT EXISTS "${WORK}/${_path}")
        list(APPEND _missing "${_path}")
    else()
        file(SHA256 "${WORK}/${_path}" _got)
        if(NOT _got STREQUAL _hash)
            list(APPEND _changed "${_path}")
        endif()
    endif()
endforeach()
file(GLOB_RECURSE _found LIST_DIRECTORIES false RELATIVE "${WORK}" "${WORK}/*")
list(REMOVE_ITEM _found ${_paths})

set(_report "")
foreach(_kind changed missing found)
    list(LENGTH _${_kind} _n)
    if(_n GREATER 0)
        list(SUBLIST _${_kind} 0 20 _some)
        list(JOIN _some "\n    " _some)
        if(_kind STREQUAL "changed")
            string(APPEND _report "${_n} files have another SHA-256:\n    ${_some}\n")
        elseif(_kind STREQUAL "missing")
            string(APPEND _report "${_n} listed files were not written:\n    ${_some}\n")
        else()
            string(APPEND _report "${_n} files were written but are not listed:\n    ${_some}\n")
        endif()
    endif()
endforeach()
if(NOT _report STREQUAL "")
    list(LENGTH _runs _songs)
    message(FATAL_ERROR "snapshots.cmake: groovecore gen's files (${_songs} songs, in ${WORK}) differ from ${MANIFEST}:\n"
        "${_report}The in-memory check (core: the generator snapshots match ...) says what to do.")
endif()
list(LENGTH _runs _songs)
message(STATUS "snapshots.cmake: ${_count} files from ${_songs} groovecore gen runs match ${MANIFEST}")
