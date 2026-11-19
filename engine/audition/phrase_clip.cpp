#include "audition/phrase_clip.h"

namespace wubansia::engine
{
    std::unique_ptr<PhraseClip> makePhraseClip (std::span<const std::uint8_t>, const KitManifest&, const MixConfig&, std::int32_t, std::int64_t,
        std::uint32_t)
    {
        return nullptr;
    }

    void PhraseHandoff::stopUpTo (std::uint32_t) noexcept {}
}
