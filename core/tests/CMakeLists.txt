# core_tests: every .cpp under core/tests/ (subfolders too), one Catch2 binary. Add a test file and
# the next build picks it up. Test names must be unique in the binary (Catch2 stops otherwise);
# ctest lists them as "core: <name>" with the label "core", so `ctest -L core` runs just these.
#
# The tests can find files through these definitions:
#   WUBANSIA_SOURCE_DIR    the repository root (content/, core/tests/ fixtures...)
#   WUBANSIA_BINARY_DIR    the top of the build tree (e.g. build/release)
#   WUBANSIA_TEST_TMP_DIR  a scratch folder in the build tree for files a test writes

file(GLOB_RECURSE WUBANSIA_CORE_TEST_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")

add_executable(core_tests ${WUBANSIA_CORE_TEST_SOURCES})
target_link_libraries(core_tests PRIVATE wubansia_core wubansia_core_warnings Catch2::Catch2WithMain)
# Catch2's headers as system headers here, so our warning flags (and -Werror) judge only our code.
target_include_directories(core_tests SYSTEM PRIVATE $<TARGET_PROPERTY:Catch2,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(core_tests PRIVATE
    WUBANSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    WUBANSIA_BINARY_DIR="${PROJECT_BINARY_DIR}"
    WUBANSIA_TEST_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}/tmp")

# Unlike the plugin's tests, core_tests stays universal on macOS: it is small, and the generator
# snapshots are meant to be checked on the x86_64 slice too (under Rosetta: arch -x86_64 core_tests).

# WUBANSIA_GOLDEN_DIR (cmake/WubansiaGolden.cmake) tells the legacy metric tests where golden_check
# exported the baseline, so a configure with another folder tests that folder; empty means build/golden.
catch_discover_tests(core_tests
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "core: "
    PROPERTIES LABELS core ENVIRONMENT "WUBANSIA_GOLDEN_DIR=${WUBANSIA_GOLDEN_DIR}")
