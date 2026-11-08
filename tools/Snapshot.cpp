// Renders the editor to PNG files (headless, under xvfb).
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/CurveEditor.h"

struct PH : juce::AudioPlayHead
{
    double ppq = 0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    { PositionInfo p; p.setIsPlaying (playing); p.setBpm (110); p.setPpqPosition (ppq); p.setTimeSignature (TimeSignature { 4, 4 }); return p; }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (argc > 1 ? argv[1] : "/tmp/at_snap");
    out.createDirectory();
    const int program = argc > 2 ? juce::String (argv[2]).getIntValue() : 1;
    const double stopAt = argc > 3 ? juce::String (argv[3]).getDoubleValue() : 9.3;

    at::Processor proc;
    PH ph; proc.setPlayHead (&ph);
    proc.prepareToPlay (48000, 512);
    proc.setCurrentProgram (program);
    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
    auto run = [&] (double beats) { while (ph.ppq < beats) { proc.processBlock (b, m); ph.ppq += 512 * 110 / 60.0 / 48000.0; } };

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (at::Editor::designW, at::Editor::designH);
    auto pump = [&] { for (int i = 0; i < 6; ++i) { juce::MessageManager::getInstance()->runDispatchLoopUntil (40); } };

    run (stopAt);
    pump();
    auto save = [&] (const juce::String& name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png; juce::FileOutputStream os (out.getChildFile (name));
        os.setPosition (0); os.truncate(); png.writeImageToStream (img, os);
    };
    save ("main.png");

    // live view
    proc.apvts.getParameter ("mode")->setValueNotifyingHost (proc.apvts.getParameter ("mode")->convertTo0to1 (1));
    proc.fieldActive.store (true);
    for (int i = 0; i < 40; ++i)
    {
        proc.fieldX.store (0.3f + 0.012f * i); proc.fieldY.store (0.5f + 0.008f * i);
        proc.processBlock (b, m); ph.ppq += 512 * 110 / 60.0 / 48000.0;
        juce::MessageManager::getInstance()->runDispatchLoopUntil (34);
    }
    pump();
    save ("live.png");
    proc.fieldActive.store (false);

    // ---- scripted editing on a blank pattern
    proc.apvts.getParameter ("mode")->setValueNotifyingHost (0.0f);
    proc.setCurrentProgram (0);
    pump();
    at::ui::CurveEditor* ce = nullptr;
    std::function<void (juce::Component*)> find = [&] (juce::Component* c)
    { for (auto* k : c->getChildren()) { if (auto* x = dynamic_cast<at::ui::CurveEditor*> (k)) ce = x; find (k); } };
    find (ed.get());
    if (ce == nullptr) { std::printf ("no curve editor\n"); return 1; }
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    auto ev = [&] (juce::Point<float> p, juce::Point<float> down, bool dragged, juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier)
    {
        return juce::MouseEvent (src, p, mods, 1.0f, 0, 0, 0, 0, ce, ce, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged);
    };
    const float W = (float) ce->getWidth(), H = (float) ce->getHeight();
    // place 4 nodes by clicking
    const float xs[] = { 0.1f, 0.3f, 0.55f, 0.8f }, ys[] = { 0.6f, 0.3f, 0.45f, 0.2f };
    for (int i = 0; i < 4; ++i)
    {
        juce::Point<float> p (W * xs[i], H * ys[i]);
        ce->mouseDown (ev (p, p, false)); ce->mouseUp (ev (p, p, false));
    }
    std::printf ("after clicks: pitch nodes %d\n", (int) proc.getPattern().pitch.nodes.size());
    // drag node 2 upward
    { juce::Point<float> p (W * 0.3f, H * 0.3f), q (W * 0.33f, H * 0.18f);
      auto& n0 = proc.getPattern().pitch.nodes[1]; const float before = n0.value;
      ce->mouseDown (ev (p, p, false)); ce->mouseDrag (ev (q, p, true)); ce->mouseUp (ev (q, p, true));
      std::printf ("drag: node value %.2f -> %.2f (count %d)\n", before, proc.getPattern().pitch.nodes[1].value, (int) proc.getPattern().pitch.nodes.size());
      for (auto& n : proc.getPattern().pitch.nodes) std::printf ("   %.3f %.2f sel %d\n", n.beat, n.value, (int) n.selected); }
    // freehand in the volume lane with the draw tool
    ce->setTool (at::ui::CurveEditor::DrawTool);
    { juce::Point<float> p0 (W * 0.06f, H * 0.93f);
      ce->mouseDown (ev (p0, p0, false));
      for (int i = 0; i <= 60; ++i) { juce::Point<float> q (W * (0.06f + 0.9f * i / 60.0f), H * (0.88f - 0.05f * std::sin (i * 0.4f))); ce->mouseDrag (ev (q, p0, true)); }
      ce->mouseUp (ev ({ W * 0.96f, H * 0.88f }, p0, true)); }
    std::printf ("after draw: volume nodes %d\n", (int) proc.getPattern().volume.nodes.size());
    ce->setTool (at::ui::CurveEditor::NodeTool);
    // audio check: play the edited pattern
    ph.ppq = 0; float peak = 0;
    for (int i = 0; i < 400; ++i) { proc.processBlock (b, m); ph.ppq += 512 * 110 / 60.0 / 48000.0; peak = juce::jmax (peak, b.getMagnitude (0, 0, 512)); }
    std::printf ("edited pattern plays, peak %.3f\n", peak);
    { juce::Point<float> p (W * 0.55f, H * 0.45f); ce->mouseMove (ev (p, p, false, {})); }
    pump();
    save ("edited.png");
    ed.reset();
    return 0;
}
