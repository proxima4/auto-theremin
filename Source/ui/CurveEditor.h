#pragma once
#include "LookAndFeel.h"
#include "../model/CurveModel.h"

namespace at { class Processor; struct RecPoint; }

namespace at::ui
{
/** The Auto Theremin screen: a green-phosphor grid where you plot the pitch curve
    (notes up the side, bars along the bottom) and a volume lane underneath. */
class CurveEditor : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Tool { NodeTool = 0, DrawTool, EraseTool };

    explicit CurveEditor (Processor&);

    void setTool (Tool t) { tool = t; updateCursor(); repaint(); }
    Tool getTool() const { return tool; }
    void setNewShape (Shape s) { newShape = s; }
    Shape getNewShape() const { return newShape; }
    void applyShapeToSelection (Shape s);
    bool hasSelection() const;

    void setTimeSnap (int idx) { timeSnap = idx; repaint(); }
    void setPitchSnap (int idx) { pitchSnap = idx; }
    void fitView();
    void tick();                   // called by the editor's timer
    void deleteSelection();
    void clearAll();

    std::function<void()> onEdited;   // pattern length / selection changed

    static juce::StringArray timeSnapNames() { return { "Off", "1/32", "1/16", "1/8", "1/4", "1/2", "Bar" }; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    Processor& proc;
    Pattern& pat;
    Tool tool = NodeTool;
    Shape newShape = Shape::Swoop;
    int timeSnap = 3, pitchSnap = 1;

    double viewStart = 0, viewBeats = 16;
    float viewLow = 48, viewNotes = 36;
    juce::Rectangle<float> piano, pitchR, volR, ruler;

    enum Drag { None, Move, Tension, Band, Drawing, Erasing } drag = None;
    bool dragInPitch = true;
    std::vector<Node> dragBase, dragMoved;
    Node primaryOrig;
    juce::Point<float> downPos, curPos;
    int tensionIdx = -1; float tensionOrig = 0;
    std::vector<std::pair<double, float>> drawPts;
    bool auditioning = false;

    int hoverNode = -1; bool hoverInPitch = true; int hoverSeg = -1;
    juce::Point<float> mousePos { -100, -100 };

    std::vector<RecPoint> recPass;
    juce::uint32 lastRecMs = 0;
    double lastPlayBeat = -1;

    // coordinates
    double L() const;
    float beatToX (double b) const;
    double xToBeat (float x) const;
    float pitchToY (float p) const;
    float yToPitch (float y) const;
    float volToY (float v) const;
    float yToVol (float y) const;
    float valueToY (bool pitchLane, float v) const { return pitchLane ? pitchToY (v) : volToY (v); }
    float yToValue (bool pitchLane, float y) const { return pitchLane ? yToPitch (y) : yToVol (y); }
    Lane& lane (bool pitchLane) { return pitchLane ? pat.pitch : pat.volume; }

    double snapBeat (double b, bool bypass) const;
    float snapPitch (float p, bool bypass) const;
    int hitNode (bool pitchLane, juce::Point<float> pos) const;
    int hitTensionHandle (bool pitchLane, juce::Point<float> pos) const;
    juce::Point<float> segmentHandle (bool pitchLane, int seg) const;
    void selectNone();
    void edited();
    void commitRecordPass();
    void updateCursor();
    void showNodeMenu (bool pitchLane, int idx);

    void paintGrid (juce::Graphics&);
    void paintLanes (juce::Graphics&);
    void paintOverlay (juce::Graphics&);
};
} // namespace at::ui
