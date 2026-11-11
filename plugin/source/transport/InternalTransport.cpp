#include "transport/InternalTransport.h"

#include <algorithm>
#include <cmath>

namespace wubansia
{
    double InternalTransport::clampBpm (double bpm) noexcept
    {
        if (! std::isfinite (bpm) || bpm <= 0.0)
            return 0.0;
        return std::clamp (bpm, kMinBpm, kMaxBpm);
    }

    InternalTransport::Block InternalTransport::resolve (const juce::Optional<juce::AudioPlayHead::PositionInfo>& host, bool nonRealtime) noexcept
    {
        Block block;
        block.info.nonRealtime = nonRealtime;

        const auto ppq = host.hasValue() ? host->getPpqPosition() : juce::nullopt;
        const auto bpm = host.hasValue() ? host->getBpm() : juce::nullopt;
        const double hostBpm = bpm.hasValue() && std::isfinite (*bpm) && *bpm > 0.0 ? *bpm : 0.0;
        const bool hostPlaying = host.hasValue() && host->getIsPlaying();

        if (ppq.hasValue() && std::isfinite (*ppq))
        {
            if (hostPlaying)
            {
                // The host's transport; Preview gives way for good.
                preview_.store (false, std::memory_order_release);
                block.info.hasPosition = true;
                block.info.playing = true;
                block.info.ppq = *ppq;
                block.info.bpm = hostBpm;
                if (host->getIsLooping())
                    if (const auto loop = host->getLoopPoints(); loop.hasValue() && std::isfinite (loop->ppqStart) && std::isfinite (loop->ppqEnd)
                                                                 && loop->ppqEnd > loop->ppqStart)
                    {
                        block.info.looping = true;
                        block.info.loopStartPpq = loop->ppqStart;
                        block.info.loopEndPpq = loop->ppqEnd;
                    }
                return block;
            }
            if (preview_.load (std::memory_order_acquire))
            {
                // Preview: the plugin's own clock while the host is stopped, at the host's tempo.
                block.internal = true;
                block.info.playing = true;
                block.info.bpm = hostBpm;
                return block;
            }
            block.info.hasPosition = true;
            block.info.ppq = *ppq;
            block.info.bpm = hostBpm;
            return block;
        }

        // No position from the host (the Standalone app): the plugin's own clock.
        block.internal = true;
        block.info.playing = playing_.load (std::memory_order_acquire) || preview_.load (std::memory_order_acquire);
        const auto set = bpm_.load (std::memory_order_acquire);
        block.info.bpm = set > 0.0 ? set : hostBpm;
        return block;
    }
}
