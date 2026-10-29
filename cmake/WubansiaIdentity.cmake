# Plugin identity. Every value marked PERMANENT is stored in DAW projects and must never
# change once anyone has saved a project with the plugin (docs/decisions/0002-plugin-identity.md).
# plugin/tests/IdentityTests.cpp holds the frozen values and fails if this file drifts.

set(WUBANSIA_PRODUCT_NAME "Wubansia")                  # shown in DAWs and used for bundle names
set(WUBANSIA_COMPANY_NAME "Wubansia Audio")            # vendor name in DAWs
set(WUBANSIA_BUNDLE_ID "com.wubansiaaudio.wubansia")   # PERMANENT (macOS bundle identifier)
set(WUBANSIA_CLAP_ID "${WUBANSIA_BUNDLE_ID}")          # PERMANENT (CLAP hosts store it)
set(WUBANSIA_MANUFACTURER_CODE "Wuba")                 # PERMANENT (AU manufacturer, part of the VST3 class ID)
set(WUBANSIA_PLUGIN_CODE "Wbns")                       # PERMANENT (AU subtype, part of the VST3 class ID)
