# The instrument: VST3, AU, CLAP and Standalone from one JUCE target.
# See libs/JUCE/docs/CMake API.md for every juce_add_plugin option.

juce_add_plugin(Wubansia
    PRODUCT_NAME "${WUBANSIA_PRODUCT_NAME}"
    COMPANY_NAME "${WUBANSIA_COMPANY_NAME}"
    BUNDLE_ID "${WUBANSIA_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE ${WUBANSIA_MANUFACTURER_CODE}
    PLUGIN_CODE ${WUBANSIA_PLUGIN_CODE}
    FORMATS Standalone AU VST3 # AU builds on macOS only; CLAP comes from clap-juce-extensions below
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${WUBANSIA_COPY_PLUGINS})

clap_juce_extensions_plugin(TARGET Wubansia
    CLAP_ID "${WUBANSIA_CLAP_ID}"
    CLAP_FEATURES instrument sampler drum-machine stereo)

# Our code goes into an INTERFACE library so the plugin and the tests compile the same sources.
file(GLOB_RECURSE WUBANSIA_SOURCES CONFIGURE_DEPENDS source/*.cpp source/*.h)
add_library(WubansiaPluginCode INTERFACE)
target_sources(WubansiaPluginCode INTERFACE ${WUBANSIA_SOURCES})
target_include_directories(WubansiaPluginCode INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)

target_compile_definitions(WubansiaPluginCode INTERFACE
    JUCE_WEB_BROWSER=0 # the WebView arrives in M4
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0 # PERMANENT: changing it later breaks saved projects
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=0 # PERMANENT
    JUCE_USE_OGGVORBIS=0 # codecs M0 doesn't use (fewer third-party notices)
    JUCE_USE_OPUS=0
    JUCE_USE_WEBP=0
    WUBANSIA_VERSION="${PROJECT_VERSION}")

# Debug builds also look for samples in the checkout's content/samples (ADR 0022); never CI or Release.
target_compile_definitions(WubansiaPluginCode INTERFACE
    "$<$<CONFIG:Debug>:WUBANSIA_DEV_CONTENT_DIR=\"${PROJECT_SOURCE_DIR}/content/samples\">")

# The kits the plugin carries (ADR 0017; plugin/source/content/EmbeddedContent.h): each kit's two
# manifests, content/plugin/<kit>/<KIT>_kit.json and <KIT>_mix.json, every kit-prefixed MIDI file
# under content/midi/<kit>/ (the extended mixes' region files stay out), and its phrase spec and library
# from content/phrases/<kit>/, about 1.5 MB. The kit prefix
# keeps the resource names unique. Sorted, so the generated sources don't depend on the file system.
file(GLOB WUBANSIA_KIT_MANIFESTS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/content/plugin/*/*_kit.json"
    "${PROJECT_SOURCE_DIR}/content/plugin/*/*_mix.json")
set(WUBANSIA_CONTENT_FILES ${WUBANSIA_KIT_MANIFESTS})
foreach(manifest IN LISTS WUBANSIA_KIT_MANIFESTS)
    if(manifest MATCHES "/([a-z0-9]+)/([A-Z0-9]+)_kit\\.json$")
        file(GLOB_RECURSE kit_midi CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/content/midi/${CMAKE_MATCH_1}/${CMAKE_MATCH_2}_*.mid")
        # The Phrases drawer's spec and library (ADR 0031), about 40 KB for the four kits.
        file(GLOB kit_phrases CONFIGURE_DEPENDS
            "${PROJECT_SOURCE_DIR}/content/phrases/${CMAKE_MATCH_1}/${CMAKE_MATCH_2}_phrases.json"
            "${PROJECT_SOURCE_DIR}/content/phrases/${CMAKE_MATCH_1}/${CMAKE_MATCH_2}_phrase_library.json")
        list(APPEND WUBANSIA_CONTENT_FILES ${kit_midi} ${kit_phrases})
    endif()
endforeach()
list(SORT WUBANSIA_CONTENT_FILES)
if(NOT WUBANSIA_KIT_MANIFESTS)
    message(FATAL_ERROR "No kit manifests in ${PROJECT_SOURCE_DIR}/content/plugin: the plugin embeds them")
endif()
juce_add_binary_data(WubansiaContent
    NAMESPACE WubansiaContentData
    HEADER_NAME WubansiaContentData.h
    SOURCES ${WUBANSIA_CONTENT_FILES})

# The looks' thirteen font faces (ADR 0032): SIL OFL 1.1 subsets made by tools/fonts/subset_fonts.py, 326 KB,
# loaded once per process (editor/skin/Fonts.cpp). Licences in THIRD_PARTY_NOTICES.md and docs/third_party/fonts/.
file(GLOB WUBANSIA_FONT_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/*.ttf")
list(SORT WUBANSIA_FONT_FILES)
juce_add_binary_data(WubansiaFonts
    NAMESPACE WubansiaFontData
    HEADER_NAME WubansiaFontData.h
    SOURCES ${WUBANSIA_FONT_FILES})

target_link_libraries(WubansiaPluginCode INTERFACE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    wubansia_build_defaults
    wubansia_engine # the engine and, through it, the groove core (ADR 0017); both are built PIC for the .so
    WubansiaContent # the embedded manifests and MIDI
    WubansiaFonts) # the looks' fonts

target_link_libraries(Wubansia PRIVATE WubansiaPluginCode)

if(WUBANSIA_BUILD_TESTS)
    file(GLOB_RECURSE WUBANSIA_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp tests/*.h)
    add_executable(WubansiaPluginTests ${WUBANSIA_TEST_SOURCES})
    target_include_directories(WubansiaPluginTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

    # The tests see the same JucePlugin_* definitions as the plugin, plus the CLAP ID,
    # which otherwise exists only on the CLAP target.
    target_compile_definitions(WubansiaPluginTests PRIVATE
        $<TARGET_PROPERTY:Wubansia,COMPILE_DEFINITIONS>
        JUCE_MODAL_LOOPS_PERMITTED=1
        WUBANSIA_TEST_CLAP_ID="${WUBANSIA_CLAP_ID}"
        WUBANSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

    # wubansia_engine_testing: the synthetic kit, SMF builder and fake host, shared with engine_tests.
    target_link_libraries(WubansiaPluginTests PRIVATE WubansiaPluginCode wubansia_engine_testing Catch2::Catch2)

    # Universal macOS builds: the tests only run on the host's slice, so build just that one.
    if(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES ";")
        set_target_properties(WubansiaPluginTests PROPERTIES OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    endif()

    catch_discover_tests(WubansiaPluginTests DISCOVERY_MODE PRE_TEST)

    # The looks draw their words in embedded faces (ADR 0032), but strings the person or the run time supplies (folder
    # paths, file names) fall back to the platform's default sans-serif font: Liberation Sans on most Linux machines,
    # Lucida Grande on macOS, Verdana on Windows, and DejaVu Sans on Linux without Liberation, the widest of them (about
    # 12 % wider than Liberation Sans). Where DejaVu Sans is installed, the fit tests of the drag strip, every label, the
    # logo and the header run again in it alone (JUCE_FONT_PATH replaces the font folders), so a label that fits only
    # in narrower fonts fails here too. The filter is a comma list of test names.
    set(_wubansia_dejavu /usr/share/fonts/truetype/dejavu)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND EXISTS "${_wubansia_dejavu}/DejaVuSans-Bold.ttf")
        set(_wubansia_dejavu_test "the drag strip fits every label at 100 % in DejaVu Sans, the widest default font")
        add_test(NAME "${_wubansia_dejavu_test}"
                 COMMAND WubansiaPluginTests "the drag strip reads label, Loop, Song, Part*,every label reads in full in every look,the logo reads the kit's direction in full in every look,the header fits Phrases and Samples beside the zoom*")
        set_tests_properties("${_wubansia_dejavu_test}" PROPERTIES ENVIRONMENT "JUCE_FONT_PATH=${_wubansia_dejavu}")
    endif()
endif()

# Warnings as errors on our own files only: JUCE's module sources compile inside these targets too.
if(WUBANSIA_WERROR)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(_wubansia_werror /WX)
    else()
        set(_wubansia_werror -Werror)
    endif()
    set_source_files_properties(${WUBANSIA_SOURCES} ${WUBANSIA_TEST_SOURCES} PROPERTIES COMPILE_OPTIONS ${_wubansia_werror})
endif()
