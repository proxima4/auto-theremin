#pragma once
// 1950s sci-fi laboratory look: cream enamel, chrome, bakelite, green phosphor CRTs, valve glow.
#include <juce_gui_basics/juce_gui_basics.h>

namespace at::ui
{
namespace col
{
    const juce::Colour enamel     { 0xffece2c6 };
    const juce::Colour enamelDark { 0xffd3c39d };
    const juce::Colour ink        { 0xff2b2722 };
    const juce::Colour inkSoft    { 0xff5d554a };
    const juce::Colour cherry     { 0xffb5332a };
    const juce::Colour cherryDark { 0xff7d1f19 };
    const juce::Colour mint       { 0xff7cc4b2 };
    const juce::Colour mintDark   { 0xff3f7d70 };
    const juce::Colour phosphor   { 0xff86ffa6 };
    const juce::Colour phosphorDim{ 0xff2f8a52 };
    const juce::Colour crtBlack   { 0xff031208 };
    const juce::Colour amber      { 0xffffa040 };
    const juce::Colour bakelite   { 0xff241512 };
}

struct Fonts
{
    Fonts();
    juce::Typeface::Ptr logo, label, mono, sign;
    juce::Font labelFont (float h) const { return juce::Font (juce::FontOptions (label).withHeight (h)); }
    juce::Font monoFont (float h) const  { return juce::Font (juce::FontOptions (mono).withHeight (h)); }
    juce::Font logoFont (float h) const  { return juce::Font (juce::FontOptions (logo).withHeight (h)); }
    juce::Font signFont (float h) const  { return juce::Font (juce::FontOptions (sign).withHeight (h)); }
};
const Fonts& fonts();

/** Procedural textures, generated once. */
struct Textures
{
    juce::Image enamel (int w, int h);       // speckled cream enamel with gentle wear
    juce::Image scanlines (int w, int h);
private:
    juce::Image enamelImg, scanImg;
};

// drawing helpers
void drawChromeBezel (juce::Graphics&, juce::Rectangle<float> r, float corner, float thickness);
void drawRivet (juce::Graphics&, juce::Point<float> c, float radius);
void drawGlowPath (juce::Graphics&, const juce::Path&, juce::Colour, float width, float glow = 1.0f);
void drawSectionPlate (juce::Graphics&, juce::Rectangle<float> r, const juce::String& text);
void drawEngravedBox (juce::Graphics&, juce::Rectangle<float> r, float corner);
void drawCrtGlass (juce::Graphics&, juce::Rectangle<float> r, float corner);
void drawLamp (juce::Graphics&, juce::Point<float> c, float radius, juce::Colour, bool on);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool sep, bool active, bool highlighted,
                            bool ticked, bool sub, const juce::String& text, const juce::String& shortcut,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
};
} // namespace at::ui
