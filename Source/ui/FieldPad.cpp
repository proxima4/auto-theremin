#include "FieldPad.h"
#include "../PluginProcessor.h"

namespace at::ui
{
FieldPad::FieldPad (Processor& p) : proc (p) { setMouseCursor (juce::MouseCursor::CrosshairCursor); }

void FieldPad::resized() { area = getLocalBounds().toFloat().reduced (16, 14).withTrimmedBottom (20).withTrimmedTop (22); }

void FieldPad::tick()
{
    const float x = proc.liveP01.load(), y = proc.liveV01.load();
    trail.push_back ({ x, y, proc.displayVol.load() });
    if (trail.size() > 50) trail.erase (trail.begin());
    repaint();
}

void FieldPad::setFromMouse (juce::Point<float> pos)
{
    proc.fieldX.store (juce::jlimit (0.0f, 1.0f, (pos.x - area.getX()) / area.getWidth()));
    proc.fieldY.store (juce::jlimit (0.0f, 1.0f, (area.getBottom() - pos.y) / area.getHeight()));
}

void FieldPad::mouseDown (const juce::MouseEvent& e) { setFromMouse (e.position); proc.fieldActive.store (true); }
void FieldPad::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }
void FieldPad::mouseUp (const juce::MouseEvent&) { proc.fieldActive.store (false); }

void FieldPad::paint (juce::Graphics& g)
{
    auto all = getLocalBounds().toFloat();
    juce::ColourGradient bg (juce::Colour (0xff072414), all.getCentreX(), all.getCentreY(), col::crtBlack, all.getX(), all.getY(), true);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (all, 10);
    const auto green = col::phosphor;

    const int low = (int) proc.apvts.getRawParameterValue ("liveLow")->load() + 12 * proc.liveOctaveShift.load();
    const int oct = (int) proc.apvts.getRawParameterValue ("liveRange")->load();
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load();
    const int scaleIdx = (int) proc.apvts.getRawParameterValue ("scale")->load();
    const auto mask = scales()[(size_t) scaleIdx].mask;
    const int notes = oct * 12;

    // note columns
    for (int i = 0; i <= notes; ++i)
    {
        const int nn = low + i;
        const float x = area.getX() + area.getWidth() * (float) i / (float) notes;
        const int pc = ((nn % 12) + 12) % 12;
        const bool inSc = inScale (nn, key, mask);
        g.setColour (green.withAlpha (pc == 0 ? 0.42f : (scaleIdx != 0 && inSc) ? 0.2f : 0.07f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        if (pc == 0 && i < notes)
        {
            g.setFont (fonts().monoFont (13.0f));
            g.setColour (green.withAlpha (0.85f));
            g.drawText (noteName (nn), juce::Rectangle<float> (i == 0 ? x : x - 20, area.getBottom() + 3, 40, 14),
                        i == 0 ? juce::Justification::centredLeft : juce::Justification::centred);
        }
    }
    for (int i = 1; i < 4; ++i)
    {
        g.setColour (green.withAlpha (0.08f));
        g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * i / 4.0f), area.getX(), area.getRight());
    }
    g.setColour (green.withAlpha (0.35f));
    g.drawRect (area, 1.0f);

    g.setFont (fonts().monoFont (13.0f));
    g.setColour (green.withAlpha (0.7f));
    g.drawText ("HIGHER PITCH >>  " + noteName (low + notes), area.withY (area.getBottom() + 3).withHeight (14), juce::Justification::centredRight);
    g.drawText ("VOLUME ^", area.withHeight (16).translated (0, -19), juce::Justification::centredLeft);

    // trail
    auto toPt = [&] (float px, float vy) { return juce::Point<float> (area.getX() + px * area.getWidth(), area.getBottom() - vy * area.getHeight()); };
    for (size_t i = 1; i < trail.size(); ++i)
    {
        const float a = (float) i / (float) trail.size();
        const float px0 = trail[i - 1].x, px1 = trail[i].x;
        g.setColour (col::amber.withAlpha (a * 0.6f));
        g.drawLine ({ toPt (px0, trail[i - 1].y), toPt (px1, trail[i].y) }, 1.0f + 3.0f * a);
    }
    if (! trail.empty())
    {
        const auto& t = trail.back();
        const auto c = toPt (t.x, t.y);
        const float r = 6.0f + 12.0f * t.v;
        juce::ColourGradient gl (col::amber.withAlpha (0.85f), c.x, c.y, col::amber.withAlpha (0.0f), c.x + r * 2.2f, c.y, true);
        g.setGradientFill (gl);
        g.fillEllipse (c.x - r * 2.2f, c.y - r * 2.2f, r * 4.4f, r * 4.4f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (c.x - 3, c.y - 3, 6, 6);
    }

    // help text
    const auto snap = proc.gamepad().read();
    g.setColour (green.withAlpha (0.55f));
    g.setFont (fonts().monoFont (13.0f));
    const juce::String help = snap.connected
        ? juce::String::fromUTF8 ("\xe2\x9c\x95 HOLD PITCH   \xe2\x96\xb3 RECORD   \xe2\x96\xa1 RE-CENTRE TILT   D-PAD OCTAVE   OPTIONS PLAY")
        : juce::String ("DRAG HERE WITH THE MOUSE TO PLAY  -  OR CONNECT A PS5 CONTROLLER");
    g.drawText (help, area.reduced (8, 6), juce::Justification::topRight);

    if (proc.getMode() != Mode::Live)
    {
        g.setColour (col::amber.withAlpha (0.8f));
        g.setFont (fonts().monoFont (16.0f));
        g.drawText ("SWITCH TO LIVE MODE TO PLAY THE FIELD", area, juce::Justification::centred);
    }

    static Textures tex;
    g.setOpacity (0.55f);
    g.drawImageAt (tex.scanlines (getWidth(), getHeight()), 0, 0);
    g.setOpacity (1.0f);
    drawCrtGlass (g, all, 10);
}
} // namespace at::ui
