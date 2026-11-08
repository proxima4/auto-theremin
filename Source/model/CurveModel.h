#pragma once
// The Auto Theremin pattern: a pitch lane and a volume lane made of nodes,
// like a PC fan curve or an FL Studio automation clip.
#include <juce_core/juce_core.h>
#include <vector>

namespace at
{
enum class Shape : int
{
    Smooth = 0,   // S-curve ease from node to node (tension bends it)
    Linear,       // straight line (tension bends it like FL automation)
    Hold,         // stay on this value, jump at the next node
    Swoop         // hold, then a theremin-style slide into the next node (tension = slide length)
};

inline const char* shapeName (Shape s)
{
    switch (s) { case Shape::Smooth: return "Smooth"; case Shape::Linear: return "Linear";
                 case Shape::Hold: return "Hold"; case Shape::Swoop: return "Swoop"; }
    return "";
}

struct Node
{
    double beat = 0;      // position in quarter-note beats from the pattern start
    float value = 0;      // pitch lane: MIDI note (fractional); volume lane: 0..1
    Shape shape = Shape::Smooth;
    float tension = 0;    // -1..1
    bool selected = false; // editor selection only (not saved)
};

struct Lane
{
    std::vector<Node> nodes;
    float defaultValue = 0;

    /** Value at a beat. hint caches the segment index for fast sequential reads. */
    float evaluate (double beat, int& hint) const;
    static float shapeSegment (float t, Shape s, float tension);

    int insert (Node n);                       // keeps nodes sorted, returns index
    void removeRange (double b0, double b1);   // removes nodes with b0 <= beat <= b1
    void sort();
    juce::String toString() const;
    void fromString (const juce::String&);
};

struct Pattern
{
    Lane pitch, volume;
    int lengthBars = 4;
    int beatsPerBar = 4;
    bool loop = true;

    Pattern() { pitch.defaultValue = 60.0f; volume.defaultValue = 0.0f; }
    double lengthBeats() const { return (double) lengthBars * beatsPerBar; }

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement&);
    void clear() { pitch.nodes.clear(); volume.nodes.clear(); }

    /** Copy without reallocating when capacity allows (safe on the audio thread
        once both sides have reserved capacity). */
    void copyFrom (const Pattern& o);
    void reserve (size_t n) { pitch.nodes.reserve (n); volume.nodes.reserve (n); }
};

/** Vertical Ramer-Douglas-Peucker: reduce a dense drawn/recorded trace to nodes. */
std::vector<Node> simplifyTrace (const std::vector<std::pair<double, float>>& pts, float epsilon,
                                 Shape shape = Shape::Linear);
} // namespace at
