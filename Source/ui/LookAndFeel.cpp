#include "LookAndFeel.h"
#include "BinaryData.h"
#include <random>

namespace at::ui
{
Fonts::Fonts()
{
    using juce::Typeface;
    logo  = Typeface::createSystemTypefaceFor (BinaryData::MonotonRegular_ttf, BinaryData::MonotonRegular_ttfSize);
    label = Typeface::createSystemTypefaceFor (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
    mono  = Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    sign  = Typeface::createSystemTypefaceFor (BinaryData::RighteousRegular_ttf, BinaryData::RighteousRegular_ttfSize);
}

const Fonts& fonts()
{
    static Fonts f;
    return f;
}

// ---------------------------------------------------------------- textures
juce::Image Textures::enamel (int w, int h)
{
    if (enamelImg.isValid() && enamelImg.getWidth() == w && enamelImg.getHeight() == h) return enamelImg;
    juce::Image img (juce::Image::RGB, w, h, false);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    std::minstd_rand rng (1957);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);

    // low-frequency mottling from a few soft blobs
    struct Blob { float x, y, r, a; };
    std::vector<Blob> blobs;
    for (int i = 0; i < 26; ++i) blobs.push_back ({ u (rng) * w, u (rng) * h, 60 + u (rng) * 260, (u (rng) - 0.5f) * 0.05f });

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            float shade = 0;
            for (auto& b : blobs)
            {
                const float dx = (x - b.x) / b.r, dy = (y - b.y) / b.r;
                const float d2 = dx * dx + dy * dy;
                if (d2 < 1) shade += b.a * (1 - d2);
            }
            shade += (u (rng) - 0.5f) * 0.035f;                    // fine grain
            if (u (rng) < 0.0018f) shade -= 0.10f + u (rng) * 0.1f;  // dark speckle
            const float vy = (float) y / (float) h;
            shade -= 0.05f * vy * vy;                               // grime towards the bottom
            const auto c = col::enamel;
            auto ch = [&] (juce::uint8 v) { return (juce::uint8) juce::jlimit (0, 255, (int) (v * (1.0f + shade))); };
            bd.setPixelColour (x, y, juce::Colour (ch (c.getRed()), ch (c.getGreen()), ch (c.getBlue())));
        }
    enamelImg = img;
    return img;
}

juce::Image Textures::scanlines (int w, int h)
{
    if (scanImg.isValid() && scanImg.getWidth() == w && scanImg.getHeight() == h) return scanImg;
    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Graphics g (img);
    for (int y = 0; y < h; y += 3)
    {
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRect (0, y, w, 1);
    }
    scanImg = img;
    return img;
}

// ---------------------------------------------------------------- helpers
void drawChromeBezel (juce::Graphics& g, juce::Rectangle<float> r, float corner, float t)
{
    juce::ColourGradient cg (juce::Colour (0xfff8fafb), r.getX(), r.getY(), juce::Colour (0xff5d656c), r.getX(), r.getBottom(), false);
    cg.addColour (0.18, juce::Colour (0xffc9d0d6));
    cg.addColour (0.45, juce::Colour (0xff7e868d));
    cg.addColour (0.55, juce::Colour (0xffeef2f4));
    cg.addColour (0.8, juce::Colour (0xff9aa2a9));
    g.setGradientFill (cg);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    const auto inner = r.reduced (t);
    juce::ColourGradient ig (juce::Colour (0xff4a5157), inner.getX(), inner.getY(), juce::Colour (0xffe6eaed), inner.getX(), inner.getBottom(), false);
    g.setGradientFill (ig);
    g.drawRoundedRectangle (inner.expanded (1.0f), juce::jmax (1.0f, corner - t + 1), 2.0f);
}

void drawRivet (juce::Graphics& g, juce::Point<float> c, float rad)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - rad + 0.6f, c.y - rad + 1.0f, rad * 2, rad * 2);
    juce::ColourGradient cg (juce::Colour (0xfffbfcfc), c.x - rad * 0.5f, c.y - rad * 0.6f, juce::Colour (0xff6c747a), c.x + rad, c.y + rad, true);
    g.setGradientFill (cg);
    g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
}

void drawGlowPath (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w, float glow)
{
    const juce::PathStrokeType::JointStyle j = juce::PathStrokeType::curved;
    g.setColour (c.withAlpha (0.07f * glow));  g.strokePath (p, juce::PathStrokeType (w * 7.0f, j, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.16f * glow));  g.strokePath (p, juce::PathStrokeType (w * 3.6f, j, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.85f));         g.strokePath (p, juce::PathStrokeType (w, j, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.35f * glow)); g.strokePath (p, juce::PathStrokeType (w * 0.35f, j, juce::PathStrokeType::rounded));
}

void drawSectionPlate (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text)
{
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (r.translated (0, 1.2f), 3);
    juce::ColourGradient cg (col::cherry.brighter (0.15f), r.getX(), r.getY(), col::cherryDark, r.getX(), r.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (r, 3);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3, r.getRight() - 3);
    g.setColour (col::enamel);
    g.setFont (fonts().labelFont (r.getHeight() * 0.56f).withExtraKerningFactor (0.12f));
    g.drawText (text, r, juce::Justification::centred);
}

void drawEngravedBox (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawRoundedRectangle (r.translated (0.8f, 0.8f), corner, 1.0f);
    g.setColour (col::inkSoft.withAlpha (0.55f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void drawCrtGlass (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    // vignette
    juce::ColourGradient vg (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                             juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(), true);
    g.setGradientFill (vg);
    g.fillRoundedRectangle (r, corner);
    // curved-glass reflection
    juce::Path refl;
    refl.addRoundedRectangle (r.getX() + r.getWidth() * 0.03f, r.getY() + r.getHeight() * 0.03f,
                              r.getWidth() * 0.94f, r.getHeight() * 0.22f, corner);
    juce::ColourGradient rg (juce::Colours::white.withAlpha (0.07f), r.getX(), r.getY(),
                             juce::Colours::white.withAlpha (0.0f), r.getX(), r.getY() + r.getHeight() * 0.25f, false);
    g.setGradientFill (rg);
    g.fillPath (refl);
}

void drawLamp (juce::Graphics& g, juce::Point<float> c, float rad, juce::Colour colour, bool on)
{
    if (on)
    {
        juce::ColourGradient glow (colour.withAlpha (0.55f), c.x, c.y, colour.withAlpha (0.0f), c.x + rad * 3.2f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - rad * 3.2f, c.y - rad * 3.2f, rad * 6.4f, rad * 6.4f);
    }
    g.setColour (juce::Colour (0xff8d959b));
    g.fillEllipse (c.x - rad - 1.5f, c.y - rad - 1.5f, (rad + 1.5f) * 2, (rad + 1.5f) * 2);
    const auto base = on ? colour : colour.darker (2.2f).withMultipliedSaturation (0.5f);
    juce::ColourGradient cg (base.brighter (on ? 0.9f : 0.3f), c.x - rad * 0.3f, c.y - rad * 0.4f, base.darker (0.4f), c.x + rad, c.y + rad, true);
    g.setGradientFill (cg);
    g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
}

// ---------------------------------------------------------------- LookAndFeel
LookAndFeel::LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, col::enamel);
    setColour (juce::PopupMenu::textColourId, col::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::cherry);
    setColour (juce::PopupMenu::highlightedTextColourId, col::enamel);
    setColour (juce::ComboBox::textColourId, col::phosphor);
    setColour (juce::TooltipWindow::backgroundColourId, col::crtBlack);
    setColour (juce::TooltipWindow::textColourId, col::phosphor);
    setDefaultSansSerifTypeface (fonts().label);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f;
    const auto c = bounds.getCentre();
    const float ang = start + pos * (end - start);
    const bool bipolar = s.getProperties()["bipolar"];

    // printed scale on the enamel
    g.setColour (col::ink.withAlpha (0.75f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f;
        const float r0 = R * (i % 5 == 0 ? 0.80f : 0.86f), r1 = R * 0.98f;
        g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), i % 5 == 0 ? 1.4f : 0.9f);
    }
    // value arc in cherry red
    {
        juce::Path arc;
        const float from = bipolar ? (start + end) * 0.5f : start;
        arc.addCentredArc (c.x, c.y, R * 0.9f, R * 0.9f, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setColour (col::cherry.withAlpha (0.85f));
        g.strokePath (arc, juce::PathStrokeType (2.2f));
    }

    const float skirt = R * 0.74f;
    // drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillEllipse (c.x - skirt + 1.5f, c.y - skirt + 3.0f, skirt * 2, skirt * 2);
    // chrome skirt with knurling
    juce::ColourGradient cg (juce::Colour (0xfff4f6f7), c.x - skirt, c.y - skirt, juce::Colour (0xff5e666d), c.x + skirt, c.y + skirt, false);
    cg.addColour (0.5, juce::Colour (0xffb6bec4));
    g.setGradientFill (cg);
    g.fillEllipse (c.x - skirt, c.y - skirt, skirt * 2, skirt * 2);
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    for (int i = 0; i < 48; ++i)
    {
        const float a = ang + juce::MathConstants<float>::twoPi * (float) i / 48.0f;
        g.drawLine (c.x + skirt * 0.84f * std::sin (a), c.y - skirt * 0.84f * std::cos (a),
                    c.x + skirt * std::sin (a), c.y - skirt * std::cos (a), 0.8f);
    }
    // red index on skirt
    g.setColour (col::cherry);
    g.drawLine (c.x + skirt * 0.78f * std::sin (ang), c.y - skirt * 0.78f * std::cos (ang),
                c.x + skirt * 0.99f * std::sin (ang), c.y - skirt * 0.99f * std::cos (ang), 2.6f);

    // bakelite cap
    const float cap = skirt * 0.72f;
    juce::ColourGradient bg (col::bakelite.brighter (0.6f), c.x - cap * 0.5f, c.y - cap * 0.7f, col::bakelite.darker (0.6f), c.x + cap * 0.6f, c.y + cap, true);
    g.setGradientFill (bg);
    g.fillEllipse (c.x - cap, c.y - cap, cap * 2, cap * 2);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.fillEllipse (c.x - cap * 0.62f, c.y - cap * 0.82f, cap * 1.0f, cap * 0.55f);
    // cream pointer
    g.setColour (col::enamel);
    g.drawLine (c.x + cap * 0.15f * std::sin (ang), c.y - cap * 0.15f * std::cos (ang),
                c.x + cap * 0.88f * std::sin (ang), c.y - cap * 0.88f * std::cos (ang), 2.2f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const juce::String style = b.getProperties()["style"].toString();
    const bool on = b.getToggleState();

    if (style == "tool")   // dark glass keys on the CRT bezel
    {
        g.setColour (on ? col::phosphor.withAlpha (0.18f) : juce::Colour (0xff0c1a10));
        g.fillRoundedRectangle (r, 3);
        g.setColour (on ? col::phosphor.withAlpha (0.9f) : col::phosphorDim.withAlpha (over ? 0.9f : 0.55f));
        g.drawRoundedRectangle (r, 3, on ? 1.4f : 1.0f);
        return;
    }

    // chrome push button (mode keys get a lamp)
    const bool pushed = down || (on && style == "key");
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (r.translated (0, pushed ? 0.6f : 1.8f), 4);
    juce::ColourGradient cg (juce::Colour (pushed ? 0xffc3cad0 : 0xfffafbfc), r.getX(), r.getY(),
                             juce::Colour (pushed ? 0xff7a838a : 0xff8e979e), r.getX(), r.getBottom(), false);
    cg.addColour (0.5, juce::Colour (pushed ? 0xff9aa2a9 : 0xffd2d8dc));
    g.setGradientFill (cg);
    g.fillRoundedRectangle (r.translated (0, pushed ? 1.0f : 0.0f), 4);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (r.translated (0, pushed ? 1.0f : 0.0f), 4, 1.0f);
    if (over && ! pushed)
    {
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillRoundedRectangle (r, 4);
    }
    if (style == "key" || style == "lamp" || style == "rec")
    {
        const auto lc = style == "rec" ? juce::Colour (0xffff3b2a) : (style == "lamp" ? col::mint : col::amber);
        drawLamp (g, { r.getX() + 11.0f, r.getCentreY() + (pushed ? 1.0f : 0.0f) }, 4.0f, lc, on);
    }
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    const juce::String style = b.getProperties()["style"].toString();
    if (style == "tool") return fonts().monoFont (juce::jmin (15.0f, h * 0.62f));
    return fonts().labelFont (juce::jmin (11.0f, h * 0.42f));
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const juce::String style = b.getProperties()["style"].toString();
    auto r = b.getLocalBounds();
    const bool on = b.getToggleState();
    g.setFont (getTextButtonFont (b, b.getHeight()));
    if (style == "tool")
    {
        g.setColour (on ? col::phosphor : col::phosphor.withAlpha (0.62f));
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
        return;
    }
    if (style == "key" || style == "lamp" || style == "rec") r.removeFromLeft (16);
    if (down || (on && style == "key")) r.translate (0, 1);
    g.setColour (col::ink);
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    // little bat-handle toggle switch with a label
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    auto sw = r.removeFromLeft (22).withSizeKeepingCentre (14, 22);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (sw.translated (0.5f, 1.5f), 7);
    juce::ColourGradient cg (juce::Colour (0xffeef1f3), sw.getX(), sw.getY(), juce::Colour (0xff6d757c), sw.getRight(), sw.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (sw, 7);
    const auto knob = juce::Rectangle<float> (sw.getX() + 2, on ? sw.getY() + 2 : sw.getBottom() - 12, sw.getWidth() - 4, 10);
    g.setColour (on ? col::cherry : col::bakelite);
    g.fillEllipse (knob);
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.fillEllipse (knob.reduced (3).translated (-1, -1));
    g.setColour (over ? col::ink : col::inkSoft);
    g.setFont (fonts().labelFont (9.0f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (3), juce::Justification::centredLeft, 2);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff8e979e));
    g.fillRoundedRectangle (r, 3);
    auto win = r.reduced (2.0f);
    g.setColour (col::crtBlack);
    g.fillRoundedRectangle (win, 2);
    g.setColour (col::phosphor.withAlpha (box.isMouseOver (true) ? 0.08f : 0.04f));
    g.fillRoundedRectangle (win, 2);
    // arrow
    juce::Path tri;
    const float ax = win.getRight() - 9, ay = win.getCentreY();
    tri.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (col::phosphor.withAlpha (0.8f));
    g.fillPath (tri);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& b) { return fonts().monoFont (juce::jmin (16.0f, b.getHeight() * 0.68f)); }

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 18, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (col::enamel);
    g.setColour (col::inkSoft);
    g.drawRect (0, 0, w, h, 1);
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool sep, bool active, bool hl,
                                     bool ticked, bool sub, const juce::String& text, const juce::String& shortcut,
                                     const juce::Drawable* icon, const juce::Colour* tc)
{
    if (sep || ! hl) { LookAndFeel_V4::drawPopupMenuItem (g, area, sep, active, hl, ticked, sub, text, shortcut, icon, tc); return; }
    g.setColour (col::cherry);
    g.fillRect (area);
    g.setColour (col::enamel);
    g.setFont (getPopupMenuFont());
    auto r = area.reduced (8, 0);
    if (ticked) g.drawText (juce::String::fromUTF8 ("\xe2\x97\x8f"), r.removeFromLeft (14), juce::Justification::centred);
    else r.removeFromLeft (14);
    g.drawText (text, r, juce::Justification::centredLeft);
}

juce::Font LookAndFeel::getPopupMenuFont() { return fonts().labelFont (12.0f); }

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.setColour (col::crtBlack);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 3);
    g.setColour (col::phosphorDim);
    g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, h - 1.0f, 3, 1);
    g.setColour (col::phosphor);
    g.setFont (fonts().monoFont (15.0f));
    g.drawFittedText (text, 8, 4, w - 16, h - 8, juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s; s.append (tip, fonts().monoFont (15.0f));
    juce::TextLayout tl; tl.createLayout (s, 320.0f);
    const int w = (int) tl.getWidth() + 18, h = (int) tl.getHeight() + 10;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h).constrainedWithin (parent);
}
} // namespace at::ui
