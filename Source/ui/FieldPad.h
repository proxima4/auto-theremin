#pragma once
#include "LookAndFeel.h"

namespace at { class Processor; }

namespace at::ui
{
/** Live playing field: left-right is pitch (like moving towards the antenna), up-down is volume.
    Play it with the mouse, or watch your DualSense hand moving across it. */
class FieldPad : public juce::Component
{
public:
    explicit FieldPad (Processor&);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    Processor& proc;
    juce::Rectangle<float> area;
    struct Trail { float x, y, v; };
    std::vector<Trail> trail;
    void setFromMouse (juce::Point<float>);
};
} // namespace at::ui
