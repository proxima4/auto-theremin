#pragma once
// =============================================================================
//  APT Industrial - version badge + version history (shared by every plugin)
//
//  A small badge showing the version number (e.g. "v1.0.4"). Clicking it opens
//  the version history - CHANGELOG.md, embedded in the plugin at build time - so
//  the history on GitHub and the history inside the plugin are the same text.
//
//  Use:   apt::VersionUI versionUi;                  // member of the main panel
//         versionUi.attach (*this, style, { x, y, w, h });   // last line of its constructor
//
//  Builds that do not embed the changelog (no "VersionHistoryData.h", e.g. the
//  Android app) simply show no badge.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#if __has_include("VersionHistoryData.h")
 #include "VersionHistoryData.h"
 #define APT_HAS_VERSION_HISTORY 1
#else
 #define APT_HAS_VERSION_HISTORY 0
#endif

#ifndef APT_VERSION
 #define APT_VERSION "dev"
#endif

namespace apt
{
struct VersionStyle
{
    juce::Colour veil   { 0xb0000000 };   // dims the plugin behind the history panel
    juce::Colour panel  { 0xff1c1c1c };   // history panel background
    juce::Colour page   { 0xff111111 };   // text area background
    juce::Colour edge   { 0xffc8c8c8 };   // borders
    juce::Colour text   { 0xffeeeeee };
    juce::Colour dim    { 0xff9a9a9a };
    juce::Colour accent { 0xffffc040 };   // version number, headings
    juce::Colour badgeFill { 0x00000000 };
    juce::Font titleFont { juce::FontOptions (24.0f, juce::Font::bold) };
    juce::Font bodyFont  { juce::FontOptions (16.0f) };
    juce::Font badgeFont { juce::FontOptions (13.0f, juce::Font::bold) };
    float corner = 5.0f;
    juce::String productName;
};

inline juce::String versionString() { return juce::String ("v") + APT_VERSION; }

inline juce::String historyText()
{
   #if APT_HAS_VERSION_HISTORY
    const auto md = juce::String::fromUTF8 (VersionHistoryData::CHANGELOG_md, VersionHistoryData::CHANGELOG_mdSize);
    juce::StringArray out;
    for (auto line : juce::StringArray::fromLines (md))
    {
        if (line.startsWith ("# ")) continue;                                         // title is drawn separately
        if (line.startsWith ("## ")) { if (out.size() > 0) out.add (""); out.add (line.substring (3).toUpperCase()); continue; }
        if (line.startsWith ("- "))  { out.add (juce::String::charToString ((juce::juce_wchar) 0x2022) + "  " + line.substring (2)); continue; }
        out.add (line);
    }
    return out.joinIntoString ("\n").trim();
   #else
    return {};
   #endif
}

class VersionBadge : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit VersionBadge (const VersionStyle& s) : style (s)
    {
        setTooltip ("Version history");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setRepaintsOnMouseActivity (true);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        if (! style.badgeFill.isTransparent()) { g.setColour (style.badgeFill); g.fillRoundedRectangle (r, style.corner); }
        if (isMouseOver()) { g.setColour (style.accent.withAlpha (0.18f)); g.fillRoundedRectangle (r, style.corner); }
        g.setColour (style.accent.withAlpha (isMouseOver() ? 1.0f : 0.75f));
        g.drawRoundedRectangle (r, style.corner, 1.2f);
        g.setFont (style.badgeFont);
        g.setColour (style.accent);
        g.drawFittedText (versionString(), getLocalBounds().reduced (3, 0), juce::Justification::centred, 1, 0.8f);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
private:
    const VersionStyle& style;
};

class VersionHistoryPanel : public juce::Component
{
public:
    explicit VersionHistoryPanel (const VersionStyle& s) : style (s)
    {
        text.setMultiLine (true, true);
        text.setReadOnly (true);
        text.setScrollbarsShown (true);
        text.setCaretVisible (false);
        text.setFont (style.bodyFont);
        text.setColour (juce::TextEditor::backgroundColourId, style.page);
        text.setColour (juce::TextEditor::textColourId, style.text);
        text.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        text.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        text.setColour (juce::TextEditor::highlightColourId, style.accent.withAlpha (0.3f));
        text.setText (historyText(), false);
        addAndMakeVisible (text);

        close.setColour (juce::TextButton::buttonColourId, style.panel.brighter (0.15f));
        close.setColour (juce::TextButton::textColourOffId, style.text);
        close.onClick = [this] { setVisible (false); };
        addAndMakeVisible (close);
    }

    void resized() override
    {
        auto p = panelArea().toNearestInt().reduced (22);
        p.removeFromTop (58);
        close.setBounds (p.removeFromBottom (32).withSizeKeepingCentre (110, 30));
        p.removeFromBottom (10);
        text.setBounds (p);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (style.veil);
        auto p = panelArea();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (p.translated (0.0f, 5.0f), style.corner * 2.0f);
        g.setColour (style.panel);
        g.fillRoundedRectangle (p, style.corner * 2.0f);
        g.setColour (style.edge);
        g.drawRoundedRectangle (p.reduced (1.0f), style.corner * 2.0f, 1.5f);
        auto head = p.reduced (22.0f).removeFromTop (54.0f);
        g.setColour (style.accent);
        g.setFont (style.titleFont);
        g.drawFittedText ("Version History", head.removeFromTop (32.0f).toNearestInt(), juce::Justification::centred, 1);
        g.setColour (style.dim);
        g.setFont (style.bodyFont.withHeight (style.bodyFont.getHeight() * 0.9f));
        g.drawFittedText ((style.productName.isNotEmpty() ? style.productName + "  " : juce::String()) + versionString(),
                          head.toNearestInt(), juce::Justification::centred, 1);
    }

    void mouseDown (const juce::MouseEvent& e) override { if (! panelArea().contains (e.position)) setVisible (false); }
    bool keyPressed (const juce::KeyPress& k) override { if (k == juce::KeyPress::escapeKey) { setVisible (false); return true; } return false; }

private:
    juce::Rectangle<float> panelArea() const
    {
        auto b = getLocalBounds().toFloat();
        return b.withSizeKeepingCentre (juce::jmin (780.0f, b.getWidth() - 40.0f), juce::jmin (640.0f, b.getHeight() - 40.0f));
    }
    const VersionStyle& style;
    juce::TextEditor text;
    juce::TextButton close { "Close" };
};

// Owns the badge and the history panel and keeps the panel covering the host component.
class VersionUI : private juce::ComponentListener
{
public:
    VersionUI() = default;
    // Safe in any member order: if the host panel has already been destroyed, there is nothing to detach from.
    ~VersionUI() override { if (auto* h = host.getComponent()) h->removeComponentListener (this); }

    void attach (juce::Component& hostComponent, const VersionStyle& s, juce::Rectangle<int> badgeBounds)
    {
       #if APT_HAS_VERSION_HISTORY
        style = s;
        host = &hostComponent;
        badge = std::make_unique<VersionBadge> (style);
        panel = std::make_unique<VersionHistoryPanel> (style);
        hostComponent.addAndMakeVisible (*badge);
        badge->setBounds (badgeBounds);
        hostComponent.addChildComponent (*panel);      // added last, so it sits above everything
        panel->setBounds (hostComponent.getLocalBounds());
        badge->onClick = [this] { open(); };
        hostComponent.addComponentListener (this);
       #else
        juce::ignoreUnused (hostComponent, s, badgeBounds);
       #endif
    }

    void open()
    {
        if (panel == nullptr) return;
        if (auto* h = host.getComponent()) panel->setBounds (h->getLocalBounds());
        panel->setVisible (true);
        panel->toFront (true);
    }
    VersionBadge* getBadge() const noexcept { return badge.get(); }

private:
    void componentMovedOrResized (juce::Component& c, bool, bool wasResized) override
    {
        if (wasResized && panel != nullptr) panel->setBounds (c.getLocalBounds());
    }
    void componentChildrenChanged (juce::Component&) override
    {
        if (panel != nullptr && panel->isVisible()) panel->toFront (false);
    }
    VersionStyle style;
    juce::Component::SafePointer<juce::Component> host;
    std::unique_ptr<VersionBadge> badge;
    std::unique_ptr<VersionHistoryPanel> panel;
};
} // namespace apt
