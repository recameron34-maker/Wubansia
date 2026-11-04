# groovecore, the command-line tool: `groovecore gen`, `groovecore metrics` (S1b), `render` later (S1d).
# Every .cpp and .h directly in cli/ is part of it, so a command can add its own files without
# editing this file. It links only the groove core for now; render brings juce_core and
# juce_audio_formats (ADR 0006).

file(GLOB WUBANSIA_CLI_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")

add_executable(groovecore ${WUBANSIA_CLI_SOURCES})
target_link_libraries(groovecore PRIVATE wubansia_core wubansia_core_warnings)
target_compile_definitions(groovecore PRIVATE WUBANSIA_VERSION="${PROJECT_VERSION}")

if(WUBANSIA_BUILD_TESTS)
    # The dispatcher's own behaviour; each command tests its options itself.
    # tests/expect.cmake runs a command and checks its exit code and output.
    string(REPLACE "." "\\." _version_regex "${PROJECT_VERSION}")
    function(_groovecore_test name exit_code stream regex)
        add_test(NAME "groovecore: ${name}"
            COMMAND ${CMAKE_COMMAND} -DEXPECT_EXIT=${exit_code} -DEXPECT_STREAM=${stream} "-DEXPECT_REGEX=${regex}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/expect.cmake" -- $<TARGET_FILE:groovecore> ${ARGN})
        set_tests_properties("groovecore: ${name}" PROPERTIES LABELS cli)
    endfunction()

    _groovecore_test("--version" 0 stdout "^groovecore ${_version_regex}\n$" --version)
    _groovecore_test("--help lists the commands" 0 stdout "Usage: groovecore <command>.*\n  gen .*\n  metrics " --help)
    _groovecore_test("help is --help" 0 stdout "Usage: groovecore <command>" help)
    _groovecore_test("no command is a usage error" 2 stderr "Usage: groovecore <command>")
    _groovecore_test("an unknown command is a usage error" 2 stderr "unknown command \"frobnicate\"" frobnicate)
    _groovecore_test("an unknown option is a usage error" 2 stderr "unknown command \"--frobnicate\"" --frobnicate)

    # groovecore gen: its options, refusals and one short run (the generator's own tests are in core/tests).
    set(_content "${PROJECT_SOURCE_DIR}/content")
    set(_gen_out "${CMAKE_CURRENT_BINARY_DIR}/gen-test")
    _groovecore_test("gen --help" 0 stdout "^Usage: groovecore gen --kit <ID> --seed <N> --out <DIR>" gen --help)
    _groovecore_test("gen without options is a usage error" 2 stderr "--kit, --seed and --out are required" gen)
    _groovecore_test("gen: a seed that is not a whole number is a usage error" 2 stderr "--seed takes a whole number from 0 to 18446744073709551615, not \"abc\""
        gen --kit FUT01 --seed abc --out "${_gen_out}/refused")
    _groovecore_test("gen: a seed above 64 bits is a usage error" 2 stderr "not \"18446744073709551616\""
        gen --kit FUT01 --seed 18446744073709551616 --out "${_gen_out}/refused")
    _groovecore_test("gen: bars off the 8-bar grid is a usage error" 2 stderr "bars must be a multiple of 8"
        gen --kit FUT01 --seed 1 --bars 12 --content "${_content}" --out "${_gen_out}/refused")
    _groovecore_test("gen: --feel and --quantized together is a usage error" 2 stderr "--feel and --quantized each ask for only one version"
        gen --kit FUT01 --seed 1 --feel --quantized --out "${_gen_out}/refused")
    _groovecore_test("gen: an unknown kit fails" 1 stderr "no kit \"NOPE\""
        gen --kit NOPE --seed 1 --content "${_content}" --out "${_gen_out}/refused")
    _groovecore_test("gen: a missing content folder fails and says so" 1 stderr "no content folder at"
        gen --kit FUT01 --seed 1 --content "${_gen_out}/no-such-folder" --out "${_gen_out}/refused")
    _groovecore_test("gen writes 16 bars of FUT01 as one region" 0 stdout "bar 1    01_intro_16bars\\.mid\nwrote 6 files under "
        gen --kit FUT01 --seed 1 --bars 16 --content "${_content}" --out "${_gen_out}/16bars")

    # The generator snapshots through groovecore itself: every file gen writes for the manifest's kits
    # and seeds, with its SHA-256 (cli/tests/snapshots.cmake). The in-memory check, and how to update
    # the manifest, is core/tests/test_gen_snapshots.cpp.
    add_test(NAME "groovecore: gen writes the snapshot files"
        COMMAND ${CMAKE_COMMAND} -DGROOVECORE=$<TARGET_FILE:groovecore> "-DCONTENT=${_content}"
            "-DMANIFEST=${PROJECT_SOURCE_DIR}/core/tests/snapshots/gen.sha256" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/snapshot-run"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/snapshots.cmake")
    set_tests_properties("groovecore: gen writes the snapshot files" PROPERTIES LABELS cli TIMEOUT 600)

    # A --bars cut through groovecore (cli/tests/cut.cmake): 8 bars into FUT01 seed 31's second drop,
    # where the chord change on the line is pushed 240 ticks early and the bass approaches it, it must
    # write the whole song's head, which groovecore_cut_check (a test tool, never installed) reads back
    # from the files; and the cut written into the whole song's folder is refused.
    add_executable(groovecore_cut_check tests/cut_check.cpp)
    target_link_libraries(groovecore_cut_check PRIVATE wubansia_core wubansia_core_warnings)
    function(_groovecore_cut_test name work)
        add_test(NAME "${name}"
            COMMAND ${CMAKE_COMMAND} -DGROOVECORE=$<TARGET_FILE:groovecore> -DCHECKER=$<TARGET_FILE:groovecore_cut_check> "-DCONTENT=${_content}"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/${work}" ${ARGN} -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cut.cmake")
        set_tests_properties("${name}" PROPERTIES LABELS cli)
    endfunction()
    _groovecore_cut_test("groovecore: gen --bars inside a section writes the whole song's head" cut-run -DKIT=FUT01 -DSEED=31 -DBARS=152)
    # The checker at the feel's extremes, where the files can lose which performed note plays which
    # score note (cut_check.cpp): it pairs them only inside the section the cut ends, so FUT01 seed 0's
    # break, whose song merges notes on one tick elsewhere, is checked exactly; and where the files
    # can't tell, it says so (exit 3) instead of calling a right cut wrong: seed 8's drop, whose last
    # 16th kick plays after the downbeat it is cut at, and seed 16's, which plays two kicks on the
    # cut's last tick.
    _groovecore_cut_test("groovecore_cut_check: at the feel's extremes it pairs the notes of the section the cut ends" cut-extreme-section
        -DKIT=FUT01 -DSEED=0 -DAMOUNT=1000 -DBARS=112 -DEXTREME=ON)
    _groovecore_cut_test("groovecore_cut_check: notes played out of score order across the cut are unknown, not wrong" cut-extreme-order
        -DKIT=FUT01 -DSEED=8 -DAMOUNT=2000 -DBARS=80 -DEXTREME=ON -DCHECK_EXIT=3 "-DCHECK_REGEX=out of score order")
    _groovecore_cut_test("groovecore_cut_check: notes merged on one tick at the cut are unknown, not wrong" cut-extreme-merged
        -DKIT=FUT01 -DSEED=16 -DAMOUNT=1000 -DBARS=96 -DEXTREME=ON -DCHECK_EXIT=3 "-DCHECK_REGEX=on one tick")

    # groovecore metrics: its options and refusals, on the audit's fixtures (tools/legacy_audit/fixtures).
    set(_fixtures "${PROJECT_SOURCE_DIR}/tools/legacy_audit/fixtures")
    _groovecore_test("metrics --help" 0 stdout "^Usage: groovecore metrics" metrics --help)
    _groovecore_test("metrics without files is a usage error" 2 stderr "name the MIDI files" metrics)
    _groovecore_test("metrics: an unknown option is a usage error" 2 stderr "unknown option \"--frobnicate\"" metrics --frobnicate "${_fixtures}")
    _groovecore_test("metrics: --gate and --expect-fail together is a usage error" 2 stderr "choose one" metrics --gate --expect-fail "${_fixtures}")
    _groovecore_test("metrics: a missing file fails" 1 stderr "no file or folder" metrics "${_fixtures}/no-such.mid")
    _groovecore_test("metrics scores a folder of files as seeds" 0 stdout "^28 seeds, 28 files.*\nG1 .*\nG10 .*\nfailed: " metrics "${_fixtures}")
    _groovecore_test("metrics --gate fails when a band fails" 1 stderr "gating bands fail: G1 " metrics --gate "${_fixtures}")
    _groovecore_test("metrics --json prints the result" 0 stdout "^\\{\n \"seeds\": 1,\n" metrics --json "${_fixtures}/harmony_a.mid")
    _groovecore_test("metrics: --legacy refuses files that are not the legacy layout" 2 stderr "--legacy needs every path to be <KIT>/<KIT>_<slot>\\.mid"
        metrics --legacy "${_fixtures}/harmony_a.mid")
    # --check compares every section both ways: one the expected file has and the result doesn't,
    # or the other way round, fails (cli/tests/check.cmake).
    add_test(NAME "groovecore: metrics --check fails an extra or a missing section"
        COMMAND ${CMAKE_COMMAND} -DGROOVECORE=$<TARGET_FILE:groovecore> "-DFIXTURE=${_fixtures}/harmony_a.mid"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/check-test" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check.cmake")
    set_tests_properties("groovecore: metrics --check fails an extra or a missing section" PROPERTIES LABELS cli)
endif()

# golden_check's last step (cmake/WubansiaGolden.cmake; plan sections 4.3 and 5.1): after the fresh
# legacy export matches its manifest, the generator metrics score it. The step passes only if legacy
# fails every gating band on its primary set (--expect-fail) and every one of the 5,842 values equals
# the committed core/tests/legacy_expected.json, but for DFA alpha's tolerance of +-1 in thousandths
# (--check, never regenerated to make this pass).
if(COMMAND wubansia_golden_check_add_step)
    wubansia_golden_check_add_step(NAME metrics DEPENDS groovecore
        COMMAND groovecore metrics --legacy --expect-fail --check "${PROJECT_SOURCE_DIR}/core/tests/legacy_expected.json" "${WUBANSIA_GOLDEN_DIR}")
endif()
