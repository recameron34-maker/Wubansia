// The sampler (sampler.h): the queue of note events and the rules that turn a note into voices. The
// voices themselves are voice.cpp's; the pool and stealing voice_pool.cpp's.

#include "sampler/sampler.h"

#include "mix/mixer.h"
#include "sampler/selection.h"
#include "sampler/sinc_bank.h"
#include "sampler/voice_pool.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace wubansia::engine
{
    namespace
    {
        enum class EventKind : std::uint8_t {
            noteOn,
            noteOff,
            releaseAll,
            fadeOut,
            fadeKit
        };

        struct Event
        {
            EventKind kind = EventKind::noteOn;
            Source source = Source::sequencer;
            std::int32_t offset = 0;
            std::int32_t instrument = -1;
            std::int32_t key = 0;
            std::int32_t fadeSamples = 0;
            const Kit* kit = nullptr;
            NoteOn note;
        };

        bool chokes (ChokeRole role) noexcept { return role == ChokeRole::chokes || role == ChokeRole::both; }
        bool choked (ChokeRole role) noexcept { return role == ChokeRole::choked || role == ChokeRole::both; }
    }

    struct Sampler::Impl
    {
        double rate = 48000.0;
        double baseRatio = 1.0;
        bool unitBase = true;
        int maxBlock = 0;
        std::int64_t now = 0; // samples rendered since prepare () or reset ()
        std::uint64_t order = 0;
        int dropped = 0;
        double laneStep = 0.0; // the most a lane gain moves in a sample
        VoicePool pool;
        std::vector<Event> events; // capacity kMaxEvents from prepare (); never grows
        std::array<float, kLanes> laneTarget {};
        std::array<float, kLanes> laneGain {};
        std::vector<float> laneSend; // [lane][sample of the piece]

        double samples (std::int64_t us) const noexcept { return samplesOf (us, rate); }

        void push (const Event& e) noexcept
        {
            if (events.size() >= events.capacity())
            {
                ++dropped;
                return;
            }
            events.push_back (e); // within the capacity: no allocation
        }

        // The queue in offset order, ties in the order queued (an insertion sort: it is nearly sorted).
        void sortEvents() noexcept
        {
            for (std::size_t i = 1; i < events.size(); ++i)
            {
                if (events[i].offset >= events[i - 1].offset)
                    continue;
                const Event e = events[i];
                std::size_t j = i;
                while (j > 0 && events[j - 1].offset > e.offset)
                {
                    events[j] = events[j - 1];
                    --j;
                }
                events[j] = e;
            }
        }

        void fillLaneSends (int n) noexcept
        {
            const auto stride = static_cast<std::size_t> (maxBlock);
            for (std::size_t lane = 0; lane < static_cast<std::size_t> (kLanes); ++lane)
            {
                auto g = static_cast<double> (laneGain[lane]);
                const auto target = static_cast<double> (laneTarget[lane]);
                float* out = laneSend.data() + lane * stride;
                for (int i = 0; i < n; ++i)
                {
                    g = g < target ? std::min (g + laneStep, target) : std::max (g - laneStep, target);
                    out[i] = static_cast<float> (g);
                }
                laneGain[lane] = static_cast<float> (g);
            }
        }

        void renderVoices (MixBus& bus, SendBus& sends, int from, int to) noexcept
        {
            if (from >= to)
                return;
            const auto stride = static_cast<std::size_t> (maxBlock);
            pool.forEachActive ([&] (Voice& v) {
                if (v.mixChannel >= bus.numChannels())
                {
                    v.stop();
                    return;
                }
                VoiceTarget t;
                t.left = bus.left (v.mixChannel);
                t.right = bus.right (v.mixChannel);
                t.space = sends.send (Send::space);
                t.echo = sends.send (Send::echo);
                t.laneSend = laneSend.data() + static_cast<std::size_t> (v.lane < kLanes ? v.lane : 0) * stride;
                v.render (t, from, to);
            });
        }

        void apply (const Event& e, int offset) noexcept
        {
            const std::int64_t at = now + offset;
            switch (e.kind)
            {
                case EventKind::noteOn:
                    if (e.kit != nullptr)
                        noteOn (*e.kit, e.note, offset);
                    break;
                case EventKind::noteOff:
                    for (auto& v : pool.voices)
                        if (v.isActive() && v.isKeyDown() && v.trigger == e.instrument && v.key == e.key
                            && v.source == e.source)
                            v.noteOff (at);
                    break;
                case EventKind::releaseAll:
                    for (auto& v : pool.voices)
                        if (v.isActive() && v.isKeyDown() && v.source == e.source)
                            v.noteOff (at);
                    break;
                case EventKind::fadeOut:
                    pool.forEachActive ([&] (Voice& v) {
                        if (v.source == e.source)
                            v.fade (static_cast<double> (e.fadeSamples), FadeCurve::cos2);
                    });
                    pool.stopPhrases (e.source, offset, e.fadeSamples);
                    break;
                case EventKind::fadeKit:
                {
                    const double n = samples (kKitFadeUs);
                    pool.forEachActive ([&] (Voice& v) {
                        if (v.kit == e.kit)
                            v.fade (n, FadeCurve::cos2);
                    });
                    pool.stopPhrasesOf (e.kit, offset, static_cast<int> (std::lround (n)));
                    break;
                }
            }
        }

        void noteOn (const Kit& kit, const NoteOn& note, int offset) noexcept
        {
            if (note.slice >= 0 && note.phrase >= 0)
            {
                pool.phraseNoteOn (kit, note, offset, static_cast<int> (std::lround (samples (kStealFadeUs))));
                return;
            }
            const auto count = static_cast<int> (kit.instruments.size());
            if (note.instrument < 0 || note.instrument >= count)
                return;
            startInstrument (kit, note.instrument, note.instrument, note, offset);
            const int layered = kit.instruments[static_cast<std::size_t> (note.instrument)].layeredWith;
            if (layered >= 0 && layered < count && layered != note.instrument)
                startInstrument (kit, layered, note.instrument, note, offset);
        }

        void startInstrument (const Kit& kit, int index, int trigger, const NoteOn& note, int offset) noexcept
        {
            const Instrument& inst = kit.instruments[static_cast<std::size_t> (index)];
            const std::int64_t at = now + offset;

            // Restrike-continue: struck again on the very sample its voice was released.
            if (inst.restrikeContinues && note.startFrame == 0)
                for (auto& v : pool.voices)
                    if (v.isActive() && ! v.isKeyDown() && ! v.isFading() && v.kit == &kit && v.instrument == index
                        && v.trigger == trigger && v.key == note.key && v.source == note.source && v.keyUpAt == at)
                    {
                        v.restrike();
                        return;
                    }

            // Mono: a note while the voice is held glides that voice to it.
            if (inst.mono)
                for (auto& v : pool.voices)
                    if (v.isActive() && v.isKeyDown() && ! v.isFading() && v.kit == &kit && v.instrument == index
                        && v.trigger == trigger && v.source == note.source)
                    {
                        const Glide& g = inst.glide;
                        const bool dip = note.dip && g.curve == GlideCurve::raisedCosine && g.dipUs > 0;
                        v.glide (static_cast<int> (note.key) - static_cast<int> (v.root),
                            velocityGain (inst.velocityLaw, note.velocity, v.layerVelocity), g.curve,
                            samples (dip ? g.dipUs : g.us));
                        v.key = note.key;
                        return;
                    }

            // Chokes: this note-on fades the group's choked voices, each over its own instrument's fade.
            if (inst.choke.group > 0 && chokes (inst.choke.role))
                for (auto& v : pool.voices)
                {
                    if (! v.isActive() || v.kit != &kit || v.instrument < 0)
                        continue;
                    const auto& other = kit.instruments[static_cast<std::size_t> (v.instrument)].choke;
                    if (other.group == inst.choke.group && choked (other.role))
                        v.fade (samples (other.fadeUs), fadeCurveOf (other.curve));
                }

            // A new voice: zone, layer, round robin, file.
            const auto zones = zonesOf (kit, inst);
            const int z = pickZone (zones, note.key);
            if (z < 0)
                return;
            const Zone& zone = zones[static_cast<std::size_t> (z)];
            const auto layers = layersOf (kit, zone);
            const int l = pickLayer (layers, note.velocity);
            if (l < 0)
                return;
            const Layer& layer = layers[static_cast<std::size_t> (l)];
            if (layer.rrCount == 0)
                return;
            const std::size_t rrIndex = static_cast<std::size_t> (layer.firstRr) + static_cast<std::size_t> (note.rr % layer.rrCount);
            if (rrIndex >= kit.rr.size())
                return;
            const SampleView view = kit.view (kit.rr[rrIndex]);
            if (view.left == nullptr || view.frames <= 0 || inst.mixChannel >= kMaxMixChannels)
                return;

            const int semitones = static_cast<int> (note.key) - static_cast<int> (zone.root);
            const double pitch = std::pow (2.0, semitones / 12.0);
            const double ratio = baseRatio * pitch;
            const auto frames = static_cast<double> (view.frames);
            const auto first = static_cast<double> (std::clamp<std::int64_t> (inst.startFrames, 0, view.frames - 1));

            VoiceStart s;
            s.kit = &kit;
            s.source = note.source;
            s.instrument = static_cast<std::int16_t> (index);
            s.trigger = static_cast<std::int16_t> (trigger);
            s.key = note.key;
            s.root = zone.root;
            s.layerVelocity = layer.velocity;
            s.lane = static_cast<std::uint8_t> (inst.lane);
            s.mixChannel = inst.mixChannel;
            s.sample = view;
            s.interp = inst.interp;
            s.mode = inst.mode;
            s.baseRatio = baseRatio;
            s.unitBase = unitBase;
            s.semitones = semitones;
            s.startPosition = first;
            s.attackSamples = samples (inst.attackUs);
            s.releaseSamples = samples (inst.releaseUs);
            if (inst.align == Align::end && note.lengthSamples > 0)
            {
                // The file's end on the note's end: start part-way in, or late.
                const double need = static_cast<double> (note.lengthSamples) * ratio;
                if (need <= frames - first)
                    s.startPosition = frames - need;
                else
                    s.delay = std::llround ((need - (frames - first)) / ratio);
            }
            else if (note.startFrame > 0)
            {
                s.startPosition = std::min (first + static_cast<double> (note.startFrame) * pitch, frames);
            }
            if (note.startFrame > 0)
                s.attackSamples = std::max (s.attackSamples, samples (kChaseFadeUs));
            s.velocityGain = velocityGain (inst.velocityLaw, note.velocity, layer.velocity);
            s.gain = gainOfMdb (inst.gainMdb);
            const int pan = inst.panCount > 0 && static_cast<std::size_t> (inst.firstPan) + inst.panCount <= kit.pans.size()
                                ? kit.pans[static_cast<std::size_t> (inst.firstPan) + note.panIndex % inst.panCount]
                                : 0;
            const auto gains = panGains (pan);
            s.panLeft = gains[0];
            s.panRight = gains[1];
            s.throwSpace = std::max (0.0f, note.throwSpace);
            s.throwEcho = std::max (0.0f, note.throwEcho);
            s.order = ++order;

            pool.take (samples (kStealFadeUs)).start (s);
        }
    };

    Sampler::Sampler() : impl (std::make_unique<Impl>()) {}

    Sampler::~Sampler() = default;

    void Sampler::prepare (double sampleRate, int maxBlock)
    {
        auto& m = *impl;
        m.rate = sampleRate > 0.0 ? sampleRate : 48000.0;
        m.baseRatio = static_cast<double> (kContentSampleRate) / m.rate;
        m.unitBase = std::fabs (m.rate - static_cast<double> (kContentSampleRate)) < 1.0e-9;
        m.maxBlock = std::max (1, maxBlock);
        m.events.clear();
        m.events.reserve (static_cast<std::size_t> (kMaxEvents));
        m.laneSend.assign (static_cast<std::size_t> (kLanes) * static_cast<std::size_t> (m.maxBlock), 1.0f);
        m.laneTarget.fill (1.0f);
        m.laneGain.fill (1.0f);
        m.laneStep = 1.0 / samplesOf (kLaneRampUs, m.rate);
        (void) SincBank::instance(); // built here, off the audio thread, before the phrase voices ask for it
        m.pool.prepare (m.rate); // so PhraseVoice::prepare never allocates
        m.now = 0;
        m.order = 0;
        m.dropped = 0;
    }

    void Sampler::reset() noexcept
    {
        impl->pool.reset();
        impl->events.clear();
        impl->now = 0;
        impl->laneGain = impl->laneTarget;
    }

    void Sampler::noteOn (const Kit& kit, const NoteOn& note) noexcept
    {
        Event e;
        e.kind = EventKind::noteOn;
        e.source = note.source;
        e.offset = note.offset;
        e.kit = &kit;
        e.note = note;
        impl->push (e);
    }

    void Sampler::noteOff (int instrument, int key, int offset, Source source) noexcept
    {
        Event e;
        e.kind = EventKind::noteOff;
        e.source = source;
        e.offset = offset;
        e.instrument = instrument;
        e.key = key;
        impl->push (e);
    }

    void Sampler::releaseAll (Source source, int offset) noexcept
    {
        Event e;
        e.kind = EventKind::releaseAll;
        e.source = source;
        e.offset = offset;
        impl->push (e);
    }

    void Sampler::fadeOut (Source source, int offset, int fadeSamples) noexcept
    {
        Event e;
        e.kind = EventKind::fadeOut;
        e.source = source;
        e.offset = offset;
        e.fadeSamples = std::max (0, fadeSamples);
        impl->push (e);
    }

    void Sampler::fadeKit (const Kit* kit, int offset) noexcept
    {
        Event e;
        e.kind = EventKind::fadeKit;
        e.offset = offset;
        e.kit = kit;
        impl->push (e);
    }

    bool Sampler::holdsKit (const Kit* kit) const noexcept
    {
        if (kit == nullptr)
            return false;
        bool held = false;
        impl->pool.forEachActive ([&] (const Voice& v) { held = held || v.kit == kit; });
        held = held || impl->pool.phrasesHold (kit);
        for (const auto& e : impl->events)
            held = held || (e.kind == EventKind::noteOn && e.kit == kit);
        return held;
    }

    void Sampler::setTempo (double hostBpm) noexcept
    {
        impl->pool.setPhraseTempo (hostBpm);
    }

    void Sampler::setLaneGains (const std::array<float, kLanes>& gains) noexcept
    {
        for (std::size_t i = 0; i < gains.size(); ++i)
            impl->laneTarget[i] = gains[i] > 0.0f ? gains[i] : 0.0f;
    }

    void Sampler::render (MixBus& bus, SendBus& sends, int numSamples) noexcept
    {
        auto& m = *impl;
        const int n = std::min (numSamples, m.maxBlock);
        if (n <= 0)
            return;
        m.fillLaneSends (n);
        m.sortEvents();
        int cursor = 0;
        for (const auto& e : m.events)
        {
            const int offset = std::clamp (e.offset, 0, n - 1);
            m.renderVoices (bus, sends, cursor, offset);
            cursor = std::max (cursor, offset);
            m.apply (e, offset);
        }
        m.renderVoices (bus, sends, cursor, n);
        m.pool.renderPhrases (bus, sends, n, m.laneSend.data(), static_cast<std::size_t> (m.maxBlock));
        m.events.clear();
        m.now += n;
    }

    int Sampler::activeVoices() const noexcept
    {
        return impl->pool.countActive();
    }

    int Sampler::droppedEvents() const noexcept
    {
        return impl->dropped + impl->pool.phraseSteals;
    }

    std::uint32_t Sampler::phraseLanes() const noexcept
    {
        return impl->pool.phraseLanes();
    }
}
