#pragma once
#include "AptVersion.h"
#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Widgets.h"
#include "ui/CurveEditor.h"
#include "ui/FieldPad.h"

namespace at
{
class Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int designW = 1400, designH = 860;

    explicit Editor (Processor&);
    ~Editor() override;
    void resized() override;
    void paint (juce::Graphics&) override {}

private:
    struct Content : public juce::Component
    {
        explicit Content (Editor& e) : ed (e) {}
        void paint (juce::Graphics& g) override { ed.paintContent (g); }
        Editor& ed;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layout();
    void setView (bool field);
    void refreshFromPattern();
    void updateToolButtons();

    Processor& proc;
    ui::LookAndFeel lnf;
    apt::VersionUI versionUi;   // version badge + history (Source/AptVersion.h)
    Content content { *this };
    ui::Textures textures;
    juce::TooltipWindow tooltips { this, 650 };

    using Combo = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using Toggle = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // header
    juce::TextButton modeBtn[3];
    std::unique_ptr<juce::ParameterAttachment> modeAtt;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    ui::Valve valve;

    // left column
    ui::RoundScope scope;
    juce::ComboBox livePitchBox, liveVolBox, liveLowBox, liveRangeBox;
    juce::ToggleButton invertToggle { "INVERT PITCH" };
    std::unique_ptr<Combo> livePitchAtt, liveVolAtt, liveLowAtt, liveRangeAtt;
    std::unique_ptr<Toggle> invertAtt;

    // CRT controls
    juce::TextButton tabCurve { "CURVE" }, tabField { "FIELD" };
    juce::TextButton playBtn { "PLAY" }, recBtn { "REC" };
    juce::ComboBox barsBox;
    juce::ToggleButton loopToggle { "LOOP" };
    juce::TextButton toolBtn[3], shapeBtn[4], fitBtn { "FIT" }, clearBtn { "CLEAR" }, delBtn { "DELETE" };
    juce::ComboBox snapBox, pitchSnapBox;
    ui::CurveEditor curve;
    ui::FieldPad field;
    bool showingField = false;
    int lastMode = -1;

    // sound panel
    std::vector<std::unique_ptr<ui::Knob>> knobs;
    juce::ComboBox scaleBox, keyBox, echoDivBox;
    juce::ToggleButton smartToggle { "EXPRESSIVE" }, echoSyncToggle { "SYNC" };
    std::unique_ptr<Combo> scaleAtt, keyAtt, echoDivAtt;
    std::unique_ptr<Toggle> smartAtt, echoSyncAtt;

    ui::Badge badge;

    int lastPatternCount = -1, lastProgram = -1;
    int blink = 0;
    juce::String padName;
    bool padConnected = false;

    struct Section { juce::Rectangle<float> r; juce::String title; };
    std::vector<Section> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};
} // namespace at
