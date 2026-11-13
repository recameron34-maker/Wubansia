#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "content/InstalledContent.h"

#include <algorithm>
#include <cmath>
#include <thread>

namespace wubansia
{
    PluginProcessor::PluginProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, State::rootTag, Parameters::createLayout()),
          raw (apvts),
          content (std::make_unique<InstalledContent>()),
          service (std::make_unique<KitService> (player, *content)),
          sounds (std::make_unique<SoundService> (*content, auditions)),
          controllerImpl (*this)
    {
        // Nothing else: no file is read and no thread starts until the first prepareToPlay or
        // setStateInformation (hosts and validators make instances by the dozen to scan them).
        mirrorSettings (stateOnly);
    }

    PluginProcessor::~PluginProcessor()
    {
        // The services first: they stop their threads and drop their Kits and clips while the engine and the audition
        // player still exist. ~AuditionPlayer then deletes the clips it still holds.
        sounds.reset();
        service.reset();
    }

    void PluginProcessor::setContentForTesting (std::unique_ptr<ContentSourceFactory> replacement, const juce::String& kit)
    {
        jassert (replacement != nullptr && ! service->started() && ! sounds->started());
        sounds.reset();
        service.reset();
        content = std::move (replacement);
        service = std::make_unique<KitService> (player, *content);
        sounds = std::make_unique<SoundService> (*content, auditions);
        if (kit.isNotEmpty())
        {
            const std::lock_guard lock (stateMutex);
            stateOnly.kit = kit;
        }
    }

    StateOnly PluginProcessor::settings() const
    {
        const std::lock_guard lock (stateMutex);
        return stateOnly;
    }

    void PluginProcessor::mirrorSettings (const StateOnly& s) noexcept
    {
        phrasesMirror.store (static_cast<std::uint8_t> (s.phrases), std::memory_order_relaxed);
        midiInMirror.store (static_cast<std::uint8_t> (s.midiIn), std::memory_order_relaxed);
        midiInLaneMirror.store (static_cast<std::uint8_t> (s.midiInLane), std::memory_order_relaxed);
        transport.setBpm (s.standaloneBpm);
    }

    bool PluginProcessor::changeSettings (const std::function<void (StateOnly&)>& change)
    {
        bool changed = false;
        {
            const std::lock_guard lock (stateMutex);
            auto edited = stateOnly;
            change (edited);
            changed = ! (edited == stateOnly);
            if (changed)
            {
                stateOnly = edited;
                mirrorSettings (stateOnly);
            }
        }
        if (changed)
            updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged (true));
        return changed;
    }

    int PluginProcessor::contentVersionOf (const juce::String& kit) const
    {
        const auto* entry = service->catalog().find (kit.toStdString());
        return entry != nullptr && entry->ok() ? entry->manifest.kit.contentVersion : 0;
    }

    void PluginProcessor::queueFirstLoad()
    {
        juce::String kit;
        int version = 0;
        {
            const std::lock_guard lock (stateMutex);
            if (loadQueued)
                return;
            loadQueued = true;
            kit = stateOnly.kit;
            version = stateOnly.kitContent;
        }
        service->request (kit.toStdString(), version);
    }

    //==============================================================================
    void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        const auto rate = sampleRate > 0.0 && std::isfinite (sampleRate) ? sampleRate : 48000.0;
        preparedRate = rate;
        preparedBlock = std::max (1, samplesPerBlock);
        const int outputs = std::clamp (getTotalNumOutputChannels(), 1, 2);

        player.prepare (rate, preparedBlock, outputs);
        setLatencySamples (player.latencySamples());
        auditions.prepare (rate); // clips are made for this rate from now on (ADR 0024)

        masterGain.reset (rate, 0.02); // 20 ms ramps, so gain changes never click
        masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw.masterGainDb(), Parameters::minDb));

        // A reset () or releaseResources () pending is kept: a host deactivates with them, then reactivates with this,
        // and the engine was just reset here, but a clip of the drawer's would play on mid-clip at the same rate. The
        // first block acts on it.
        bypassed = false;
        lastPlaying.store (false, std::memory_order_relaxed);
        prepared.store (true, std::memory_order_release);

        // The kits' manifests, parsed here once, so a host that saves state from its audio thread never
        // parses them there.
        (void) service->catalog();
        queueFirstLoad();
    }

    void PluginProcessor::releaseResources()
    {
        // The engine keeps its buffers for the next prepareToPlay; the voices go at the next block.
        resetPending.store (true, std::memory_order_release);
    }

    bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto out = layouts.getMainOutputChannelSet();
        return layouts.getMainInputChannelSet().isDisabled() && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
    }

    void PluginProcessor::reset()
    {
        resetPending.store (true, std::memory_order_release);
    }

    double PluginProcessor::getTailLengthSeconds() const
    {
        return player.tailSeconds();
    }

    juce::AudioProcessorEditor* PluginProcessor::createEditor()
    {
        return new PluginEditor (*this, controller());
    }

    //==============================================================================
    void PluginProcessor::waitForKitOffline() noexcept
    {
        // ADR 0005 D5: in an offline render, and only there, wait (1 ms at a time, 30 s per load at most)
        // for a kit load in flight to hand the engine its Kit, which it then adopts on this block's first
        // sample. A load that fails ends the wait at once.
        if (! isNonRealtime() || ! service->loadInFlight())
            return;
        using Clock = std::chrono::steady_clock;
        const auto load = service->loadGeneration();
        if (load != waitGeneration)
        {
            waitGeneration = load;
            waited = {};
        }
        const auto limit = std::chrono::milliseconds (waitLimitMs.load (std::memory_order_relaxed));
        const auto start = Clock::now();
        while (service->loadInFlight() && service->loadGeneration() == load && waited + (Clock::now() - start) < limit)
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        waited += Clock::now() - start;
    }

    void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        // 1.
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();
        const int numSamples = buffer.getNumSamples();
        if (! prepared.load (std::memory_order_acquire) || numSamples <= 0)
            return;
        bypassed = false;

        // 2. The host's reset (or releaseResources, even with a prepare since): every voice, tail and audition silent at once.
        if (resetPending.exchange (false, std::memory_order_acq_rel))
        {
            player.reset();
            auditions.reset();
        }

        // 3. The offline wait.
        waitForKitOffline();

        // 4. The controls: the parameters, and the state-only settings' mirrors.
        raw.read (controls);
        controls.phrases = static_cast<engine::PhraseMode> (phrasesMirror.load (std::memory_order_relaxed));
        controls.midiIn = static_cast<engine::MidiInMode> (midiInMirror.load (std::memory_order_relaxed));
        controls.midiInLane = midiInLaneMirror.load (std::memory_order_relaxed);

        // 5. The transport: the host's, or the plugin's own.
        juce::Optional<juce::AudioPlayHead::PositionInfo> position;
        if (auto* head = getPlayHead())
            position = head->getPosition();
        const auto block = transport.resolve (position, isNonRealtime());

        // 6. The MIDI: channel messages of up to 3 bytes, in order; no SysEx, no MidiMessage.
        int numMidi = 0;
        for (const auto event : midi)
        {
            if (numMidi == kMaxMidiEvents)
                break;
            const auto* data = event.data;
            if (data == nullptr || event.numBytes < 1 || event.numBytes > 3 || data[0] < 0x80 || data[0] >= 0xF0)
                continue;
            auto& e = midiEvents[static_cast<std::size_t> (numMidi++)];
            e.offset = std::clamp (event.samplePosition, 0, numSamples - 1);
            e.status = data[0];
            e.data1 = event.numBytes > 1 ? data[1] : 0;
            e.data2 = event.numBytes > 2 ? data[2] : 0;
        }

        // 7. The engine, in pieces no longer than the prepared block; the host's position moves on with them.
        const int outputs = std::min ({ buffer.getNumChannels(), getTotalNumOutputChannels(), 2 });
        if (outputs > 0)
        {
            auto info = block.info;
            const double startPpq = info.ppq;
            int done = 0;
            int next = 0;
            while (done < numSamples)
            {
                const int n = std::min (preparedBlock, numSamples - done);
                int end = next;
                while (end < numMidi && midiEvents[static_cast<std::size_t> (end)].offset < done + n)
                    midiEvents[static_cast<std::size_t> (end++)].offset -= done;
                float* out[2] { buffer.getWritePointer (0, done), outputs > 1 ? buffer.getWritePointer (1, done) : nullptr };
                player.process (out, outputs, n, info, controls, midiEvents.data() + next, end - next);
                next = end;
                done += n;

                if (done < numSamples && info.hasPosition && info.playing && info.bpm > 0.0)
                {
                    auto ppq = startPpq + static_cast<double> (done) / preparedRate * info.bpm / 60.0;
                    if (info.looping && startPpq < info.loopEndPpq && ppq >= info.loopEndPpq)
                        ppq = info.loopStartPpq + std::fmod (ppq - info.loopStartPpq, info.loopEndPpq - info.loopStartPpq);
                    info.ppq = ppq;
                }
            }
        }

        // 7b. The Samples drawer's audition (ADR 0024): dry, on top of the engine, under master_gain; never in an
        //     offline render. With nothing playing it touches nothing. It keeps no buffers, so it takes the whole block.
        if (outputs > 0)
        {
            float* out[2] { buffer.getWritePointer (0), outputs > 1 ? buffer.getWritePointer (1) : nullptr };
            auditions.process (out, outputs, numSamples, isNonRealtime());
        }
        else
        {
            auditions.process (nullptr, 0, numSamples, true); // no output to play into: let every clip go
        }

        // 8. master_gain, through M0's smoother.
        masterGain.setTargetValue (juce::Decibels::decibelsToGain (raw.masterGainDb(), Parameters::minDb));
        masterGain.applyGain (buffer, numSamples);

        // 9. What the editor shows of the transport (the engine keeps the rest of the telemetry).
        lastPlaying.store (block.info.playing, std::memory_order_relaxed);
        lastInternal.store (block.internal, std::memory_order_relaxed);
        lastBpm.store (block.info.bpm, std::memory_order_relaxed);
    }

    void PluginProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();
        resetPending.store (false, std::memory_order_relaxed);
        if (! bypassed && prepared.load (std::memory_order_acquire))
        {
            player.reset(); // once, when the bypass starts
            bypassed = true;
        }
        auditions.process (nullptr, 0, buffer.getNumSamples(), true); // every block: an audition offered now never plays
        lastPlaying.store (false, std::memory_order_relaxed);
    }

    //==============================================================================
    void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
    {
        const std::lock_guard io (stateIoMutex);
        const auto parameters = apvts.copyState();
        StateOnly s;
        int version = State::version;
        juce::NamedValueSet others;
        {
            const std::lock_guard lock (stateMutex);
            s = stateOnly;
            version = std::max (State::version, stateVersionRead);
            others = otherAttributes;
        }
        const int kitContent = s.kitContent > 0 ? s.kitContent : contentVersionOf (s.kit);
        State::write (destData, parameters, s, kitContent, version, others, *this);
    }

    void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
    {
        auto loaded = State::read (data, sizeInBytes, *this);
        if (! loaded)
            return; // not ours: ignored

        const std::lock_guard io (stateIoMutex);
        apvts.replaceState (loaded->parameters);

        // Every parameter exactly as the state has it. The APVTS leaves a parameter alone when its
        // value already reads the same (a switch a host set to 0.7 reads "on", like the state's 1.0),
        // but the host would then read 0.7 back (pluginval's restoration test).
        for (auto* base : getParameters())
            if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (base))
            {
                const auto child = loaded->parameters.getChildWithProperty ("id", parameter->getParameterID());
                if (! child.isValid())
                    continue;
                const auto normalised = parameter->convertTo0to1 (static_cast<float> (static_cast<double> (child["value"])));
                if (! juce::exactlyEqual (parameter->getValue(), normalised))
                    parameter->setValueNotifyingHost (normalised);
            }

        bool queue = false;
        {
            const std::lock_guard lock (stateMutex);
            queue = ! loadQueued || loaded->settings.kit != stateOnly.kit;
            loadQueued = true;
            stateOnly = loaded->settings;
            stateVersionRead = loaded->version;
            otherAttributes = loaded->otherAttributes;
            mirrorSettings (stateOnly);
        }
        if (queue)
            service->request (loaded->settings.kit.toStdString(), loaded->settings.kitContent);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new wubansia::PluginProcessor();
}
