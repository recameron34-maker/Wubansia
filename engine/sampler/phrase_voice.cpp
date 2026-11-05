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
#include <type_traits>

namespace wubansia::engine
{
    // The sampler steals a phrase voice by copying it into a spare slot to fade there: plain data only.
    static_assert (std::is_trivially_copyable_v<PhraseVoice>);

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
        bpm = 0.0;
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
        // A free voice: nothing of another phrase still sounds (the sampler moves a stolen voice to a slot of its own).
        for (auto& h : heads)
            h.on = false;
        if (! setUp (kit, note.phrase) || eventCount >= kMaxEvents)
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
        setup.frames = frames;
        setup.looped = frames > 0 && setup.slices[setup.count - 1].end - frames == setup.slices[0].start;
        setup.gain = std::pow (10.0, (static_cast<double> (p.gainMdb) / 1000.0) / 20.0); // the model's undb (gain_mdb / 1000)
        setup.channel = p.mixChannel;
        setup.fadeFrames = stemFramesOf (p.fadeUs);
        setup.kitBpm = static_cast<double> (kit.info.bpm);
        kit_ = &kit;
        phrase_ = phrase;
        return true;
    }

    int PhraseVoice::follower (int slice) const noexcept
    {
        if (slice + 1 < setup.count)
            return slice + 1;
        return setup.looped ? 0 : -1; // a looped stem's last slice reads on into slice 0
    }

    bool PhraseVoice::expects (const Head& h, int slice) const noexcept
    {
        if (! h.on || h.timeOutLength > 0.0 || h.delay > 0.0)
            return false;
        if (! h.seen)
            return h.slice == slice; // it reads into this slice, its note not come yet
        return h.follows && h.earlySlice < 0 && follower (h.slice) == slice;
    }

    // A free head; with all four sounding, the oldest fading one gives way, else the oldest (never `keep`).
    PhraseVoice::Head& PhraseVoice::newHead (const Head* keep) noexcept
    {
        for (auto& h : heads)
            if (! h.on)
                return h;
        Head* oldest = nullptr;
        for (auto& h : heads)
            if (&h != keep && h.timeOutLength > 0.0 && (oldest == nullptr || h.order < oldest->order))
                oldest = &h;
        if (oldest == nullptr)
            for (auto& h : heads)
                if (&h != keep && (oldest == nullptr || h.order < oldest->order))
                    oldest = &h;
        oldest->on = false;
        return *oldest;
    }

    PhraseVoice::Throw PhraseVoice::throwOf (const NoteOn& note) const noexcept
    {
        Throw t;
        if (setup.phrase == nullptr || setup.phrase->throwMode == PhraseThrow::baked)
            return t; // the stem has its throw in the audio
        t.space = std::max (0.0f, note.throwSpace);
        t.echo = std::max (0.0f, note.throwEcho);
        t.left = (t.space > 0.0f || t.echo > 0.0f) && note.lengthSamples > 0 ? static_cast<double> (note.lengthSamples) : 0.0;
        return t;
    }

    void PhraseVoice::startHead (Head& h, const NoteOn& note, int slice, double pos, bool follows) noexcept
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
        h.follows = follows;
        h.thrown = throwOf (note);
        h.spaceNow = h.thrown.space;
        h.echoNow = h.thrown.echo;
        if (h.view.left == nullptr || h.view.frames <= 0)
            h.on = false;
    }

    // From one head to a new one reading `slice` from pos, over us both ways.
    void PhraseVoice::crossfade (Head& from, const NoteOn& note, int slice, double pos, bool follows, std::int32_t us) noexcept
    {
        const double samples = static_cast<double> (us) * samplesPerUs;
        fadeOutBy (from, samples);
        Head& h = newHead (&from);
        startHead (h, note, slice, pos, follows);
        h.timeInLength = samples;
    }

    // A run from slice `slice`: from its start frame (and startFrame on), or anchored on its onset off tempo.
    void PhraseVoice::runFrom (const NoteOn& note, int slice, bool follows, bool anchored) noexcept
    {
        const auto& s = setup.slices[slice];
        const auto chase = static_cast<double> (std::max<std::int64_t> (0, note.startFrame));
        auto pos = static_cast<double> (s.start) + chase;
        double delay = 0.0;
        if (anchored)
        {
            // The onset frame (start + lead) lands where the note's onset does at the host's tempo: lead / q frames of
            // time after the note's start.
            const double q = bpm > 0.0 && setup.kitBpm > 0.0 ? bpm / setup.kitBpm : 1.0;
            pos += static_cast<double> (s.lead) * (1.0 - 1.0 / q);
            if (unit)
                pos = std::floor (pos + 0.5); // whole frames at 48 kHz
            if (pos < static_cast<double> (s.start))
            {
                delay = std::floor ((static_cast<double> (s.start) - pos) / ratio + 0.5);
                pos = static_cast<double> (s.start);
            }
        }
        ++stats_.runs;
        Head& h = newHead();
        startHead (h, note, slice, pos, follows);
        h.delay = delay;
        const auto fadeUs = s.lead == 0 ? kRunFadeLeadlessUs : kRunFadeUs;
        if (chase > 0.0)
            h.timeInLength = samplesOf (Sampler::kChaseFadeUs, rate);
        else if (pos > static_cast<double> (s.start))
            h.timeInLength = static_cast<double> (fadeUs) * samplesPerUs; // anchored part-way in
        else
        {
            h.posIn = true;
            h.inFrom = static_cast<double> (s.start);
            h.inFrames = stemFramesOf (fadeUs);
            h.inLength = std::min (h.inFrames, static_cast<double> (s.end - s.start));
        }
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
        // MIDI in never plays slices (the router never asks); a live note here plays nothing.
        if (setup.phrase == nullptr || note.phrase != phrase_ || j < 0 || j >= setup.count || note.source == Source::live)
        {
            ++stats_.dropped;
            return;
        }
        const auto& s = setup.slices[j];
        const auto chase = static_cast<double> (std::max<std::int64_t> (0, note.startFrame));
        const double at = static_cast<double> (s.start) + chase; // where the note says the voice reads
        const std::uint8_t character = note.character == 1 ? 1 : 0;
        // Off the kit's tempo a slice reads on into its follower only while the tempo keeps it within a frame.
        const double q = bpm > 0.0 && setup.kitBpm > 0.0 ? bpm / setup.kitBpm : 1.0;
        const bool steady = std::fabs (1.0 - 1.0 / q) * static_cast<double> (s.end - s.start) <= kDriftFrames;
        const bool follows = note.continues && steady;
        // A note's throw on a head that reads on: a thrown note sets it; a timed throw plays out to its note's end;
        // anything else follows the newest note.
        auto takeThrow = [this] (Throw& now, const NoteOn& n) {
            const auto t = throwOf (n);
            if (t.space > 0.0f || t.echo > 0.0f || now.left <= 0.0)
                now = t;
        };

        if (chase > 0.0)
        {
            // A chase: the head already reading this slice takes it; else it starts its own and leaves the rest.
            Head* reader = nullptr;
            for (auto& h : heads)
                if (h.on && h.timeOutLength <= 0.0 && h.delay <= 0.0 && h.slice == j && h.character == character
                    && (reader == nullptr || h.order > reader->order))
                    reader = &h;
            if (reader != nullptr)
            {
                const double off = std::fabs (reader->pos - at);
                if (off < kResyncFrames)
                {
                    ++stats_.readOns;
                    reader->seen = true;
                    reader->follows = follows;
                    takeThrow (reader->thrown, note);
                    return;
                }
                const bool near = off <= kReadOnFrames;
                ++(near ? stats_.resyncs : stats_.jumps);
                crossfade (*reader, note, j, at, follows, near ? kResyncFadeUs : kJumpFadeUs);
                return;
            }
            runFrom (note, j, follows, ! steady);
            return;
        }

        // The head that reads on into this slice (or already has): the newest such.
        Head* cur = nullptr;
        for (auto& h : heads)
            if (expects (h, j) && (cur == nullptr || h.order > cur->order))
                cur = &h;
        if (cur != nullptr)
        {
            // Before its position gets there the note says it is the current slice's end.
            const bool before = cur->seen;
            const double expected = before ? static_cast<double> (setup.slices[cur->slice].end) : at;
            const double off = std::fabs (cur->pos - expected);
            const bool wrap = before ? follower (cur->slice) <= cur->slice : cur->wrapped;
            const bool sameStem = character == cur->character;
            if (sameStem && (wrap ? off < kResyncFrames : off <= kReadOnFrames))
            {
                ++stats_.readOns;
                if (before)
                {
                    cur->earlySlice = j;
                    cur->earlyFollows = follows;
                    cur->earlyThrow = cur->thrown;
                    takeThrow (cur->earlyThrow, note);
                }
                else
                {
                    cur->seen = true;
                    cur->follows = follows;
                    takeThrow (cur->thrown, note);
                }
                return;
            }
            // A frame or two off at the cycle's wrap: re-sync over 1 ms. Further, or the other stem: a jump.
            const bool resync = sameStem && wrap && off <= kReadOnFrames;
            ++(resync ? stats_.resyncs : stats_.jumps);
            crossfade (*cur, note, j, at, follows, resync ? kResyncFadeUs : kJumpFadeUs);
            return;
        }
        // A head that reads on into another slice than this note's: the newest such jumps to it.
        Head* onward = nullptr;
        for (auto& h : heads)
            if (h.on && h.timeOutLength <= 0.0 && h.delay <= 0.0 && (! h.seen || h.follows)
                && (onward == nullptr || h.order > onward->order))
                onward = &h;
        if (onward != nullptr)
        {
            ++stats_.jumps;
            crossfade (*onward, note, j, at, follows, kJumpFadeUs);
            return;
        }
        // A new run, over whatever plays out (a slice without a follower fades on its own schedule).
        runFrom (note, j, follows, ! steady);
    }

    // ------------------------------------------------------------------ the sound

    void PhraseVoice::read (const Head& h, double& left, double& right) const noexcept
    {
        const auto& v = h.view;
        if (unit)
        {
            auto k = static_cast<std::int64_t> (h.pos); // whole frames at 48 kHz
            if (setup.looped)
                k = wrapInto (k, v.frames); // a wrapping slice runs past the stem's end
            if (k < 0 || k >= v.frames)
            {
                left = right = 0.0;
                return;
            }
            left = static_cast<double> (v.left[k]);
            right = h.stereo ? static_cast<double> (v.right[k]) : left;
            return;
        }
        auto i = static_cast<std::int64_t> (std::floor (h.pos));
        float c[SincBank::kTaps];
        sincCoefficients (sinc, h.pos - static_cast<double> (i), c);
        if (setup.looped)
            i = wrapInto (i, v.frames);
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
            bool thrown = false;
            for (auto& h : heads)
            {
                if (! h.on)
                    continue;
                if (h.delay > 0.0) // anchored on its onset: it hasn't started yet
                {
                    h.delay -= 1.0;
                    continue;
                }

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
                    h.pos += static_cast<double> (sl[next].start - sl[h.slice].end); // a wrap goes back by the stem's frames
                    h.slice = next;
                    h.wrapped = wraps;
                    h.posIn = false;
                    if (! fading && h.earlySlice == next)
                    {
                        h.seen = true;
                        h.follows = h.earlyFollows;
                        h.thrown = h.earlyThrow;
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

                // Into a slice whose note doesn't come: fade over 1 ms.
                if (! h.seen && h.timeOutLength <= 0.0 && h.pos - static_cast<double> (s.start) > kReadOnFrames)
                {
                    fadeOutBy (h, static_cast<double> (kMissedFadeUs) * samplesPerUs);
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
                        g *= raisedIn (k, h.inFrames);
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
                if (h.thrown.space > 0.0f || h.thrown.echo > 0.0f || h.spaceNow > 0.0 || h.echoNow > 0.0)
                {
                    h.spaceNow += std::clamp (static_cast<double> (h.thrown.space) - h.spaceNow, -throwStep, throwStep);
                    h.echoNow += std::clamp (static_cast<double> (h.thrown.echo) - h.echoNow, -throwStep, throwStep);
                    const double mono = 0.5 * (l + r);
                    sumSpace += mono * h.spaceNow;
                    sumEcho += mono * h.echoNow;
                    thrown = true;
                }
                if (h.thrown.left > 0.0 && (h.thrown.left -= 1.0) <= 0.0)
                    h.thrown = Throw {}; // the thrown note has ended

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
            if (thrown)
            {
                const double lane = laneSend != nullptr ? static_cast<double> (laneSend[i]) : 1.0;
                space[i] += static_cast<float> (sumSpace * lane);
                echo[i] += static_cast<float> (sumEcho * lane);
            }
        }
    }
}
