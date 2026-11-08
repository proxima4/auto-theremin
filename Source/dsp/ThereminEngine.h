#pragma once
// Auto Theremin sound engine: one continuous voice, like the real instrument.
//
//   pitch target -> correction -> glide -> vibrato -> heterodyne oscillator (4x oversampled)
//   -> valve stage -> formant + brightness -> volume (antenna) -> tape echo -> plate/hall reverb
#include <juce_dsp/juce_dsp.h>
#include <random>
#include "PitchTools.h"

namespace at
{
// ------------------------------------------------------------------ small building blocks
struct OnePole
{
    float z = 0.0f, a = 0.0f;
    void setTime (float ms, float sr) { a = ms <= 0.01f ? 0.0f : std::exp (-1.0f / (0.001f * ms * sr)); }
    float process (float x) { z = x + a * (z - x); return z; }
    void reset (float v) { z = v; }
};

/** Topology-preserving state variable filter (Zavalishin). */
struct Svf
{
    float g = 0, k = 1, ic1 = 0, ic2 = 0;
    float lp = 0, bp = 0, hp = 0;
    void set (float fc, float q, float sr)
    {
        fc = juce::jlimit (10.0f, 0.45f * sr, fc);
        g = std::tan (juce::MathConstants<float>::pi * fc / sr);
        k = 1.0f / q;
    }
    void process (float x)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + g * a1 * v3;
        const float v2 = ic2 + g * v1;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

/** Heterodyne beat oscillator.
    Two coupled RF oscillators pull on each other; the difference phase psi follows
    Adler's equation  dpsi/dt = w (1 - r sin psi).  Solved in closed form it gives the
    lopsided, slightly reedy waveform of a real theremin: a pure sine at r = 0, growing
    harmonics as the coupling r rises. The phase theta runs at exactly the played pitch. */
struct HeterodyneOsc
{
    double phase = 0.0;

    inline float process (double inc, float r, float detect)
    {
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
        const float s = std::sqrt (1.0f - r * r);
        const float h = juce::MathConstants<float>::pi * (float) phase; // theta / 2
        const float x = std::cos (h);
        const float y = r * x + s * std::sin (h);
        const float inv = 1.0f / (x * x + y * y);
        const float sinPsi = 2.0f * x * y * inv;
        const float cosPsi = (x * x - y * y) * inv;
        return (1.0f - detect) * sinPsi + detect * cosPsi;
    }
};

/** Slightly asymmetric triode-like soft clip (adds even harmonics). */
struct Valve
{
    float k = 1, bias = 0, offs = 0, norm = 1;
    void set (float warmth)
    {
        k = 0.6f + 5.0f * warmth;
        bias = 0.28f * warmth;
        offs = std::tanh (k * bias);
        const float p = std::abs (std::tanh (k * (1.0f + bias)) - offs);
        const float n = std::abs (std::tanh (k * (-1.0f + bias)) - offs);
        norm = 1.0f / juce::jmax (p, n);
    }
    float process (float x) const { return (std::tanh (k * (x + bias)) - offs) * norm; }
};

struct DcBlock
{
    float x1 = 0, y1 = 0, R = 0.999f;
    void set (float sr) { R = 1.0f - (2.0f * juce::MathConstants<float>::pi * 12.0f / sr); }
    float process (float x) { const float y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
};

/** Fractional delay line with 4-point Hermite interpolation. */
struct DelayLine
{
    std::vector<float> buf;
    int w = 0, mask = 0;
    void prepare (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (float delay) const
    {
        const float rp = (float) w - 1.0f - delay;
        int i = (int) std::floor (rp);
        const float f = rp - (float) i;
        auto at = [&] (int j) { return buf[(size_t) ((i + j) & mask)]; };
        const float xm1 = at (-1), x0 = at (0), x1 = at (1), x2 = at (2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    float readInt (int delay) const { return buf[(size_t) ((w - 1 - delay) & mask)]; }
};

// ------------------------------------------------------------------ tape echo
struct TapeEcho
{
    DelayLine l, r;
    OnePole timeSmooth;
    Svf toneL, toneR, lowL, lowR;
    float sr = 48000, wowPhase = 0, flutPhase = 0;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        l.prepare ((int) (sr * 2.6f)); r.prepare ((int) (sr * 2.6f));
        timeSmooth.setTime (180.0f, sr);
        timeSmooth.reset (0.3f * sr);
        toneL.set (3600, 0.6f, sr); toneR.set (3600, 0.6f, sr);
        lowL.set (140, 0.6f, sr);  lowR.set (140, 0.6f, sr);
    }
    void reset() { l.clear(); r.clear(); toneL.reset(); toneR.reset(); lowL.reset(); lowR.reset(); }

    /** Ping-pong tape echo. time in samples. */
    inline void process (float in, float timeSamples, float fb, float mix, float& outL, float& outR)
    {
        const float t = timeSmooth.process (timeSamples);
        wowPhase += 0.55f / sr;  if (wowPhase > 1) wowPhase -= 1;
        flutPhase += 6.3f / sr;  if (flutPhase > 1) flutPhase -= 1;
        const float mod = 0.0011f * sr * std::sin (kTwoPi () * wowPhase)
                        + 0.00012f * sr * std::sin (kTwoPi () * flutPhase);
        const float d = juce::jlimit (8.0f, (float) l.mask - 8.0f, t + mod);
        float el = l.read (d), er = r.read (d);
        toneL.process (el); lowL.process (toneL.lp); el = toneL.lp - lowL.lp;
        toneR.process (er); lowR.process (toneR.lp); er = toneR.lp - lowR.lp;
        l.push (std::tanh (in + fb * er * 1.05f));   // left feeds right, right feeds left
        r.push (std::tanh (fb * el * 1.05f));
        outL = mix * el;
        outR = mix * er;
    }
    static constexpr float kTwoPi() { return 6.28318530718f; }
};

// ------------------------------------------------------------------ FDN reverb (8 lines)
struct FdnReverb
{
    static constexpr int N = 8;
    DelayLine lines[N], pre;
    float lenMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.9f, 61.3f, 67.9f, 79.1f };
    float len[N] {}, gain[N] {}, damp[N] {};
    OnePole lp[N];
    DelayLine ap[4];
    int apLen[4] {};
    float sr = 48000, lfo = 0;
    float curSize = -1, curDamp = -1;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (auto& d : lines) d.prepare ((int) (sr * 0.2f));
        pre.prepare ((int) (sr * 0.1f));
        const float apMs[4] = { 4.7f, 3.6f, 2.4f, 1.3f };
        for (int i = 0; i < 4; ++i) { apLen[i] = (int) (apMs[i] * 0.001f * sr); ap[i].prepare (apLen[i] + 4); }
        curSize = curDamp = -1;
    }
    void reset() { for (auto& d : lines) d.clear(); pre.clear(); for (auto& a : ap) a.clear(); for (auto& f : lp) f.reset (0); }

    void set (float size, float dampAmt)
    {
        if (std::abs (size - curSize) < 1e-4f && std::abs (dampAmt - curDamp) < 1e-4f) return;
        curSize = size; curDamp = dampAmt;
        const float scale = 0.55f + 1.35f * size;
        const float rt60 = 0.7f + 8.5f * size * size;
        const float fc = 16000.0f * std::pow (900.0f / 16000.0f, dampAmt);
        for (int i = 0; i < N; ++i)
        {
            len[i] = lenMs[i] * 0.001f * sr * scale;
            gain[i] = std::pow (10.0f, -3.0f * (len[i] / sr) / rt60);
            lp[i].a = std::exp (-2.0f * juce::MathConstants<float>::pi * fc / sr);
        }
    }

    inline void process (float in, float mix, float& outL, float& outR)
    {
        pre.push (in);
        float x = pre.readInt ((int) (0.022f * sr));
        for (int i = 0; i < 4; ++i)            // input diffusion
        {
            const float dly = ap[i].readInt (apLen[i] - 1);
            const float v = x + 0.62f * dly;
            ap[i].push (v);
            x = dly - 0.62f * v;
        }
        lfo += 0.31f / sr; if (lfo > 1) lfo -= 1;
        float y[N];
        for (int i = 0; i < N; ++i)
        {
            const float m = (i < 4) ? 0.0007f * sr * std::sin (6.2831853f * (lfo + 0.25f * (float) i)) : 0.0f;
            y[i] = lp[i].process (lines[i].read (len[i] + m)) * gain[i];
        }
        // fast Walsh-Hadamard mix
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h * 2)
                for (int j = i; j < i + h; ++j)
                { const float a = y[j], b = y[j + h]; y[j] = a + b; y[j + h] = a - b; }
        const float norm = 0.35355339f;
        for (int i = 0; i < N; ++i) lines[i].push (y[i] * norm + (i & 1 ? x : -x) * 0.5f);
        const float L = y[0] - y[2] + y[4] - y[6] + y[1];
        const float R = y[1] - y[3] + y[5] - y[7] - y[2];
        outL = mix * L * 0.22f;
        outR = mix * R * 0.22f;
    }
};

// ------------------------------------------------------------------ engine
struct EngineParams
{
    float glideMs = 30, correction = 0, vibDepthCents = 18, vibRate = 5.6f;
    bool smartVibrato = true;
    float timbre = 0.35f, detect = 0.3f, warmth = 0.3f, formant = 0.35f, brightness = 0.0f;
    float responseMs = 18, echoSamples = 20000, echoFb = 0.35f, echoMix = 0.25f;
    float verbSize = 0.55f, verbDamp = 0.4f, verbMix = 0.3f, outGainDb = 0;
    int key = 0; uint16_t scaleMask = 0x0FFF;
    float extraVibrato = 0;    // from mod wheel
};

class ThereminEngine
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = (float) sampleRate;
        os = std::make_unique<juce::dsp::Oversampling<float>> (1, 2,
                 juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os->initProcessing ((size_t) maxBlock);
        osBuf.setSize (1, maxBlock);
        dc.set (sr);
        echo.prepare (sr);
        verb.prepare (sr);
        formLo.reset(); formHi.reset(); bright.reset();
        pitchSmooth.reset (60.0f); volSmooth.reset (0); vibAmt.reset (0);
        lastTarget = 60.0f;
        rng.seed (0xA7);
    }

    void reset() { echo.reset(); verb.reset(); os->reset(); volSmooth.reset (0); }

    int getLatencySamples() const { return os ? (int) std::round (os->getLatencyInSamples()) : 0; }

    /** pitchTarget / volTarget: one value per sample (MIDI note units, 0..1). */
    void process (float* outL, float* outR, int n, const float* pitchTarget, const float* volTarget,
                  const EngineParams& p)
    {
        pitchSmooth.setTime (p.glideMs, sr);
        volSmooth.setTime (p.responseMs, sr);
        valve.set (p.warmth);
        formLo.set (880.0f, 2.2f, sr);
        formHi.set (2650.0f, 3.2f, sr);
        const float bAbs = std::abs (p.brightness);
        if (p.brightness < 0) bright.set (18000.0f * std::pow (650.0f / 18000.0f, bAbs), 0.62f, sr);
        else                  bright.set (2200.0f, 0.62f, sr);
        verb.set (p.verbSize, p.verbDamp);
        const float r = juce::jlimit (0.0f, 0.93f, p.timbre * 0.93f);
        const float outGain = juce::Decibels::decibelsToGain (p.outGainDb);
        const float vibUp = std::exp (-1.0f / (0.42f * sr)), vibDown = std::exp (-1.0f / (0.06f * sr));

        // ---- control-rate pitch path, per sample
        float* osIn = osBuf.getWritePointer (0);
        for (int i = 0; i < n; ++i)
        {
            const float corrected = correctPitch (pitchTarget[i], p.correction, p.key, p.scaleMask);
            const float v = volSmooth.process (juce::jlimit (0.0f, 1.0f, volTarget[i]));
            volBuf[(size_t) i] = v;
            if (v < 0.0015f) pitchSmooth.reset (corrected);   // hand moves silently: no swoop from silence
            const float pitch = pitchSmooth.process (corrected);

            // smart vibrato: theremin players add vibrato on held notes, not mid-swoop
            const float vel = std::abs (corrected - lastTarget) * sr;      // semitones / sec
            lastTarget = corrected;
            velSmooth = velSmooth * 0.9995f + 0.0005f * vel;
            const float steady = p.smartVibrato ? 1.0f / (1.0f + velSmooth * velSmooth * 0.08f) : 1.0f;
            const float tgtAmt = steady;
            vibAmt.a = tgtAmt > vibAmt.z ? vibUp : vibDown;
            const float amt = vibAmt.process (tgtAmt);

            // humanised LFO: rate and depth wander a little, like a real hand
            if (--jitterCountdown <= 0)
            {
                jitterCountdown = (int) (sr * 0.05f);
                std::uniform_real_distribution<float> u (-1.0f, 1.0f);
                rateJit += 0.25f * (u (rng) * 0.08f - rateJit);
                depthJit += 0.25f * (u (rng) * 0.18f - depthJit);
            }
            vibPhase += p.vibRate * (1.0f + rateJit) / sr;
            if (vibPhase >= 1.0f) vibPhase -= 1.0f;
            const float depth = (p.vibDepthCents * amt + p.extraVibrato * 45.0f) * (1.0f + depthJit) * 0.01f;
            const float vib = depth * std::sin (6.2831853f * vibPhase);

            currentPitch = pitch + vib;
            incBuf[(size_t) i] = noteToHz (currentPitch) / (sr * 4.0f);
            osIn[i] = 0;
        }

        // ---- oscillator + valve at 4x
        juce::dsp::AudioBlock<float> block (osBuf.getArrayOfWritePointers(), 1, (size_t) n);
        auto up = os->processSamplesUp (block);
        float* u = up.getChannelPointer (0);
        const int upN = (int) up.getNumSamples();
        for (int i = 0; i < upN; ++i)
        {
            const double inc = incBuf[(size_t) (i >> 2)];
            u[i] = valve.process (0.92f * osc.process (inc, r, p.detect));
        }
        os->processSamplesDown (block);

        // ---- tone, volume, space
        const float* sig = osBuf.getReadPointer (0);
        for (int i = 0; i < n; ++i)
        {
            float x = dc.process (sig[i]);
            formLo.process (x); formHi.process (x);
            x = x * (1.0f - 0.55f * p.formant) + p.formant * (1.9f * formLo.bp + 1.1f * formHi.bp);
            bright.process (x);
            x = p.brightness < 0 ? bright.lp : x + p.brightness * 1.3f * bright.hp;

            const float v = volBuf[(size_t) i];
            currentVol = v;
            x *= v * v * 0.27f;

            float eL, eR, rL, rR;
            echo.process (x, p.echoSamples, p.echoFb, p.echoMix, eL, eR);
            verb.process (x + 0.5f * (eL + eR), p.verbMix, rL, rR);
            outL[i] = safety ((x + eL + rL) * outGain);
            outR[i] = safety ((x + eR + rR) * outGain);
        }
    }

    /** Transparent below -2 dBFS, then a soft knee that never exceeds 0 dBFS. */
    static inline float safety (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.8f) return x;
        return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
    }

    float getCurrentPitch() const { return currentPitch; }
    float getCurrentVolume() const { return currentVol; }

    void ensureCapacity (int n) { if ((int) incBuf.size() < n) incBuf.resize ((size_t) n); if ((int) volBuf.size() < n) volBuf.resize ((size_t) n); }

private:
    float sr = 48000;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::AudioBuffer<float> osBuf;
    std::vector<double> incBuf = std::vector<double> (8192);
    std::vector<float> volBuf = std::vector<float> (8192);
    HeterodyneOsc osc;
    Valve valve;
    DcBlock dc;
    Svf formLo, formHi, bright;
    OnePole pitchSmooth, volSmooth, vibAmt;
    TapeEcho echo;
    FdnReverb verb;
    float vibPhase = 0, lastTarget = 60, velSmooth = 0, rateJit = 0, depthJit = 0;
    int jitterCountdown = 0;
    std::minstd_rand rng;
    float currentPitch = 60, currentVol = 0;
};
} // namespace at
