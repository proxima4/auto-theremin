#pragma once
// ============================================================================
//  HostStop - "Stop in FL Studio means silence."
//
//  Shared by every APT Industrial / Acid Reflux plugin. A stop is triggered by:
//    * the host transport going from playing to stopped (FL Studio's Stop)
//    * MIDI All Sound Off / All Notes Off (CC120 / CC123) while the host is
//      not playing (FL and MIDI controllers send these as a panic)
//    * requestStop() - called from reset(), when a project or preset state is
//      loaded, or by the plugin itself
//
//  On a stop the plugin's output fades to zero over ~15 ms (so there is no
//  click), then the plugin's own panic function runs once: voices killed,
//  sustain released, internal sequencers / auto modes stopped, delay and
//  reverb tails cleared. After that, sound is let through again, so you can
//  still play live from a keyboard while FL is stopped.
//
//  Usage inside processBlock:
//      hostStop.beginBlock (getPlayHead(), midi);      // first
//      ... render as normal ...
//      hostStop.endBlock (buffer, [this] { panic(); }); // last
//  and hostStop.prepare (sampleRate) in prepareToPlay.
// ============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

class HostStop
{
public:
    void prepare (double sampleRate) noexcept
    {
        fadeLength = juce::jmax (1, (int) (sampleRate * 0.015));
        fadePos = 0;
        fading = false;
        pending.store (false);
        wasPlaying = false;
        transportKnown = false;
    }

    // Thread-safe: may be called from any thread (reset(), setStateInformation()).
    void requestStop() noexcept { pending.store (true, std::memory_order_release); }

    // Call first in processBlock. Watches the host transport and incoming MIDI.
    void beginBlock (juce::AudioPlayHead* playHead, const juce::MidiBuffer& midi) noexcept
    {
        bool playing = false, valid = false;
        if (playHead != nullptr)
            if (auto pos = playHead->getPosition())
            {
                playing = pos->getIsPlaying();
                valid = true;
            }

        if (valid)
        {
            if (transportKnown && wasPlaying && ! playing)
                requestStop();
            wasPlaying = playing;
            transportKnown = true;
        }

        // While the song plays, panic CCs are left to normal note handling
        // (some hosts send them at loop points); while stopped they mean stop.
        if (! (valid && playing))
            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                if (m.isAllSoundOff() || m.isAllNotesOff())
                {
                    requestStop();
                    break;
                }
            }
    }

    bool isHostPlaying() const noexcept { return transportKnown && wasPlaying; }
    bool hasTransport() const noexcept  { return transportKnown; }   // false in stand-alone apps
    bool isStopping() const noexcept    { return fading || pending.load (std::memory_order_acquire); }

    // Call last in processBlock, on the finished output buffer.
    template <typename PanicFn>
    void endBlock (juce::AudioBuffer<float>& buffer, PanicFn&& panic)
    {
        if (! fading && pending.exchange (false, std::memory_order_acq_rel))
        {
            fading = true;
            fadePos = 0;
        }
        if (! fading)
            return;

        const int n = buffer.getNumSamples();
        const int rampLen = juce::jlimit (0, n, fadeLength - fadePos);
        const float g0 = 1.0f - (float) fadePos / (float) fadeLength;
        const float g1 = 1.0f - (float) (fadePos + rampLen) / (float) fadeLength;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            if (rampLen > 0)
                buffer.applyGainRamp (ch, 0, rampLen, g0, g1);
            if (rampLen < n)
                buffer.clear (ch, rampLen, n - rampLen);
        }

        fadePos += n;
        if (fadePos >= fadeLength)
        {
            fading = false;
            panic();
        }
    }

private:
    std::atomic<bool> pending { false };
    bool fading = false, wasPlaying = false, transportKnown = false;
    int fadeLength = 662, fadePos = 0;
};
