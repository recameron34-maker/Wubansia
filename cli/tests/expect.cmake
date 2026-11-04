# Runs a command and checks its exit code and output, for groovecore's ctest entries.
#
#   cmake -DEXPECT_EXIT=<code> [-DEXPECT_STREAM=stdout|stderr -DEXPECT_REGEX=<regex>] -P expect.cmake -- <command> [args...]
#
# Fails (exit code 1) with a message showing both streams if the code differs or the chosen stream
# doesn't match the regex. CTest alone can't check for an exit code other than 0.

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

if(NOT _command)
    message(FATAL_ERROR "expect.cmake: no command after --")
endif()
if(NOT DEFINED EXPECT_EXIT)
    message(FATAL_ERROR "expect.cmake: set EXPECT_EXIT")
endif()

execute_process(COMMAND ${_command} RESULT_VARIABLE _code OUTPUT_VARIABLE _out ERROR_VARIABLE _err)

set(_problems)
if(NOT _code STREQUAL EXPECT_EXIT)
    string(APPEND _problems "exit code ${_code}, expected ${EXPECT_EXIT}\n")
endif()
if(DEFINED EXPECT_REGEX AND NOT EXPECT_REGEX STREQUAL "")
    if(EXPECT_STREAM STREQUAL "stdout")
        set(_text "${_out}")
    elseif(EXPECT_STREAM STREQUAL "stderr")
        set(_text "${_err}")
    else()
        message(FATAL_ERROR "expect.cmake: EXPECT_STREAM must be stdout or stderr")
    endif()
    if(NOT _text MATCHES "${EXPECT_REGEX}")
        string(APPEND _problems "${EXPECT_STREAM} doesn't match: ${EXPECT_REGEX}\n")
    endif()
endif()

if(_problems)
    message(FATAL_ERROR "${_command}\n${_problems}--- stdout ---\n${_out}--- stderr ---\n${_err}")
endif()
