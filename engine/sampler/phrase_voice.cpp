// PhraseVoice (P6): sampler/phrase_voice.h says what it does. tools/sound/phrase_tables.py's play () is its model:
// the same gains in the same order, (stem x gain) x envelope, in double, so at 48 kHz on the model's grid it gives
// the model's samples to the last bit.

#include "sampler/phrase_voice.h"

#include "sampler/envelope.h" // kPi, samplesOf
#include "sampler/sinc_bank.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace wubansia::engine
{
    namespace
    {
        constexpr double kContentRate = 48000.0;

        double raisedIn (double n, double length) noexcept { return 0.5 - 0.5 * std::cos (kPi * n / length); }
        double raisedOut (double n, double length) noexcept { return 0.5 + 0.5 * std::cos (kPi * n / length); }

        // Microseconds as 48 kHz stem frames, as the model counts them: us x 48000 // 10^6.
        double stemFramesOf (std::int64_t us) noexcept { return static_cast<double> (us * 48000 / 1000000); }

        std::int64_t wrapInto (std::int64_t a, std::int64_t length) noexcept
        {
            const auto r = a % length;
            return r < 0 ? r + length : r;
        }
    }

    bool playsSlices (const Phrase& phrase, std::int32_t kitBpm, double hostBpm, PhraseMode mode) noexcept
    {
        if (mode != PhraseMode::automatic || kitBpm <= 0 || ! (hostBpm > 0.0))
            return false;
        const auto kit = static_cast<double> (kitBpm);
        // abs (host / kit - 1) x 1000 <= tolerance, times kit; the slack keeps 2 % exactly (130.56 for 128) in.
        return std::fabs (hostBpm - kit) * 1000.0 <= static_cast<double> (phrase.tolerancePm) * kit + 1.0e-6;
    }

    // ------------------------------------------------------------------ the queue

    void PhraseVoice::prepare (double sampleRate)
    {
        rate = sampleRate > 0.0 ? sampleRate : kContentRate;
        ratio = kContentRate / rate;
        unit = std::fabs (rate - kContentRate) < 1.0e-9;
        const auto& bank = SincBank::instance();
        sinc = bank.table (bank.bankFor (ratio));
        samplesPerUs = rate / 1.0e6;
        reset();
    }

    void PhraseVoice::reset() noexcept
    {
        for (auto& h : heads)
            h = Head {};
        eventCount = 0;
        kit_ = nullptr;
        phrase_ = -1;
        setup = Setup {};
        stats_ = Stats {};
    }

    void PhraseVoice::start (const Kit& kit, const NoteOn& note, int offset) noexcept
    {
        const bool sounding = std::any_of (heads.begin(), heads.end(), [] (const Head& h) { return h.on; });
        if (sounding && kit_ == &kit && phrase_ == note.phrase)
        {
            continueWith (note, offset); // it plays this phrase already
            return;
        }
        // A free voice: nothing of another phrase still sounds (the sampler stops a voice before reusing it).
        for (auto& h : heads)
            h.on = false;
        if (! setUp (kit, note.phrase))
        {
            ++stats_.dropped;
            return;
        }
        if (eventCount >= kMaxEvents)
        {
            ++stats_.dropped;
            return;
        }
        auto& e = events[static_cast<std::size_t> (eventCount++)];
        e.kind = EventKind::start;
        e.offset = offset;
        e.fadeSamples = 0;
        e.kit = &kit;
        e.note = note;
    }

    void PhraseVoice::continueWith (const NoteOn& note, int offset) noexcept
    {
        if (eventCount >= kMaxEvents)
        {
            ++stats_.dropped;
            return;
        }
        auto& e = events[static_cast<std::size_t> (eventCount++)];
        e.kind = EventKind::next;
        e.offset = offset;
        e.fadeSamples = 0;
        e.kit = kit_;
        e.note = note;
    }

    void PhraseVoice::stop (int offset, int fadeSamples) noexcept
    {
        if (eventCount >= kMaxEvents)
        {
            // A stop must not get lost: it takes the last place.
            events[static_cast<std::size_t> (kMaxEvents - 1)] = Event { EventKind::stop, offset, fadeSamples, kit_, NoteOn {} };
            ++stats_.dropped;
            return;
        }
        auto& e = events[static_cast<std::size_t> (eventCount++)];
        e.kind = EventKind::stop;
        e.offset = offset;
        e.fadeSamples = fadeSamples;
        e.kit = kit_;
        e.note = NoteOn {};
    }

    bool PhraseVoice::active() const noexcept
    {
        return eventCount > 0 || std::any_of (heads.begin(), heads.end(), [] (const Head& h) { return h.on; });
    }

    double PhraseVoice::readPosition() const noexcept
    {
        const Head* newest = nullptr;
        for (const auto& h : heads)
            if (h.on && (newest == nullptr || h.order > newest->order))
                newest = &h;
        return newest != nullptr ? newest->pos : -1.0;
    }

    void PhraseVoice::render (MixBus& bus, SendBus& sends, int numSamples, const float* laneSend) noexcept
    {
        const int n = std::min (numSamples, std::min (bus.maxBlock(), sends.maxBlock()));
        if (n <= 0)
            return;
        // in offset order, keeping the order of calls at one offset (an insertion sort: nothing allocates)
        for (int i = 1; i < eventCount; ++i)
            for (int j = i; j > 0 && events[static_cast<std::size_t> (j - 1)].offset > events[static_cast<std::size_t> (j)].offset; --j)
                std::swap (events[static_cast<std::size_t> (j - 1)], events[static_cast<std::size_t> (j)]);
        int cursor = 0;
        for (int i = 0; i < eventCount; ++i)
        {
            const auto& e = events[static_cast<std::size_t> (i)];
            const int at = std::clamp (e.offset, 0, n - 1);
            renderHeads (bus, sends, laneSend, cursor, std::max (cursor, at));
            cursor = std::max (cursor, at);
            apply (e);
        }
        eventCount = 0;
        renderHeads (bus, sends, laneSend, cursor, n);
    }

    // ------------------------------------------------------------------ the notes

    bool PhraseVoice::setUp (const Kit& kit, int phrase) noexcept
    {
        if (phrase < 0 || static_cast<std::size_t> (phrase) >= kit.phrases.size())
            return false;
        const auto& p = kit.phrases[static_cast<std::size_t> (phrase)];
        if (p.sliceCount == 0 || static_cast<std::size_t> (p.firstSlice) + p.sliceCount > kit.slices.size())
            return false;
        std::int64_t frames = 0;
        for (const auto file : p.stem)
        {
            if (file >= kit.audio.size() || file >= kit.files.size())
                return false;
            const auto f = kit.view (file).frames;
            frames = frames == 0 ? f : std::min (frames, f);
        }
        setup.phrase = &p;
        setup.slices = kit.slices.data() + p.firstSlice;
        setup.count = p.sliceCount;
        setup.looped = setup.slices[0].start == 0 && setup.slices[setup.count - 1].end == frames;
        setup.gain = std::pow (10.0, (static_cast<double> (p.gainMdb) / 1000.0) / 20.0); // the model's undb (gain_mdb / 1000)
        setup.channel = p.mixChannel;
        setup.fadeFrames = stemFramesOf (p.fadeUs);
        setup.runFrames = stemFramesOf (kRunFadeUs);
        kit_ = &kit;
        phrase_ = phrase;
        return true;
    }

    int PhraseVoice::follower (int slice) const noexcept
    {
        if (slice + 1 < setup.count)
            return slice + 1;
        return setup.looped ? 0 : -1; // a looped stem's last slice reads on into its first
    }

    bool PhraseVoice::expects (const Head& h, int slice) const noexcept
    {
        if (! h.on || h.timeOutLength > 0.0)
            return false;
        if (! h.seen)
            return h.slice == slice; // it reads into this slice, its note not come yet
        return h.follows && h.earlySlice < 0 && follower (h.slice) == slice;
    }

    PhraseVoice::Head* PhraseVoice::current() noexcept
    {
        Head* newest = nullptr;
        for (auto& h : heads)
            if (h.on && h.timeOutLength <= 0.0 && (newest == nullptr || h.order > newest->order))
                newest = &h;
        return newest;
    }

    PhraseVoice::Head& PhraseVoice::newHead() noexcept
    {
        for (auto& h : heads)
            if (! h.on)
                return h;
        Head* oldest = &heads[0]; // all four sound: the oldest gives way
        for (auto& h : heads)
            if (h.order < oldest->order)
                oldest = &h;
        oldest->on = false;
        return *oldest;
    }

    void PhraseVoice::startHead (Head& h, const NoteOn& note, int slice, double pos) noexcept
    {
        h = Head {};
        h.on = true;
        h.order = ++order;
        h.character = note.character == 1 ? 1 : 0;
        h.view = kit_->view (setup.phrase->stem[h.character]);
        h.stereo = h.view.channels == 2 && h.view.right != h.view.left;
        h.pos = pos;
        h.slice = slice;
        h.seen = true;
        h.follows = note.continues;
        h.space = note.throwSpace;
        h.echo = note.throwEcho;
        h.spaceNow = h.space;
        h.echoNow = h.echo;
        if (h.view.left == nullptr || h.view.frames <= 0)
            h.on = false;
    }

    // Fades a head out over samples from now, from where its time fade is: a sooner end stays.
    void PhraseVoice::fadeOutBy (Head& h, double samples) noexcept
    {
        if (samples <= 0.0)
        {
            h.on = false;
            return;
        }
        if (h.timeOutLength > 0.0)
        {
            if (h.timeOutLength - h.timeOutCount <= samples)
                return;
            // the new curve from the level this one has reached
            const double level = raisedOut (h.timeOutCount, h.timeOutLength);
            h.timeOutLength = samples;
            h.timeOutCount = samples * std::acos (std::clamp (2.0 * level - 1.0, -1.0, 1.0)) / kPi;
            return;
        }
        h.timeOutLength = samples;
        h.timeOutCount = 0.0;
    }

    void PhraseVoice::apply (const Event& e) noexcept
    {
        if (e.kind == EventKind::stop)
        {
            for (auto& h : heads)
                if (h.on)
                    fadeOutBy (h, static_cast<double> (e.fadeSamples));
            return;
        }
        if (e.kit != kit_ || kit_ == nullptr)
        {
            ++stats_.dropped;
            return;
        }
        take (e.note);
    }

    void PhraseVoice::take (const NoteOn& note) noexcept
    {
        const int j = note.slice - static_cast<int> (setup.phrase != nullptr ? setup.phrase->firstSlice : 0);
        if (setup.phrase == nullptr || note.phrase != phrase_ || j < 0 || j >= setup.count)
        {
            ++stats_.dropped;
            return;
        }
        const auto& s = setup.slices[j];
        const auto chase = static_cast<double> (std::max<std::int64_t> (0, note.startFrame));
        const double at = static_cast<double> (s.start) + chase; // where the note says the voice reads

        Head* cur = current();
        if (cur != nullptr && expects (*cur, j))
        {
            // The slice it reads into. Before its position gets there the note says it is the current slice's end.
            const bool before = cur->seen;
            const double expected = before ? static_cast<double> (setup.slices[cur->slice].end) + chase : at;
            const double off = std::fabs (cur->pos - expected);
            const bool wrap = before ? follower (cur->slice) <= cur->slice : cur->wrapped;
            const bool sameStem = (note.character == 1 ? 1 : 0) == cur->character;
            if (sameStem && (wrap ? off < kResyncFrames : off <= kReadOnFrames))
            {
                ++stats_.readOns;
                if (before)
                {
                    cur->earlySlice = j;
                    cur->earlyFollows = note.continues;
                    cur->earlySpace = note.throwSpace;
                    cur->earlyEcho = note.throwEcho;
                }
                else
                {
                    cur->seen = true;
                    cur->follows = note.continues;
                    cur->space = note.throwSpace;
                    cur->echo = note.throwEcho;
                }
                return;
            }
            // A frame or two off at the cycle's wrap: re-sync over 1 ms. Further, or the other stem: a jump.
            const bool resync = sameStem && wrap && off <= kReadOnFrames;
            const double samples = static_cast<double> (resync ? kResyncFadeUs : kJumpFadeUs) * samplesPerUs;
            fadeOutBy (*cur, samples);
            ++(resync ? stats_.resyncs : stats_.jumps);
            Head& h = newHead();
            startHead (h, note, j, at);
            h.timeInLength = samples;
            return;
        }
        if (cur != nullptr && (! cur->seen || cur->follows))
        {
            // It reads on into a slice this note isn't: a jump.
            const double samples = static_cast<double> (kJumpFadeUs) * samplesPerUs;
            fadeOutBy (*cur, samples);
            ++stats_.jumps;
            Head& h = newHead();
            startHead (h, note, j, at);
            h.timeInLength = samples;
            return;
        }
        // A new run, over whatever plays out (a slice without a follower fades on its own schedule).
        ++stats_.runs;
        Head& h = newHead();
        startHead (h, note, j, at);
        if (chase > 0.0)
            h.timeInLength = samplesOf (Sampler::kChaseFadeUs, rate);
        else
        {
            h.posIn = true;
            h.inFrom = static_cast<double> (s.start);
            h.inLength = std::min (setup.runFrames, static_cast<double> (s.end - s.start));
        }
    }

    // ------------------------------------------------------------------ the sound

    void PhraseVoice::read (const Head& h, double& left, double& right) const noexcept
    {
        const auto& v = h.view;
        if (unit)
        {
            const auto k = static_cast<std::int64_t> (h.pos); // whole frames at 48 kHz
            if (k < 0 || k >= v.frames)
            {
                left = right = 0.0;
                return;
            }
            left = static_cast<double> (v.left[k]);
            right = h.stereo ? static_cast<double> (v.right[k]) : left;
            return;
        }
        const auto i = static_cast<std::int64_t> (std::floor (h.pos));
        float c[SincBank::kTaps];
        sincCoefficients (sinc, h.pos - static_cast<double> (i), c);
        const auto first = i - SincBank::kLeft;
        if (first >= 0 && first + SincBank::kTaps <= v.frames)
        {
            left = static_cast<double> (sincDot (c, v.left + first));
            right = h.stereo ? static_cast<double> (sincDot (c, v.right + first)) : left;
            return;
        }
        // Near the stem's ends: a looped stem wraps round; another reads silence outside it.
        float xl[SincBank::kTaps];
        float xr[SincBank::kTaps];
        for (int j = 0; j < SincBank::kTaps; ++j)
        {
            auto k = first + j;
            if (setup.looped)
                k = wrapInto (k, v.frames);
            const bool inside = k >= 0 && k < v.frames;
            xl[j] = inside ? v.left[k] : 0.0f;
            xr[j] = inside ? v.right[k] : 0.0f;
        }
        left = static_cast<double> (sincDot (c, xl));
        right = h.stereo ? static_cast<double> (sincDot (c, xr)) : left;
    }

    void PhraseVoice::renderHeads (MixBus& bus, SendBus& sends, const float* laneSend, int from, int to) noexcept
    {
        if (from >= to || std::none_of (heads.begin(), heads.end(), [] (const Head& h) { return h.on; }))
            return;
        const bool toBus = setup.channel < bus.numChannels();
        float* outL = toBus ? bus.left (setup.channel) : nullptr;
        float* outR = toBus ? bus.right (setup.channel) : nullptr;
        float* space = sends.send (Send::space);
        float* echo = sends.send (Send::echo);
        const double throwStep = 1.0 / std::max (1.0, static_cast<double> (kThrowRampUs) * samplesPerUs);
        const auto* sl = setup.slices;

        for (int i = from; i < to; ++i)
        {
            double sumL = 0.0, sumR = 0.0, sumSpace = 0.0, sumEcho = 0.0;
            for (auto& h : heads)
            {
                if (! h.on)
                    continue;

                // Past its slice's end: on into the next slice, or done.
                while (h.on && h.pos >= static_cast<double> (sl[h.slice].end))
                {
                    const bool fading = h.timeOutLength > 0.0;
                    if (h.seen && ! h.follows)
                    {
                        h.on = false; // its slice has played out
                        break;
                    }
                    if (! h.seen && ! fading)
                    {
                        h.on = false; // a whole slice without its note
                        break;
                    }
                    // reading on: its note said so, or it fades out over a crossfade or a missed follower
                    const int next = follower (h.slice);
                    if (next < 0)
                    {
                        h.on = false;
                        break;
                    }
                    const bool wraps = next <= h.slice;
                    if (wraps)
                        h.pos -= static_cast<double> (sl[h.slice].end); // a looped stem's end: its start
                    // a follower within the tolerance comes at most this far into it: the slice's drift, 2 frames, 1 ms
                    h.late = kReadOnFrames + kLateMarginFrames
                             + static_cast<double> (sl[h.slice].end - sl[h.slice].start) * static_cast<double> (setup.phrase->tolerancePm) / 1000.0;
                    h.slice = next;
                    h.wrapped = wraps;
                    h.posIn = false;
                    if (! fading && h.earlySlice == next)
                    {
                        h.seen = true;
                        h.follows = h.earlyFollows;
                        h.space = h.earlySpace;
                        h.echo = h.earlyEcho;
                    }
                    else
                    {
                        h.seen = false;
                        h.follows = false;
                    }
                    h.earlySlice = -1;
                }
                if (! h.on)
                    continue;
                const auto& s = sl[h.slice];

                // Into a slice whose note doesn't come: fade over fade_us.
                if (! h.seen && h.timeOutLength <= 0.0 && h.pos - static_cast<double> (s.start) > h.late)
                {
                    fadeOutBy (h, static_cast<double> (setup.phrase->fadeUs) * samplesPerUs);
                    ++stats_.missed;
                }

                // The model's gains: the slice's fade without a follower, then the run's fade-in; then time fades.
                double g = 1.0;
                if (h.seen && ! h.follows)
                {
                    const double k = h.pos - static_cast<double> (s.fade);
                    if (k >= 0.0)
                    {
                        if (k >= std::min (setup.fadeFrames, static_cast<double> (s.end - s.fade)))
                        {
                            h.on = false; // silent from here to its end
                            continue;
                        }
                        g = raisedOut (k, setup.fadeFrames);
                    }
                }
                if (h.posIn)
                {
                    const double k = h.pos - h.inFrom;
                    if (k < h.inLength)
                        g *= raisedIn (k, setup.runFrames);
                    else
                        h.posIn = false;
                }
                if (h.timeInLength > 0.0)
                    g *= raisedIn (h.timeInCount, h.timeInLength);
                if (h.timeOutLength > 0.0)
                    g *= raisedOut (h.timeOutCount, h.timeOutLength);

                double l = 0.0, r = 0.0;
                read (h, l, r);
                l = (l * setup.gain) * g;
                r = (r * setup.gain) * g;
                sumL += l;
                sumR += r;
                if (h.space > 0.0f || h.echo > 0.0f || h.spaceNow > 0.0 || h.echoNow > 0.0)
                {
                    h.spaceNow += std::clamp (static_cast<double> (h.space) - h.spaceNow, -throwStep, throwStep);
                    h.echoNow += std::clamp (static_cast<double> (h.echo) - h.echoNow, -throwStep, throwStep);
                    const double mono = 0.5 * (l + r);
                    sumSpace += mono * h.spaceNow;
                    sumEcho += mono * h.echoNow;
                }

                h.pos += ratio;
                if (h.timeInLength > 0.0 && (h.timeInCount += 1.0) >= h.timeInLength)
                    h.timeInLength = 0.0;
                if (h.timeOutLength > 0.0 && (h.timeOutCount += 1.0) >= h.timeOutLength)
                    h.on = false;
            }
            if (toBus)
            {
                outL[i] += static_cast<float> (sumL);
                outR[i] += static_cast<float> (sumR);
            }
            if (sumSpace != 0.0 || sumEcho != 0.0)
            {
                const double lane = laneSend != nullptr ? static_cast<double> (laneSend[i]) : 1.0;
                space[i] += static_cast<float> (sumSpace * lane);
                echo[i] += static_cast<float> (sumEcho * lane);
            }
        }
    }
}
