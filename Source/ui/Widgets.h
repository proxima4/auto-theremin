#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

namespace at { class Processor; }

namespace at::ui
{
/** Chrome-and-bakelite knob with an engraved label; shows its value while you touch it. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label,
          const juce::String& tooltip, bool bipolar = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    juce::String label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

/** Glowing valve: the filament brightens with the output volume. */
class Valve : public juce::Component
{
public:
    void setLevel (float l) { if (std::abs (l - level) > 0.004f) { level = l; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    float level = 0;
};

/** Round green CRT oscilloscope with a note readout. */
class RoundScope : public juce::Component
{
public:
    explicit RoundScope (Processor& p);
    void refresh();
    void paint (juce::Graphics&) override;
private:
    Processor& proc;
    std::vector<float> ring = std::vector<float> (8192, 0.0f);
    int writePos = 0;
    std::vector<float> tmp = std::vector<float> (8192);
    juce::Path trace;
    float pitch = 60, vol = 0;
    void buildTrace();
};

/** Stamped maker's plate. */
class Badge : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
};
} // namespace at::ui
