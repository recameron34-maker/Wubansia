# Runs a command, then counts the .mid files it wrote, for groovecore's ctest entries.
#
#   cmake -DCOUNT=<n> -DDIR=<folder> [-DCLEAN=<folder>] -P count.cmake -- <command> [args...]
#
# CLEAN is removed first, so files from an earlier run never count. Fails (exit code 1) if the command
# fails or DIR holds another number of .mid files.

set(_command)
set(_after_dashes FALSE)
math(EXPR _last "${CMAKE_ARGC} - 1")
foreach(_i RANGE 0 ${_last})
    if(_after_dashes)
        list(APPEND _command "${CMAKE_ARGV${_i}}")
    elseif(CMAKE_ARGV${_i} STREQUAL "--")
        set(_after_dashes TRUE)
    endif()
endforeach()
if(NOT _command OR NOT DEFINED COUNT OR NOT DEFINED DIR)
    message(FATAL_ERROR "count.cmake: set COUNT and DIR, and give a command after --")
endif()
if(DEFINED CLEAN)
    file(REMOVE_RECURSE "${CLEAN}")
endif()
execute_process(COMMAND ${_command} RESULT_VARIABLE _code OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _code STREQUAL "0")
    message(FATAL_ERROR "exit code ${_code}\nstdout:\n${_out}\nstderr:\n${_err}")
endif()
file(GLOB _files "${DIR}/*.mid")
list(LENGTH _files _n)
if(NOT _n EQUAL COUNT)
    message(FATAL_ERROR "${_n} .mid files in ${DIR}, expected ${COUNT}\nstdout:\n${_out}")
endif()
