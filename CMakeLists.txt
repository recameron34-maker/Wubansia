cmake_minimum_required(VERSION 3.25) # CMakePresets.json v6 needs 3.25; JUCE needs 3.22

# These must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# sccache on Windows: embed debug info (/Z7) instead of writing .pdb files. Set as the policy
# default so it also reaches JUCE, whose own cmake_minimum_required() would reset the policy.
set(CMAKE_POLICY_DEFAULT_CMP0141 NEW CACHE STRING "" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded CACHE STRING "" FORCE)

# Static MSVC runtime, so users need no Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Windows: build with clang-cl, the compiler CI uses, unless one was chosen explicitly.
if(CMAKE_HOST_WIN32 AND NOT CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
    file(TO_CMAKE_PATH "$ENV{VCINSTALLDIR}" _vc_install_dir)
    find_program(WUBANSIA_CLANG_CL clang-cl HINTS "${_vc_install_dir}/Tools/Llvm/x64/bin")
    if(NOT WUBANSIA_CLANG_CL)
        message(FATAL_ERROR "clang-cl not found. Install the Visual Studio component \"C++ Clang Compiler for Windows\" "
            "and configure from an x64 developer shell, or pass -DCMAKE_CXX_COMPILER=<compiler>.")
    endif()
    set(CMAKE_C_COMPILER "${WUBANSIA_CLANG_CL}")
    set(CMAKE_CXX_COMPILER "${WUBANSIA_CLANG_CL}")
endif()

file(STRINGS VERSION WUBANSIA_VERSION LIMIT_COUNT 1)
project(Wubansia VERSION ${WUBANSIA_VERSION} LANGUAGES C CXX) # JUCE needs C as well

if(WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Wubansia is 64-bit only. Configure from the \"x64 Native Tools Command Prompt for VS\", "
        "or from PowerShell after Launch-VsDevShell.ps1 -Arch amd64 -HostArch amd64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(WubansiaIdentity)
include(WubansiaDefaults)

option(WUBANSIA_COPY_PLUGINS "Install the plugins into the user's plugin folders after building" OFF)
option(WUBANSIA_WERROR "Treat warnings as errors in our own source files" OFF)
option(WUBANSIA_BUILD_TESTS "Build the tests" ON)

# Pinned submodules (docs/decisions/0001-framework.md). Run `git submodule update --init --recursive` first.
add_subdirectory(libs/JUCE)
add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

if(WUBANSIA_BUILD_TESTS)
    enable_testing() # here in the root, so `ctest --preset release` finds the tests
    add_subdirectory(libs/Catch2 EXCLUDE_FROM_ALL)
    include(libs/Catch2/extras/Catch.cmake)
endif()

add_subdirectory(plugin)
