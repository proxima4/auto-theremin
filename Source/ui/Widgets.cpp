#include "Widgets.h"
#include "../PluginProcessor.h"

namespace at::ui
{
// ---------------------------------------------------------------- Knob
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& l,
            const juce::String& tip, bool bipolar)
    : label (l)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (220);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setTooltip (tip);
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart = [this] { repaint(); };
    slider.onDragEnd = [this] { repaint(); };
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.setDoubleClickReturnValue (true, s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    slider.addMouseListener (this, false);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (16);
    slider.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromBottom (15).toFloat();
    const bool active = slider.isMouseButtonDown() || slider.isMouseOver();
    if (active)
    {
        g.setColour (col::crtBlack);
        g.fillRoundedRectangle (r.reduced (2, 0), 3);
        g.setColour (col::phosphor);
        g.setFont (fonts().monoFont (14.0f));
        g.drawText (slider.getTextFromValue (slider.getValue()), r, juce::Justification::centred);
    }
    else
    {
        g.setColour (col::ink);
        g.setFont (fonts().labelFont (8.6f).withExtraKerningFactor (0.06f));
        g.drawText (label, r, juce::Justification::centred);
    }
}

// ---------------------------------------------------------------- Valve
void Valve::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    const float w = r.getWidth();
    auto base = r.removeFromBottom (r.getHeight() * 0.2f);
    // bakelite base with pins
    g.setColour (col::bakelite);
    g.fillRoundedRectangle (base.reduced (w * 0.06f, 0), 3);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) base.getY() + 2, base.getX() + w * 0.1f, base.getRight() - w * 0.1f);

    // glass envelope
    juce::Path glass;
    const auto gr = r.reduced (w * 0.08f, 0);
    glass.startNewSubPath (gr.getX(), gr.getBottom());
    glass.lineTo (gr.getX(), gr.getY() + gr.getWidth() * 0.5f);
    glass.quadraticTo (gr.getX(), gr.getY(), gr.getCentreX(), gr.getY());
    glass.quadraticTo (gr.getRight(), gr.getY(), gr.getRight(), gr.getY() + gr.getWidth() * 0.5f);
    glass.lineTo (gr.getRight(), gr.getBottom());
    glass.closeSubPath();

    const float heat = 0.22f + 0.78f * juce::jlimit (0.0f, 1.0f, level * 1.4f);
    // inner glow
    juce::ColourGradient glow (col::amber.withAlpha (0.75f * heat), gr.getCentreX(), gr.getCentreY() + gr.getHeight() * 0.1f,
                               col::amber.withAlpha (0.0f), gr.getCentreX(), gr.getY(), true);
    g.setColour (juce::Colour (0xff1a1410).withAlpha (0.55f));
    g.fillPath (glass);
    g.setGradientFill (glow);
    g.fillPath (glass);

    // plate and grid
    auto plate = gr.reduced (gr.getWidth() * 0.26f, gr.getHeight() * 0.22f).translated (0, gr.getHeight() * 0.06f);
    g.setColour (juce::Colour (0xff4a4744));
    g.fillRect (plate.withWidth (plate.getWidth() * 0.18f));
    g.fillRect (plate.withTrimmedLeft (plate.getWidth() * 0.82f));
    g.setColour (juce::Colour (0xff6b6762));
    for (int i = 0; i < 6; ++i)
    {
        const float yy = plate.getY() + plate.getHeight() * (0.1f + 0.15f * i);
        g.drawHorizontalLine ((int) yy, plate.getX() + plate.getWidth() * 0.22f, plate.getRight() - plate.getWidth() * 0.22f);
    }
    // filament
    const auto fc = col::amber.interpolatedWith (juce::Colours::white, 0.25f * heat).withAlpha (0.35f + 0.65f * heat);
    juce::Path fil;
    fil.startNewSubPath (plate.getCentreX() - 2, plate.getBottom());
    fil.lineTo (plate.getCentreX() - 2, plate.getY() + 3);
    fil.lineTo (plate.getCentreX() + 2, plate.getY() + 3);
    fil.lineTo (plate.getCentreX() + 2, plate.getBottom());
    drawGlowPath (g, fil, fc, 1.6f, heat);

    // glass highlights
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.strokePath (glass, juce::PathStrokeType (1.0f));
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillRoundedRectangle (gr.getX() + gr.getWidth() * 0.16f, gr.getY() + gr.getWidth() * 0.3f, gr.getWidth() * 0.1f, gr.getHeight() * 0.55f, 3);
}

// ---------------------------------------------------------------- RoundScope
RoundScope::RoundScope (Processor& p) : proc (p) {}

void RoundScope::refresh()
{
    const int got = proc.readScope (tmp.data(), (int) tmp.size());
    for (int i = 0; i < got; ++i) { ring[(size_t) writePos] = tmp[(size_t) i]; writePos = (writePos + 1) & 8191; }
    pitch = proc.displayPitch.load();
    vol = proc.displayVol.load();
    buildTrace();
    repaint();
}

void RoundScope::buildTrace()
{
    trace.clear();
    auto r = getLocalBounds().toFloat().reduced (getWidth() * 0.11f);
    const double sr = juce::jmax (8000.0, proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
    const float f = noteToHz (pitch);
    const int span = juce::jlimit (32, 3000, (int) (2.0 * sr / f));
    // look back for a rising zero crossing so the trace stands still
    int start = (writePos - span - 4 + 8192) & 8191;
    for (int k = 0; k < (int) (sr / f) + 2; ++k)
    {
        const int i0 = (start - k - 1 + 8192) & 8191, i1 = (start - k + 8192) & 8191;
        if (ring[(size_t) i0] < 0 && ring[(size_t) i1] >= 0) { start = i1; break; }
    }
    float peak = 0.02f;
    for (int i = 0; i < span; ++i) peak = juce::jmax (peak, std::abs (ring[(size_t) ((start + i) & 8191)]));
    const float gain = 0.42f / juce::jmax (peak, 0.08f);
    for (int i = 0; i < span; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (span - 1);
        const float y = r.getCentreY() - ring[(size_t) ((start + i) & 8191)] * gain * r.getHeight();
        if (i == 0) trace.startNewSubPath (x, y); else trace.lineTo (x, y);
    }
}

void RoundScope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (d, d);
    // chrome ring
    juce::ColourGradient cg (juce::Colour (0xfff7f9fa), r.getX(), r.getY(), juce::Colour (0xff59616a), r.getRight(), r.getBottom(), false);
    cg.addColour (0.5, juce::Colour (0xffb9c1c7));
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (r.translated (2, 4));
    g.setGradientFill (cg);
    g.fillEllipse (r);
    auto glass = r.reduced (d * 0.07f);
    g.setColour (juce::Colour (0xff2a3036));
    g.fillEllipse (glass.expanded (2));
    juce::ColourGradient crt (juce::Colour (0xff0b2a16), glass.getCentreX(), glass.getCentreY(), col::crtBlack, glass.getX(), glass.getY(), true);
    g.setGradientFill (crt);
    g.fillEllipse (glass);

    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addEllipse (glass);
    g.reduceClipRegion (clip);
    // graticule
    g.setColour (col::phosphor.withAlpha (0.12f));
    for (int i = 1; i < 8; ++i)
    {
        const float x = glass.getX() + glass.getWidth() * i / 8.0f, y = glass.getY() + glass.getHeight() * i / 8.0f;
        g.drawVerticalLine ((int) x, glass.getY(), glass.getBottom());
        g.drawHorizontalLine ((int) y, glass.getX(), glass.getRight());
    }
    g.setColour (col::phosphor.withAlpha (0.25f));
    g.drawHorizontalLine ((int) glass.getCentreY(), glass.getX(), glass.getRight());
    g.drawVerticalLine ((int) glass.getCentreX(), glass.getY(), glass.getBottom());

    drawGlowPath (g, trace, col::phosphor, 1.7f, 0.6f + 0.4f * juce::jmin (1.0f, vol * 2));

    // readout inside the tube
    const int note = juce::roundToInt (pitch);
    const int cents = juce::roundToInt ((pitch - note) * 100.0f);
    g.setColour (col::phosphor.withAlpha (vol > 0.01f ? 0.95f : 0.35f));
    g.setFont (fonts().monoFont (d * 0.11f));
    g.drawText (noteName (note) + (cents >= 0 ? " +" : " ") + juce::String (cents),
                glass.withTrimmedTop (glass.getHeight() * 0.66f).withHeight (d * 0.12f), juce::Justification::centred);
    g.setFont (fonts().monoFont (d * 0.075f));
    g.drawText (juce::String (noteToHz (pitch), 1) + " Hz",
                glass.withTrimmedTop (glass.getHeight() * 0.78f).withHeight (d * 0.09f), juce::Justification::centred);
    drawCrtGlass (g, glass, d * 0.5f);
}

// ---------------------------------------------------------------- Badge
void Badge::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (r.translated (0.5f, 1.2f), 3);
    juce::ColourGradient cg (juce::Colour (0xffdfe3e5), r.getX(), r.getY(), juce::Colour (0xff9ba3a9), r.getRight(), r.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (r, 3);
    // brushed lines
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    for (float y = r.getY() + 2; y < r.getBottom() - 1; y += 2.0f) g.drawHorizontalLine ((int) y, r.getX() + 2, r.getRight() - 2);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (r, 3, 0.8f);
    drawRivet (g, { r.getX() + 7, r.getCentreY() }, 2.6f);
    drawRivet (g, { r.getRight() - 7, r.getCentreY() }, 2.6f);

    auto t = r.reduced (13, 3);
    auto top = t.removeFromTop (t.getHeight() * 0.56f);
    // stamped: light edge under dark letters
    g.setFont (fonts().labelFont (top.getHeight() * 0.62f).withExtraKerningFactor (0.1f));
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawText ("APT INDUSTRIAL", top.translated (0, 0.8f), juce::Justification::centred);
    g.setColour (col::ink.withAlpha (0.85f));
    g.drawText ("APT INDUSTRIAL", top, juce::Justification::centred);
    g.setFont (fonts().monoFont (t.getHeight() * 0.95f));
    g.setColour (col::ink.withAlpha (0.7f));
    g.drawFittedText (juce::String::fromUTF8 ("MODEL AT-1 \xc2\xb7 SER. 0001"), t.toNearestInt(), juce::Justification::centred, 1, 0.8f);
}
} // namespace at::ui
