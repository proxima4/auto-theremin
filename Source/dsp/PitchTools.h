#pragma once
// Scales, Theremini-style pitch correction and note helpers.
#include <array>
#include <cmath>
#include <cstdint>
#include <juce_core/juce_core.h>

namespace at
{
constexpr uint16_t scaleMask (std::initializer_list<int> degrees)
{
    uint16_t m = 0;
    for (int d : degrees) m = (uint16_t) (m | (1u << d));
    return m;
}

struct ScaleDef { const char* name; uint16_t mask; };

inline const std::array<ScaleDef, 10>& scales()
{
    static const std::array<ScaleDef, 10> s {{
        { "Chromatic",        0x0FFF },
        { "Major",            scaleMask ({ 0, 2, 4, 5, 7, 9, 11 }) },
        { "Minor",            scaleMask ({ 0, 2, 3, 5, 7, 8, 10 }) },
        { "Harmonic Minor",   scaleMask ({ 0, 2, 3, 5, 7, 8, 11 }) },
        { "Dorian",           scaleMask ({ 0, 2, 3, 5, 7, 9, 10 }) },
        { "Phrygian",         scaleMask ({ 0, 1, 3, 5, 7, 8, 10 }) },
        { "Pentatonic Major", scaleMask ({ 0, 2, 4, 7, 9 }) },
        { "Pentatonic Minor", scaleMask ({ 0, 3, 5, 7, 10 }) },
        { "Blues",            scaleMask ({ 0, 3, 5, 6, 7, 10 }) },
        { "Whole Tone",       scaleMask ({ 0, 2, 4, 6, 8, 10 }) },
    }};
    return s;
}

inline bool inScale (int note, int key, uint16_t mask)
{
    const int deg = ((note - key) % 12 + 12) % 12;
    return (mask >> deg) & 1u;
}

/** Nearest scale notes below/above (inclusive) a fractional pitch. */
inline void scaleNeighbours (float pitch, int key, uint16_t mask, int& lo, int& hi)
{
    const int f = (int) std::floor (pitch);
    lo = f; hi = f + 1;
    for (int i = 0; i < 13; ++i) { if (inScale (f - i, key, mask)) { lo = f - i; break; } }
    for (int i = 1; i < 14; ++i) { if (inScale (f + i, key, mask)) { hi = f + i; break; } }
    if (inScale (f, key, mask) && juce::exactlyEqual (pitch, (float) f)) hi = f;
}

/** Theremini-style correction. amount 0 = untouched, 1 = hard snap.
    In between, the pitch is pulled towards scale notes but still slides. */
inline float correctPitch (float pitch, float amount, int key, uint16_t mask)
{
    if (amount <= 0.0005f) return pitch;
    int lo, hi;
    scaleNeighbours (pitch, key, mask, lo, hi);
    if (hi == lo) return pitch;
    const float span = (float) (hi - lo);
    const float u = 2.0f * (pitch - (float) lo) / span - 1.0f;       // -1..1
    const float g = 1.0f - 0.985f * juce::jlimit (0.0f, 1.0f, amount); // shape exponent
    const float f = std::copysign (std::pow (std::abs (u), g), u);
    return (float) lo + span * (0.5f + 0.5f * f);
}

inline float noteToHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

inline juce::String noteName (int note)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (n[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1);
}
} // namespace at
