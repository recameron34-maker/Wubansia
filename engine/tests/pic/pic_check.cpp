// A shared object linking the engine and core the way the plugin's VST3 and CLAP do
// (engine/tests/CMakeLists.txt). It uses an engine class and core's SMF and JSON readers, so their
// objects really are pulled into the link; on Linux that link fails if either library isn't
// position-independent.

#include "engine.h"
#include "gen/json.h"
#include "midi/reader.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

#if defined(_WIN32)
    #define WUBANSIA_PIC_CHECK_EXPORT __declspec (dllexport)
#else
    #define WUBANSIA_PIC_CHECK_EXPORT __attribute__ ((visibility ("default")))
#endif

extern "C" WUBANSIA_PIC_CHECK_EXPORT int wubansiaPicCheck (const std::uint8_t* bytes, std::size_t size);

extern "C" WUBANSIA_PIC_CHECK_EXPORT int wubansiaPicCheck (const std::uint8_t* bytes, std::size_t size)
{
    wubansia::engine::Engine engine;
    engine.prepare (48000.0, 64, 2);
    const auto smf = wubansia::midi::readSmf ({ bytes, size });
    const auto json = wubansia::gen::json::parse (std::string_view (reinterpret_cast<const char*> (bytes), size));
    return (smf.ok() ? 1 : 0) + (json.ok() ? 2 : 0) + engine.latencySamples();
}
