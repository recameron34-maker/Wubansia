# 0002. Plugin identity

**Status:** Accepted, 2026-10-28.

## Context
DAWs store a plugin's identity in every saved project. If one of these values changes, saved projects lose the plugin, its state or its automation. The user approved the proposed values (D1). No label name or domain was given, so the company is the placeholder "Wubansia Audio" and the reverse-DNS IDs use `wubansiaaudio`.

## Decision
The values marked "Yes" are **permanent from the first time anyone saves a DAW project with the plugin**, including the M0 DAW test projects.

| Field | Value | Hex | Permanent? | Rule / why |
|---|---|---|---|---|
| `PRODUCT_NAME` | `Wubansia` | | Soft | Name in DAWs and the bundle file names. `JucePlugin_Name` defaults to it. |
| `COMPANY_NAME` | `Wubansia Audio` | | Soft | Vendor name in DAWs and Info.plist. |
| `BUNDLE_ID` | `com.wubansiaaudio.wubansia` | | **Yes** (macOS) | Reverse DNS. |
| `CLAP_ID` | `com.wubansiaaudio.wubansia` | | **Yes** | Same as `BUNDLE_ID`, as in Pamplejuce. CLAP hosts store it. |
| `PLUGIN_MANUFACTURER_CODE` | `Wuba` | `0x57756261` | **Yes** | AU manufacturer, and one of the four 32-bit words in each VST3 class ID (the first two are JUCE constants). Needs at least one upper-case letter; GarageBand wants the first upper-case and the rest lower-case. There is no public registry to check it against. |
| `PLUGIN_CODE` | `Wbns` | `0x57626e73` | **Yes** | AU subtype, and another of the four words in each VST3 class ID. Exactly one upper-case letter. If unset, JUCE picks a random code on every configure. |
| AU type | `aumu` (`kAudioUnitType_MusicDevice`) | `0x61756d75` | **Yes** | Instrument. |
| VST3 categories | `Instrument Sampler` | | No | From JUCE's list. |
| CLAP features | `instrument sampler drum-machine stereo` | | No | All defined in CLAP 1.2.7. |
| `JUCE_VST3_CAN_REPLACE_VST2` | `0` | | **Yes** | JUCE's default is 1. Changing it later makes the VST3 "incompatible with previous versions". |
| `JUCE_FORCE_USE_LEGACY_PARAM_IDS` | `0`, set explicitly | | **Yes** | It changes the parameter IDs hosts see. |
| Gain parameter | ID `master_gain`, version hint 1 | | **Yes** | Becomes the master output level later. |
| State | root tag `WubansiaState`, `stateVersion="1"` | | **Yes** | Future migrations key off it. |

Rules that follow:
- Parameter IDs are snake_case. Rename the label, never the ID (CLAUDE.md).
- A parameter added in a later release gets a version hint higher than any earlier one: 1 for the first release, then 2, 3 and so on. Lower hints break saved AU automation in some hosts.
- A state format change bumps `stateVersion`, and every older version must keep loading.
- The values live in `cmake/WubansiaIdentity.cmake`. The frozen literals live only in `plugin/tests/IdentityTests.cpp`, so an edit to the CMake file alone fails CI.

How the tests enforce it:
- `IdentityTests` "identity matches ADR 0002": the name and company (`JucePlugin_Manufacturer`); `JucePlugin_ManufacturerCode` and `JucePlugin_PluginCode` against the hex values above; the AU type as the stringified literal `'aumu'` (`JUCE_STRINGIFY(JucePlugin_AUMainType)`); the bundle ID through `JUCE_STRINGIFY(JucePlugin_CFBundleIdentifier)`; the CLAP ID through `WUBANSIA_TEST_CLAP_ID`; `JucePlugin_IsSynth`, `JucePlugin_WantsMidiInput`, `JucePlugin_ProducesMidiOutput` and `JucePlugin_IsMidiEffect`. The file also has an `#error` if either JUCE flag above isn't 0.
- `IdentityTests` "bus layout": no inputs, one output bus that is stereo by default (mono is accepted, 5.1 is not), MIDI in, no MIDI out, not a MIDI effect, one program with a name.
- `ParameterTests` "parameter IDs are frozen": the complete list is `{master_gain}`. "master gain parameter": version hint 1, range −60 to +6 dB, default 0 dB.
- `StateTests`: the saved root tag is `WubansiaState` with `stateVersion == 1`, and the hard-coded M0 blob `<WubansiaState stateVersion="1"><PARAM id="master_gain" value="-12.5"/></WubansiaState>` still restores −12.5 dB.

## Consequences
- After the first saved project, changing a permanent value breaks that project in at least one format. It would need a new ADR and a migration plan.
- A domain the user owns can replace `wubansiaaudio` only before that first save.
- The two 4-character codes could collide with another vendor's. Nothing checks that.
- "Wubansia" contains no artist names, as the content rules require.

## Alternatives
- A reverse-DNS ID on a domain the user controls: better, but none was given.
- Leaving `PLUGIN_CODE` unset: JUCE would pick a new random code on every configure.
- `JUCE_VST3_CAN_REPLACE_VST2=1`: there is no VST2 to replace.

## Sources
- `libs/JUCE/docs/CMake API.md:518-533` (names and codes), `:600-606` (VST3 categories).
- `libs/JUCE/extras/Build/CMake/JUCEUtils.cmake:1624-1647` (the `JucePlugin_*` definitions), `:1899-1947` (AU main type).
- `libs/JUCE/modules/juce_audio_plugin_client/VST3/juce_VST3ModuleInfo.h:61` (VST3 class ID from the two codes) and `libs/JUCE/modules/juce_audio_processors_headless/utilities/juce_VST3Interface.h:181-253` (`jucePluginId`: two words that depend on the interface, then the manufacturer code, then the plugin code).
- `libs/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client.h:62-89` (the two JUCE flags).
- `libs/JUCE/modules/juce_audio_processors_headless/processors/juce_AudioProcessorParameter.h:51-64` (version hints).
- `libs/clap-juce-extensions/cmake/ClapTargetHelpers.cmake:13-14,137` (`CLAP_ID` is required and only defined on the CLAP target).
- `libs/clap-juce-extensions/clap-libs/clap/include/clap/plugin-features.h:16,35,37,77`.
- Pamplejuce `ca3cae0` `CMakeLists.txt:102-104` (`CLAP_ID` = `BUNDLE_ID`).
