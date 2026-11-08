// Offline test harness: renders presets / modes to WAV and checks for NaNs, level and CPU.
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"

struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0, bpm = 110; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setIsPlaying (playing); p.setBpm (bpm); p.setPpqPosition (ppq);
        p.setTimeSignature (TimeSignature { 4, 4 }); return p;
    }
};

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? argv[1] : "/tmp/at_render");
    outDir.createDirectory();
    const double sr = 48000; const int bs = 256;
    juce::String mode = argc > 2 ? argv[2] : "presets";

    at::Processor proc;
    FakePlayHead ph;
    proc.setPlayHead (&ph);
    proc.prepareToPlay (sr, bs);
    juce::MidiBuffer midi;

    auto render = [&] (double seconds, std::function<void (int, juce::MidiBuffer&)> perBlock)
    {
        const int total = (int) (seconds * sr);
        juce::AudioBuffer<float> out (2, total), blk (2, bs);
        double cpu = 0; float peak = 0; bool bad = false;
        for (int pos = 0; pos < total; pos += bs)
        {
            const int n = juce::jmin (bs, total - pos);
            juce::AudioBuffer<float> b (blk.getArrayOfWritePointers(), 2, n);
            midi.clear();
            if (perBlock) perBlock (pos, midi);
            const auto t0 = juce::Time::getHighResolutionTicks();
            proc.processBlock (b, midi);
            cpu += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            ph.ppq += n * ph.bpm / 60.0 / sr;
            for (int c = 0; c < 2; ++c)
            {
                out.copyFrom (c, pos, b, c, 0, n);
                for (int i = 0; i < n; ++i) { const float v = b.getSample (c, i); if (! std::isfinite (v)) bad = true; peak = juce::jmax (peak, std::abs (v)); }
            }
        }
        std::printf ("    peak %.3f (%.1f dBFS)  cpu %.2f%% of realtime  %s\n", peak, juce::Decibels::gainToDecibels (peak),
                     100.0 * cpu / seconds, bad ? "NAN/INF!" : "finite");
        return out;
    };

    if (mode == "presets")
    {
        juce::AudioBuffer<float> all (2, 0);
        for (int i = 1; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            ph.ppq = 0;
            const double len = proc.getPattern().lengthBars * 4 * 60.0 / ph.bpm;
            const double secs = juce::jlimit (6.0, 18.0, len * (len < 6 ? 3 : 1)) + 1.5;
            std::printf ("%s (%.1fs)\n", proc.getProgramName (i).toRawUTF8(), secs);
            auto b = render (secs, nullptr);
            writeWav (outDir.getChildFile ("preset_" + juce::String (i) + ".wav"), b, sr);
            const int old = all.getNumSamples();
            all.setSize (2, old + b.getNumSamples(), true, true);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, old, b, c, 0, b.getNumSamples());
        }
        writeWav (outDir.getChildFile ("demo_all.wav"), all, sr);
    }
    else if (mode == "dry")
    {
        const int prog = argc > 3 ? juce::String (argv[3]).getIntValue() : 3;
        proc.setCurrentProgram (prog);
        for (auto id : { "echoMix", "verbMix" }) proc.apvts.getParameter (id)->setValueNotifyingHost (0.0f);
        for (int a = 4; a + 1 < argc; a += 2)
            proc.apvts.getParameter (argv[a])->setValueNotifyingHost (proc.apvts.getParameter (argv[a])->convertTo0to1 (juce::String (argv[a + 1]).getFloatValue()));
        ph.ppq = 0;
        std::printf ("dry %d\n", prog);
        auto b = render (9.0, nullptr);
        writeWav (outDir.getChildFile ("dry.wav"), b, sr);
    }
    else if (mode == "midi")
    {
        proc.setCurrentProgram (0);
        proc.apvts.getParameter ("mode")->setValueNotifyingHost (proc.apvts.getParameter ("mode")->convertTo0to1 (2));
        proc.apvts.getParameter ("glide")->setValueNotifyingHost (proc.apvts.getParameter ("glide")->convertTo0to1 (90));
        const int notes[] = { 60, 67, 72, 71, 67, 64, 65, 60 };
        std::printf ("MIDI legato phrase\n");
        auto b = render (9.0, [&] (int pos, juce::MidiBuffer& m)
        {
            for (int k = 0; k < 8; ++k)
            {
                const int on = (int) ((0.2 + k * 1.0) * sr), off = (int) ((0.2 + k * 1.0 + 1.05) * sr);
                if (on >= pos && on < pos + bs) m.addEvent (juce::MidiMessage::noteOn (1, notes[k], (juce::uint8) 100), on - pos);
                if (off >= pos && off < pos + bs) m.addEvent (juce::MidiMessage::noteOff (1, notes[k]), off - pos);
            }
        });
        writeWav (outDir.getChildFile ("midi.wav"), b, sr);
    }
    else if (mode == "live")
    {
        proc.setCurrentProgram (0);
        proc.apvts.getParameter ("mode")->setValueNotifyingHost (proc.apvts.getParameter ("mode")->convertTo0to1 (1));
        proc.fieldActive.store (true);
        proc.recordArmed.store (true);
        std::printf ("Live field sweep + record\n");
        auto b = render (6.0, [&] (int pos, juce::MidiBuffer&)
        {
            const double t = pos / sr;
            proc.fieldX.store ((float) (0.5 + 0.45 * std::sin (t * 1.3)));
            proc.fieldY.store (t < 5.0 ? 0.85f : 0.0f);
        });
        std::vector<at::RecPoint> pts;
        proc.readRecorded (pts);
        std::printf ("    recorded %d points, beats %.2f..%.2f\n", (int) pts.size(), pts.empty() ? 0 : pts.front().beat, pts.empty() ? 0 : pts.back().beat);
        writeWav (outDir.getChildFile ("live.wav"), b, sr);
    }
    else if (mode == "extreme")
    {
        // every float parameter at its max, then min, with a fast siren; looking for blow-ups
        proc.setCurrentProgram (6);
        for (float target : { 1.0f, 0.0f, 1.0f })
        {
            for (auto* p : proc.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    if (rp->getParameterID() != "mode" && rp->getParameterID() != "output") rp->setValueNotifyingHost (target);
            ph.ppq = 0;
            std::printf ("all params at %.0f\n", target);
            render (4.0, nullptr);
        }
    }
    return 0;
}
