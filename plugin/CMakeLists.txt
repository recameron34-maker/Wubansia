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

target_link_libraries(WubansiaPluginCode INTERFACE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    wubansia_build_defaults
    wubansia_engine) # the engine and, through it, the groove core (ADR 0017); both are built PIC for the .so

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
        WUBANSIA_TEST_CLAP_ID="${WUBANSIA_CLAP_ID}")

    target_link_libraries(WubansiaPluginTests PRIVATE WubansiaPluginCode Catch2::Catch2)

    # Universal macOS builds: the tests only run on the host's slice, so build just that one.
    if(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES ";")
        set_target_properties(WubansiaPluginTests PROPERTIES OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    endif()

    catch_discover_tests(WubansiaPluginTests DISCOVERY_MODE PRE_TEST)
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
