#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace at
{
static const char* kIds[] = { "glide", "correction", "scale", "key", "vibDepth", "vibRate", "smartVib",
                              "timbre", "character", "warmth", "formant", "brightness", "response",
                              "echoSync", "echoDiv", "echoTime", "echoFeedback", "echoMix",
                              "verbSize", "verbDamp", "verbMix", "output",
                              "livePitch", "liveVolume", "liveLow", "liveRange", "liveInvert", "bendRange" };

juce::StringArray Processor::echoDivisionNames()
{
    return { "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/2D", "1 Bar" };
}
static const double kDivBeats[] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 3.0, 4.0 };

juce::AudioProcessorValueTreeState::ParameterLayout Processor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [] (float v, int) { return String (juce::roundToInt (v * 100)) + "%"; };
    auto ms = [] (float v, int) { return v < 1000 ? String (juce::roundToInt (v)) + " ms" : String (v / 1000.0f, 2) + " s"; };
    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def,
                     std::function<String (float, int)> txt = nullptr)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                   AudioParameterFloatAttributes().withStringFromValueFunction (txt)));
    };
    auto skewed = [] (float a, float b, float centre) { NormalisableRange<float> r (a, b); r.setSkewForCentre (centre); return r; };

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Auto", "Live", "MIDI" }, 0));

    addF ("glide", "Glide", skewed (0, 1000, 120), 30, ms);
    addF ("correction", "Pitch Correction", { 0, 1 }, 0, pct);
    StringArray sc; for (auto& s : scales()) sc.add (s.name);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", sc, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key",
               StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    addF ("vibDepth", "Vibrato Depth", { 0, 80 }, 18, [] (float v, int) { return String (roundToInt (v)) + " ct"; });
    addF ("vibRate", "Vibrato Rate", { 3, 9 }, 5.6f, [] (float v, int) { return String (v, 1) + " Hz"; });
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "smartVib", 1 }, "Expressive Vibrato", true));
    addF ("timbre", "Timbre", { 0, 1 }, 0.35f, pct);
    addF ("character", "Character", { 0, 1 }, 0.3f, pct);
    addF ("warmth", "Valve Warmth", { 0, 1 }, 0.3f, pct);
    addF ("formant", "Formant", { 0, 1 }, 0.35f, pct);
    addF ("brightness", "Brightness", { -1, 1 }, 0, [] (float v, int) { return String (roundToInt (v * 100)); });
    addF ("response", "Volume Response", skewed (1, 250, 25), 18, ms);
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "echoSync", 1 }, "Echo Sync", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "echoDiv", 1 }, "Echo Division", echoDivisionNames(), 6));
    addF ("echoTime", "Echo Time", skewed (40, 2000, 380), 380, ms);
    addF ("echoFeedback", "Echo Feedback", { 0, 0.95f }, 0.38f, pct);
    addF ("echoMix", "Echo Mix", { 0, 1 }, 0.22f, pct);
    addF ("verbSize", "Reverb Size", { 0, 1 }, 0.6f, pct);
    addF ("verbDamp", "Reverb Damping", { 0, 1 }, 0.4f, pct);
    addF ("verbMix", "Reverb Mix", { 0, 1 }, 0.3f, pct);
    addF ("output", "Output", { -24, 6 }, 0, [] (float v, int) { return String (v, 1) + " dB"; });
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "livePitch", 1 }, "Live Pitch Input",
               StringArray { "Right Stick", "Right Stick (Glide)", "Touchpad", "Gyro Tilt" }, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "liveVolume", 1 }, "Live Volume Input",
               StringArray { "R2 Trigger", "L2 Trigger", "Left Stick Up", "Touchpad Up/Down", "Always On" }, 0));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "liveLow", 1 }, "Live Lowest Note", 24, 84, 48,
               AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v); })));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "liveRange", 1 }, "Live Range (Octaves)", 1, 6, 3));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "liveInvert", 1 }, "Live Invert Pitch", false));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "bendRange", 1 }, "MIDI Bend Range", 1, 24, 2));
    return l;
}

Processor::Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AutoTheremin", createLayout())
{
    modeParam = apvts.getRawParameterValue ("mode");
    for (int i = 0; i < pCount; ++i) p[i] = apvts.getRawParameterValue (kIds[i]);
    for (auto* pat : { &uiPattern, &pendingPattern, &audioPattern }) pat->reserve (16384);
    heldNotes.reserve (128);
    loadFactory (1);
    currentProgram = 1;
}

Processor::~Processor() = default;

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int n = juce::jmax (samplesPerBlock, 32);
    engine.prepare (sampleRate, n);
    engine.ensureCapacity (n);
    pitchBuf.assign ((size_t) n, 60.0f);
    volBuf.assign ((size_t) n, 0.0f);
    outL.assign ((size_t) n, 0.0f);
    outR.assign ((size_t) n, 0.0f);
    scopeTmp.assign ((size_t) n, 0.0f);
    setLatencySamples (engine.getLatencySamples());
}

double Processor::patternLengthBeats() const
{
    return (double) uiPattern.lengthBars * hostQuarterPerBar.load();
}

void Processor::patternChanged()
{
    {
        const juce::SpinLock::ScopedLockType sl (patternLock);
        pendingPattern.copyFrom (uiPattern);
    }
    patternDirty.store (true);
}

EngineParams Processor::readParams (double bpm) const
{
    EngineParams e;
    e.glideMs = p[pGlide]->load();
    e.correction = p[pCorr]->load();
    e.scaleMask = scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, (int) p[pScale]->load())].mask;
    e.key = (int) p[pKey]->load();
    e.vibDepthCents = p[pVibD]->load();
    e.vibRate = p[pVibR]->load();
    e.smartVibrato = p[pSmart]->load() > 0.5f;
    e.timbre = p[pTimbre]->load();
    e.detect = p[pDetect]->load();
    e.warmth = p[pWarm]->load();
    e.formant = p[pForm]->load();
    e.brightness = p[pBright]->load();
    e.responseMs = p[pResp]->load();
    double secs = p[pEchoTime]->load() * 0.001;
    if (p[pEchoSync]->load() > 0.5f)
        secs = kDivBeats[juce::jlimit (0, 9, (int) p[pEchoDiv]->load())] * 60.0 / juce::jmax (20.0, bpm);
    e.echoSamples = (float) (juce::jlimit (0.02, 2.5, secs) * sr);
    e.echoFb = p[pEchoFb]->load();
    e.echoMix = p[pEchoMix]->load();
    e.verbSize = p[pVerbSize]->load();
    e.verbDamp = p[pVerbDamp]->load();
    e.verbMix = p[pVerbMix]->load();
    e.outGainDb = p[pOut]->load();
    e.extraVibrato = modWheel;
    return e;
}

void Processor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), m.getNoteNumber()), heldNotes.end());
        heldNotes.push_back (m.getNoteNumber());
        midiNote = m.getNoteNumber();
        midiVel = m.getFloatVelocity();
    }
    else if (m.isNoteOff())
    {
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), m.getNoteNumber()), heldNotes.end());
        if (! heldNotes.empty()) midiNote = heldNotes.back();
    }
    else if (m.isPitchWheel())
        midiBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * p[pBendRange]->load();
    else if (m.isController())
    {
        if (m.getControllerNumber() == 1) modWheel = m.getControllerValue() / 127.0f;
        else if (m.getControllerNumber() == 11 || m.getControllerNumber() == 7) expression = m.getControllerValue() / 127.0f;
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        heldNotes.clear();
}

void Processor::handleLiveButtons (const GamepadSnapshot& s)
{
    if (! pressInit) { std::copy (std::begin (s.pressCount), std::end (s.pressCount), lastPress); pressInit = true; return; }
    auto pressed = [&] (int b) { const bool r = s.pressCount[b] != lastPress[b]; lastPress[b] = s.pressCount[b]; return r; };
    if (pressed (BtnDpadUp))   liveOctaveShift.store (juce::jmin (3, liveOctaveShift.load() + 1));
    if (pressed (BtnDpadDown)) liveOctaveShift.store (juce::jmax (-3, liveOctaveShift.load() - 1));
    if (pressed (BtnSquare))   pad->recentreTilt();
    if (pressed (BtnTriangle)) recordArmed.store (! recordArmed.load());
    if (pressed (BtnOptions))  internalPlay.store (! internalPlay.load());
    for (int b = 0; b < BtnCount; ++b) lastPress[b] = s.pressCount[b];
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if ((int) pitchBuf.size() < n) { buffer.clear(); return; }   // host broke its promise; stay safe

    // ---- pattern hand-over from the editor (never blocks)
    if (patternDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType tl (patternLock);
        if (tl.isLocked()) { audioPattern.copyFrom (pendingPattern); patternDirty.store (false); }
    }

    // ---- transport
    bool hostPlaying = false;
    double ppq = 0, bpm = 120;
    int qpb = 4;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto v = pos->getPpqPosition()) ppq = *v;
            if (auto v = pos->getBpm()) bpm = *v;
            if (auto ts = pos->getTimeSignature())
                qpb = juce::jmax (1, juce::roundToInt (ts->numerator * 4.0 / juce::jmax (1, ts->denominator)));
        }
    hostQuarterPerBar.store (qpb);
    currentBpm.store (bpm);
    const bool internal = ! hostPlaying && internalPlay.load();
    if (internal && ! wasInternal) internalPpq = 0;
    wasInternal = internal;
    if (internal) ppq = internalPpq;
    const bool playing = hostPlaying || internal;
    const double beatsPerSample = bpm / (60.0 * sr);
    const double L = (double) audioPattern.lengthBars * qpb;
    const bool loop = audioPattern.loop;

    const Mode mode = getMode();
    const bool recording = recordArmed.load() && playing && mode != Mode::Auto;
    const int recInterval = juce::jmax (16, (int) (sr * 0.006));

    auto wrapBeat = [&] (double b) { return loop ? std::fmod (juce::jmax (0.0, b), L) : b; };

    // ---- build per-sample targets
    if (mode == Mode::Auto)
    {
        const bool hasPitch = ! audioPattern.pitch.nodes.empty();
        const bool hasVol = ! audioPattern.volume.nodes.empty();
        for (int i = 0; i < n; ++i)
        {
            if (playing && hasPitch)
            {
                const double b = wrapBeat (ppq + i * beatsPerSample);
                pitchBuf[(size_t) i] = audioPattern.pitch.evaluate (b, hintP);
                volBuf[(size_t) i] = (! loop && b >= L) ? 0.0f
                                    : hasVol ? audioPattern.volume.evaluate (b, hintV) : 0.8f;
            }
            else if (auditionOn.load (std::memory_order_relaxed))
            {
                pitchBuf[(size_t) i] = auditionPitch.load (std::memory_order_relaxed);
                volBuf[(size_t) i] = 0.75f;
            }
            else
            {
                pitchBuf[(size_t) i] = prevBlockP;
                volBuf[(size_t) i] = 0.0f;
            }
        }
    }
    else if (mode == Mode::Live)
    {
        const auto s = pad->read();
        handleLiveButtons (s);
        const float dt = (float) n / (float) sr;
        const auto src = (PitchSource) (int) p[pPitchSrc]->load();
        const auto vsrc = (VolumeSource) (int) p[pVolSrc]->load();
        const bool hold = (s.buttons >> BtnCross) & 1u;
        const bool invert = p[pInvert]->load() > 0.5f;
        float tv = 0;

        if (fieldActive.load())
        {
            tv = fieldY.load();
            liveRaw = invert ? 1.0f - fieldX.load() : fieldX.load();
        }
        else if (s.connected)
        {
            if (! hold)
            {
                switch (src)
                {
                    case SrcStick:      liveRaw = 0.5f + 0.5f * s.rx; break;
                    case SrcStickGlide:
                    {
                        const float v = std::abs (s.rx) < 0.08f ? 0.0f : s.rx;
                        liveRaw = juce::jlimit (0.0f, 1.0f, liveRaw + std::copysign (v * v, v) * 0.9f * dt);
                        break;
                    }
                    case SrcTouchpad:   if (s.touchDown) liveRaw = s.touchX; break;
                    case SrcTilt:       liveRaw = juce::jlimit (0.0f, 1.0f, 0.5f + s.tilt / 90.0f); break;
                }
            }
            switch (vsrc)
            {
                case VolR2:        tv = s.r2; break;
                case VolL2:        tv = s.l2; break;
                case VolLeftStick: tv = juce::jlimit (0.0f, 1.0f, (-s.ly - 0.08f) / 0.92f); break;
                case VolTouchY:    tv = s.touchDown ? 1.0f - s.touchY : 0.0f; break;
                case VolFull:      tv = 1.0f; break;
            }
            pad->setLight ((((uint32_t) (60 + 195 * tv)) << 16) | (((uint32_t) (25 + 140 * tv)) << 8) | (uint32_t) (10 + 40 * tv));
        }
        const float tp = invert ? 1.0f - liveRaw : liveRaw;
        liveV = tv;
        liveP01.store (tp); liveV01.store (tv);

        const float low = p[pLow]->load() + 12.0f * (float) liveOctaveShift.load();
        const float targetP = low + tp * 12.0f * p[pOct]->load();
        for (int i = 0; i < n; ++i)   // interpolate across the block: no stepping from 8-bit sticks
        {
            const float t = (float) (i + 1) / (float) n;
            pitchBuf[(size_t) i] = prevBlockP + (targetP - prevBlockP) * t;
            volBuf[(size_t) i] = prevBlockV + (tv - prevBlockV) * t;
        }
    }
    else // MIDI
    {
        auto it = midi.begin();
        for (int i = 0; i < n; ++i)
        {
            while (it != midi.end() && (*it).samplePosition <= i) { handleMidi ((*it).getMessage()); ++it; }
            pitchBuf[(size_t) i] = (float) midiNote + midiBend;
            volBuf[(size_t) i] = heldNotes.empty() ? 0.0f : (0.35f + 0.65f * midiVel) * expression;
        }
        while (it != midi.end()) { handleMidi ((*it).getMessage()); ++it; }
    }

    // ---- recording (live / midi performance written into the curve lanes)
    if (recording)
    {
        for (int i = 0; i < n; ++i)
        {
            if (++recCounter < recInterval) continue;
            recCounter = 0;
            const double b = wrapBeat (ppq + i * beatsPerSample);
            if (! loop && b >= L) continue;
            const auto scope = recFifo.write (1);
            if (scope.blockSize1 > 0) recStore[(size_t) scope.startIndex1] = { b, pitchBuf[(size_t) i], volBuf[(size_t) i] };
        }
    }

    prevBlockP = pitchBuf[(size_t) n - 1];
    prevBlockV = volBuf[(size_t) n - 1];

    // ---- render
    const auto params = readParams (bpm);
    engine.process (outL.data(), outR.data(), n, pitchBuf.data(), volBuf.data(), params);

    const int numCh = buffer.getNumChannels();
    if (numCh >= 2)
    {
        buffer.copyFrom (0, 0, outL.data(), n);
        buffer.copyFrom (1, 0, outR.data(), n);
        for (int c = 2; c < numCh; ++c) buffer.clear (c, 0, n);
    }
    else if (numCh == 1)
    {
        auto* d = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) d[i] = 0.5f * (outL[(size_t) i] + outR[(size_t) i]);
    }

    // ---- display feeds
    for (int i = 0; i < n; ++i) scopeTmp[(size_t) i] = 0.5f * (outL[(size_t) i] + outR[(size_t) i]);
    if (scopeFifo.getFreeSpace() >= n)
    {
        const auto w = scopeFifo.write (n);
        if (w.blockSize1 > 0) std::copy (scopeTmp.begin(), scopeTmp.begin() + w.blockSize1, scopeStore.begin() + w.startIndex1);
        if (w.blockSize2 > 0) std::copy (scopeTmp.begin() + w.blockSize1, scopeTmp.begin() + w.blockSize1 + w.blockSize2, scopeStore.begin() + w.startIndex2);
    }
    displayBeat.store (playing ? wrapBeat (ppq + n * beatsPerSample) : wrapBeat (ppq));
    displayPlaying.store (playing);
    displayPitch.store (engine.getCurrentPitch());
    displayVol.store (engine.getCurrentVolume());
    if (internal) internalPpq += n * beatsPerSample;
}

int Processor::readRecorded (std::vector<RecPoint>& out)
{
    const auto r = recFifo.read (recFifo.getNumReady());
    for (int i = 0; i < r.blockSize1; ++i) out.push_back (recStore[(size_t) (r.startIndex1 + i)]);
    for (int i = 0; i < r.blockSize2; ++i) out.push_back (recStore[(size_t) (r.startIndex2 + i)]);
    return r.blockSize1 + r.blockSize2;
}

int Processor::readScope (float* dest, int maxSamples)
{
    const int ready = scopeFifo.getNumReady();
    if (ready > maxSamples)   // drop the oldest so the scope stays live
    {
        const auto skip = scopeFifo.read (ready - maxSamples);
        juce::ignoreUnused (skip);
    }
    const auto r = scopeFifo.read (juce::jmin (maxSamples, scopeFifo.getNumReady()));
    std::copy (scopeStore.begin() + r.startIndex1, scopeStore.begin() + r.startIndex1 + r.blockSize1, dest);
    std::copy (scopeStore.begin() + r.startIndex2, scopeStore.begin() + r.startIndex2 + r.blockSize2, dest + r.blockSize1);
    return r.blockSize1 + r.blockSize2;
}

// ---------------------------------------------------------------- programs & state
int Processor::getNumPrograms() { return (int) factoryPresets().size(); }
const juce::String Processor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) factoryPresets().size()) ? juce::String (factoryPresets()[(size_t) i].name) : juce::String();
}

void Processor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) factoryPresets().size())) return;
    currentProgram = index;
    loadFactory (index);
    ++patternReplacedCount;
}

void Processor::loadFactory (int index)
{
    const auto& fp = factoryPresets()[(size_t) index];
    // every parameter back to default first, then the preset's own values
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "mode" && ! rp->getParameterID().startsWith ("live"))
                rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, v] : fp.params)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
    uiPattern = Pattern();
    uiPattern.reserve (16384);
    fp.build (uiPattern);
    patternChanged();
}

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    xml->addChildElement (uiPattern.toXml().release());
    xml->setAttribute ("program", currentProgram);
    xml->setAttribute ("version", 1);
    copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    if (auto* pat = xml->getChildByName ("Pattern"))
    {
        uiPattern.fromXml (*pat);
        xml->removeChildElement (pat, true);
    }
    currentProgram = xml->getIntAttribute ("program", 0);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    // Re-apply every value explicitly so switches restore exactly, whatever raw value they held.
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (auto* raw = apvts.getRawParameterValue (rp->getParameterID()))
            {
                const float norm = rp->convertTo0to1 (raw->load());
                if (std::abs (rp->getValue() - norm) > 1e-6f) rp->setValueNotifyingHost (norm);
            }
    patternChanged();
    ++patternReplacedCount;
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }
} // namespace at

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new at::Processor(); }
