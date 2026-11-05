// The scheduler's non-template parts: see scheduler.h.

#include "sched/scheduler.h"

#include <algorithm>
#include <cmath>

namespace wubansia::engine
{
    void Scheduler::prepare (double sampleRate) noexcept
    {
        rate_ = sampleRate > 0.0 && std::isfinite (sampleRate) ? sampleRate : 48000.0;
        reset();
    }

    void Scheduler::reset() noexcept
    {
        now_ = 0;
        chunkStart_ = 0;
        playing_ = false;
        pendingZero_ = false;
        loopActive_ = false;
        current_ = nullptr;
        notes_ = {};
        ccs_ = {};
        keys_ = {};
        clearSounding();
        ++epoch_;
        setTempo (bpm_);
        anchor (0, 0.0);
    }

    void Scheduler::setTempo (double bpm) noexcept
    {
        bpm_ = std::clamp (bpm, kMinBpm, kMaxBpm);
        mps_ = bpm_ * static_cast<double> (kMilliticksPerQuarter) / (60.0 * rate_);
    }

    void Scheduler::anchor (std::int64_t sample, double h) noexcept
    {
        anchorSample_ = sample;
        anchorH_ = h;
    }

    void Scheduler::armLoop (const TransportInfo& transport) noexcept
    {
        loopActive_ = false;
        if (! transport.looping || ! std::isfinite (transport.loopStartPpq) || ! std::isfinite (transport.loopEndPpq))
            return;
        const double start = transport.loopStartPpq * static_cast<double> (kMilliticksPerQuarter);
        const double end = transport.loopEndPpq * static_cast<double> (kMilliticksPerQuarter);
        if (! (end - start >= kMinLoopSamples * mps_) || std::abs (start) > kMaxPosition || std::abs (end) > kMaxPosition)
            return;
        // A host wraps only a playhead that reaches the region's end from before it. Up to 1 ms past the
        // end still counts (a host's rounding at a block that ends on it): the wrap comes at once.
        if (positionAt (now_) >= end + kJumpSeconds * rate_ * mps_)
            return;
        loopActive_ = true;
        loopStartH_ = start;
        loopEndH_ = end;
    }

    void Scheduler::seek (const Timeline* timeline, std::int64_t fromSample, double minH) noexcept
    {
        current_ = timeline;
        notes_ = {};
        ccs_ = {};
        keys_ = {};
        if (timeline == nullptr)
            return;
        notes_ = seekStream (timeline->notes, fromSample, minH);
        ccs_ = seekStream (timeline->ccs, fromSample, minH);
        keys_ = seekStream (timeline->keys, fromSample, minH);
    }

    int Scheduler::earliestOff() const noexcept
    {
        int first = -1;
        for (int i = 0; i < count_; ++i)
            if (first < 0 || slots_[static_cast<std::size_t> (i)].off < slots_[static_cast<std::size_t> (first)].off)
                first = i;
        return first;
    }

    int Scheduler::barLineWithin (int numSamples) const noexcept
    {
        if (! playing_ || numSamples <= 0)
            return -1;
        const double bar = static_cast<double> (kMilliticksPerBar);
        const double h = positionAt (now_);
        auto k = static_cast<std::int64_t> (std::floor (h / bar));
        for (int tries = 0; tries < 3; ++tries, ++k)
        {
            const std::int64_t s = sampleAt (static_cast<double> (k) * bar);
            if (s >= now_)
                return s < now_ + numSamples ? static_cast<int> (s - now_) : -1;
        }
        return -1;
    }

    Position Scheduler::position() const noexcept
    {
        Position p;
        p.playing = playing_;
        p.bpm = bpm_;
        if (! playing_)
            return p;
        const double h = positionAt (now_);
        p.ppq = h / static_cast<double> (kMilliticksPerQuarter);
        if (current_ == nullptr || h < 0.0)
            return p;
        const auto length = static_cast<double> (current_->length);
        double inTimeline = h;
        if (current_->loops)
        {
            if (length <= 0.0)
                return p;
            inTimeline = h - std::floor (h / length) * length;
        }
        else if (h >= length)
            return p;
        p.ticks = inTimeline / static_cast<double> (kMilliticksPerTick);

        const auto& sections = current_->sections;
        const auto after = std::partition_point (sections.begin(), sections.end(),
            [inTimeline] (const Section& s) { return static_cast<double> (s.pos) <= inTimeline; });
        p.section = static_cast<std::int32_t> (after - sections.begin()) - 1;
        return p;
    }
}
