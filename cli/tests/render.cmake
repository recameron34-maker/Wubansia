# groovecore render's ctest checks (P12). One check per run:
#
#   cmake -DGROOVECORE=<exe> -DFIXTURE=<groovecore_render_fixture> -DSOURCE=<repo> -DWORK=<dir> -DCHECK=<name> -P render.cmake
#
# The synthetic kit's checks render SYN01, which groovecore_render_fixture writes into WORK/syn01 (manifests, MIDI
# and samples), so they need no real content:
#   deterministic  the loop rendered twice: the same bytes
#   blocks         the loop at blocks of 64, 512 and 4096 samples: the same bytes
#   taps           a tapped render's mix equals the plain render's, bit for bit, and --solo writes its channel's tap
#   latency        --keep-latency is the aligned render, the sidecar's latency later
#   song           a Song section by name: the sidecar's start bar and length, and 4 bars of it
#   pcm24          --format pcm24 writes a 24-bit PCM header
# The real kits:
#   no_content     FUT01 with no content folder anywhere ($WUBANSIA_CONTENT_DIR unset, HOME moved to an empty
#                  folder): exit 1 and the locator's message; the committed manifests and MIDI still parse and build
#   matrix         with $WUBANSIA_CONTENT_DIR: every kit's Loop (3 passes) and 8 bars of every Song section, in both
#                  variants, with bass A and B, at the kit's tempo, into WORK/<kit>/; each one finite and not silent.
#                  Without content it prints "SKIP" (the ctest entry's SKIP_REGULAR_EXPRESSION).

foreach(_v GROOVECORE WORK CHECK)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "render.cmake: set ${_v}")
    endif()
endforeach()

# Runs groovecore render with the given arguments; fails unless it exits 0. Sets _out to its stdout.
function(_render)
    execute_process(COMMAND "${GROOVECORE}" render ${ARGN} RESULT_VARIABLE _code OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    if(NOT _code EQUAL 0)
        message(FATAL_ERROR "groovecore render ${ARGN}\nexit ${_code}\n--- stdout ---\n${_o}--- stderr ---\n${_e}")
    endif()
    set(_out "${_o}" PARENT_SCOPE)
endfunction()

function(_same a b what)
    file(SHA256 "${a}" _ha)
    file(SHA256 "${b}" _hb)
    if(NOT _ha STREQUAL _hb)
        message(FATAL_ERROR "${what}: ${a} and ${b} differ")
    endif()
endfunction()

# The value of key in a render's sidecar (FILE.json beside FILE.wav).
function(_meta wav key var)
    string(REGEX REPLACE "\\.wav$" ".json" _json "${wav}")
    file(READ "${_json}" _text)
    string(JSON _value GET "${_text}" ${key})
    set(${var} "${_value}" PARENT_SCOPE)
endfunction()

set(_float_data 58) # the float WAV's data offset: RIFF 12, 'fmt ' 8 + 18, 'fact' 12, 'data' 8

function(_fixture)
    if(NOT DEFINED FIXTURE)
        message(FATAL_ERROR "render.cmake: set FIXTURE")
    endif()
    file(REMOVE_RECURSE "${WORK}")
    file(MAKE_DIRECTORY "${WORK}")
    execute_process(COMMAND "${FIXTURE}" "${WORK}/syn01" RESULT_VARIABLE _code OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    if(NOT _code EQUAL 0)
        message(FATAL_ERROR "groovecore_render_fixture: exit ${_code}\n${_o}${_e}")
    endif()
endfunction()

set(_syn --kit SYN01 --manifests "${WORK}/syn01/plugin" --midi "${WORK}/syn01/midi" --content "${WORK}/syn01/samples")

if(CHECK STREQUAL "deterministic")
    _fixture()
    _render(${_syn} --passes 2 --out "${WORK}/a.wav")
    _render(${_syn} --passes 2 --out "${WORK}/b.wav")
    _same("${WORK}/a.wav" "${WORK}/b.wav" "two renders")
    _meta("${WORK}/a.wav" frames _frames)
    _meta("${WORK}/a.wav" peak_dbfs _peak)
    if(_frames LESS 100000 OR _peak STREQUAL "null")
        message(FATAL_ERROR "SYN01's loop: ${_frames} frames, peak ${_peak}: a silent or short render")
    endif()

elseif(CHECK STREQUAL "blocks")
    _fixture()
    foreach(_block 64 512 4096)
        _render(${_syn} --passes 2 --block ${_block} --variant novox --bass a --out "${WORK}/block${_block}.wav")
    endforeach()
    _same("${WORK}/block64.wav" "${WORK}/block512.wav" "blocks of 64 and 512")
    _same("${WORK}/block4096.wav" "${WORK}/block512.wav" "blocks of 4096 and 512")

elseif(CHECK STREQUAL "taps")
    _fixture()
    _render(${_syn} --out "${WORK}/plain.wav")
    _render(${_syn} --taps "${WORK}/taps" --out "${WORK}/tapped.wav")
    _same("${WORK}/plain.wav" "${WORK}/tapped.wav" "the mix of a tapped render")
    _meta("${WORK}/tapped.wav" "channels;0;id" _first)
    if(NOT EXISTS "${WORK}/taps/${_first}.wav" OR NOT EXISTS "${WORK}/taps/send_space.wav")
        message(FATAL_ERROR "no taps in ${WORK}/taps")
    endif()
    _render(${_syn} --solo "${_first}" --out "${WORK}/solo.wav")
    _same("${WORK}/solo.wav" "${WORK}/taps/${_first}.wav" "--solo ${_first} and its tap")

elseif(CHECK STREQUAL "latency")
    _fixture()
    _render(${_syn} --passes 1 --out "${WORK}/aligned.wav")
    _render(${_syn} --passes 1 --keep-latency --out "${WORK}/late.wav")
    _meta("${WORK}/aligned.wav" latency _latency)
    if(_latency LESS 1)
        message(FATAL_ERROR "the sidecar's latency is ${_latency}")
    endif()
    # 2 s of the aligned render from 0.25 s on equal the late one's, latency frames on
    math(EXPR _from "${_float_data} + 12000 * 8")
    math(EXPR _late "${_from} + ${_latency} * 8")
    file(READ "${WORK}/aligned.wav" _a OFFSET ${_from} LIMIT 768000 HEX)
    file(READ "${WORK}/late.wav" _b OFFSET ${_late} LIMIT 768000 HEX)
    if(NOT _a STREQUAL _b)
        message(FATAL_ERROR "--keep-latency isn't the aligned render ${_latency} frames later")
    endif()
    if(_a MATCHES "^0*$")
        message(FATAL_ERROR "the compared stretch is silent")
    endif()

elseif(CHECK STREQUAL "song")
    _fixture()
    _render(${_syn} --mode song --section Drop --bars 4 --out "${WORK}/drop.wav")
    _meta("${WORK}/drop.wav" section _section)
    _meta("${WORK}/drop.wav" start_bar _start)
    _meta("${WORK}/drop.wav" bars _bars)
    _meta("${WORK}/drop.wav" peak_dbfs _peak)
    if(NOT _section STREQUAL "Drop" OR _start LESS 2 OR NOT _bars EQUAL 4 OR _peak STREQUAL "null")
        message(FATAL_ERROR "the Drop: section ${_section}, bar ${_start}, ${_bars} bars, peak ${_peak}")
    endif()
    execute_process(COMMAND "${GROOVECORE}" render ${_syn} --mode song --section Nowhere --out "${WORK}/none.wav"
        RESULT_VARIABLE _code ERROR_VARIABLE _e OUTPUT_QUIET)
    if(NOT _code EQUAL 2 OR NOT _e MATCHES "no section \"Nowhere\"")
        message(FATAL_ERROR "an unknown section: exit ${_code}\n${_e}")
    endif()

elseif(CHECK STREQUAL "pcm24")
    _fixture()
    _render(${_syn} --passes 1 --format pcm24 --out "${WORK}/pcm24.wav")
    file(READ "${WORK}/pcm24.wav" _head LIMIT 44 HEX)
    # RIFF....WAVE 'fmt ' 16, PCM (1), 2 channels, 48000 Hz, 288000 bytes/s, block 6, 24 bits, 'data'
    if(NOT _head MATCHES "^52494646........57415645666d7420100000000100020080bb0000006504000600180064617461")
        message(FATAL_ERROR "not a 24-bit stereo 48 kHz PCM header: ${_head}")
    endif()

elseif(CHECK STREQUAL "no_content")
    if(NOT DEFINED SOURCE)
        message(FATAL_ERROR "render.cmake: set SOURCE")
    endif()
    file(REMOVE_RECURSE "${WORK}")
    file(MAKE_DIRECTORY "${WORK}/home")
    unset(ENV{WUBANSIA_CONTENT_DIR})
    set(ENV{HOME} "${WORK}/home")
    set(ENV{APPDATA} "${WORK}/home")
    execute_process(COMMAND "${GROOVECORE}" render --kit FUT01 --manifests "${SOURCE}/content/plugin" --midi "${SOURCE}/content/midi"
        --out "${WORK}/fut01.wav" RESULT_VARIABLE _code OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    if(NOT _code EQUAL 1 OR NOT _e MATCHES "samples: FUT01's samples aren't installed: no kit folder in .*Set WUBANSIA_CONTENT_DIR or pass --content")
        message(FATAL_ERROR "FUT01 without content: exit ${_code}\n--- stdout ---\n${_o}--- stderr ---\n${_e}")
    endif()
    if(EXISTS "${WORK}/fut01.wav")
        message(FATAL_ERROR "it wrote ${WORK}/fut01.wav without content")
    endif()

elseif(CHECK STREQUAL "matrix")
    if(NOT DEFINED SOURCE)
        message(FATAL_ERROR "render.cmake: set SOURCE")
    endif()
    if("$ENV{WUBANSIA_CONTENT_DIR}" STREQUAL "")
        message("SKIP: set WUBANSIA_CONTENT_DIR to a content folder to render the real kits")
        return()
    endif()
    set(_real --manifests "${SOURCE}/content/plugin" --midi "${SOURCE}/content/midi")
    set(_count 0)
    foreach(_kit FUT01 FUT02 FUT03 FUT04)
        string(TOLOWER "${_kit}" _dir)
        file(MAKE_DIRECTORY "${WORK}/${_dir}")
        foreach(_variant vox novox)
            foreach(_bass a b)
                set(_wav "${WORK}/${_dir}/loop_${_variant}_bass${_bass}.wav")
                _render(--kit ${_kit} ${_real} --variant ${_variant} --bass ${_bass} --passes 3 --out "${_wav}")
                _meta("${_wav}" peak_dbfs _peak)
                if(_peak STREQUAL "null")
                    message(FATAL_ERROR "${_wav} is silent")
                endif()
                math(EXPR _count "${_count} + 1")
                # 8 bars of every section the Song's markers name (its sidecar lists none: take them from a render)
                set(_section 1)
                while(TRUE)
                    set(_wav "${WORK}/${_dir}/song_${_variant}_bass${_bass}_s${_section}.wav")
                    execute_process(COMMAND "${GROOVECORE}" render --kit ${_kit} ${_real} --mode song --variant ${_variant} --bass ${_bass}
                        --section ${_section} --bars 8 --out "${_wav}" RESULT_VARIABLE _code OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
                    if(_code EQUAL 2 AND _e MATCHES "no section")
                        break()
                    elseif(NOT _code EQUAL 0)
                        message(FATAL_ERROR "${_kit} ${_variant} bass ${_bass} section ${_section}: exit ${_code}\n${_o}${_e}")
                    endif()
                    _meta("${_wav}" peak_dbfs _peak)
                    _meta("${_wav}" section _name)
                    if(_peak STREQUAL "null" AND NOT _name MATCHES "^(Intro|Outro)")
                        message(FATAL_ERROR "${_wav} (${_name}) is silent")
                    endif()
                    math(EXPR _section "${_section} + 1")
                    math(EXPR _count "${_count} + 1")
                endwhile()
                if(_section LESS 3)
                    message(FATAL_ERROR "${_kit} ${_variant}: the Song has fewer than 2 sections")
                endif()
            endforeach()
        endforeach()
    endforeach()
    message("rendered ${_count} files under ${WORK}")

else()
    message(FATAL_ERROR "render.cmake: unknown CHECK \"${CHECK}\"")
endif()
