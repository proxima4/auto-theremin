#include "PluginEditor.h"
#include "Presets.h"

namespace at
{
using namespace ui;

static const int kBarsChoices[] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64 };

Editor::Editor (Processor& p)
    : AudioProcessorEditor (p), proc (p), scope (p), curve (p), field (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setSize (designW, designH);

    auto& s = proc.apvts;

    // ---- header: mode keys
    const char* modes[] = { "AUTO", "LIVE", "MIDI" };
    const char* modeTips[] = { "Auto: plays the curves you draw, following FL Studio's transport",
                               "Live: play with a PS5 DualSense or by dragging on the Field screen",
                               "MIDI: play from a keyboard or the piano roll; notes glide into each other" };
    auto* modeParam = s.getParameter ("mode");
    modeAtt = std::make_unique<juce::ParameterAttachment> (*modeParam, [this] (float v)
    {
        const int m = juce::roundToInt (v);
        for (int i = 0; i < 3; ++i) modeBtn[i].setToggleState (i == m, juce::dontSendNotification);
    });
    for (int i = 0; i < 3; ++i)
    {
        modeBtn[i].setButtonText (modes[i]);
        modeBtn[i].getProperties().set ("style", "key");
        modeBtn[i].setTooltip (modeTips[i]);
        modeBtn[i].onClick = [this, i] { modeAtt->setValueAsCompleteGesture ((float) i); };
        content.addAndMakeVisible (modeBtn[i]);
    }
    modeAtt->sendInitialUpdate();

    // ---- presets
    for (int i = 0; i < (int) factoryPresets().size(); ++i)
        presetBox.addItem (juce::String (i + 1).paddedLeft ('0', 2) + "  " + juce::String (factoryPresets()[(size_t) i].name).toUpperCase(), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram()) proc.setCurrentProgram (idx);
    };
    presetBox.setTooltip ("Factory presets: a sound plus an example curve to learn from");
    prevBtn.onClick = [this] { const int n = presetBox.getNumItems(); presetBox.setSelectedId ((presetBox.getSelectedId() + n - 2) % n + 1); };
    nextBtn.onClick = [this] { const int n = presetBox.getNumItems(); presetBox.setSelectedId (presetBox.getSelectedId() % n + 1); };
    content.addAndMakeVisible (presetBox);
    content.addAndMakeVisible (prevBtn);
    content.addAndMakeVisible (nextBtn);
    content.addAndMakeVisible (valve);

    // ---- left column
    content.addAndMakeVisible (scope);
    auto fillChoices = [&] (juce::ComboBox& b, const juce::String& id)
    {
        auto* prm = s.getParameter (id);
        const int steps = prm->getNumSteps();
        for (int i = 0; i < steps; ++i)
            b.addItem (prm->getText ((float) i / (float) juce::jmax (1, steps - 1), 64), i + 1);
        content.addAndMakeVisible (b);
    };
    fillChoices (livePitchBox, "livePitch");
    fillChoices (liveVolBox, "liveVolume");
    fillChoices (liveLowBox, "liveLow");
    fillChoices (liveRangeBox, "liveRange");
    livePitchAtt = std::make_unique<Combo> (s, "livePitch", livePitchBox);
    liveVolAtt = std::make_unique<Combo> (s, "liveVolume", liveVolBox);
    liveLowAtt = std::make_unique<Combo> (s, "liveLow", liveLowBox);
    liveRangeAtt = std::make_unique<Combo> (s, "liveRange", liveRangeBox);
    livePitchBox.setTooltip ("What controls pitch when playing live. Touchpad or gyro tilt are the smoothest; sticks spring back to centre (hold X to freeze the pitch)");
    liveVolBox.setTooltip ("What controls volume when playing live. A trigger works like the theremin's volume loop: release for silence");
    liveLowBox.setTooltip ("Lowest note of the live playing range");
    liveRangeBox.setTooltip ("Width of the live playing range in octaves (smaller = finer control)");
    content.addAndMakeVisible (invertToggle);
    invertAtt = std::make_unique<Toggle> (s, "liveInvert", invertToggle);
    invertToggle.setTooltip ("Flip the pitch direction of the live input (e.g. if tilting feels backwards)");

    // ---- CRT controls
    for (auto* b : { &tabCurve, &tabField })
    {
        b->getProperties().set ("style", "key");
        b->setRadioGroupId (77);
        b->setClickingTogglesState (false);
        content.addAndMakeVisible (*b);
    }
    tabCurve.onClick = [this] { setView (false); };
    tabField.onClick = [this] { setView (true); };
    tabCurve.setTooltip ("Curve plotter: draw the pitch and volume curves for Auto mode");
    tabField.setTooltip ("Playing field: watch or play the live input");

    playBtn.getProperties().set ("style", "lamp");
    playBtn.setTooltip ("Play the pattern on its own clock (when FL Studio isn't playing). Options button on the controller does the same");
    playBtn.onClick = [this] { proc.internalPlay.store (! proc.internalPlay.load()); };
    recBtn.getProperties().set ("style", "rec");
    recBtn.setTooltip ("Record: in Live or MIDI mode, your performance is written into the curves while the transport runs (Triangle on the controller)");
    recBtn.onClick = [this] { proc.recordArmed.store (! proc.recordArmed.load()); };
    content.addAndMakeVisible (playBtn);
    content.addAndMakeVisible (recBtn);

    for (int i = 0; i < (int) std::size (kBarsChoices); ++i) barsBox.addItem (juce::String (kBarsChoices[i]) + (kBarsChoices[i] == 1 ? " BAR" : " BARS"), i + 1);
    barsBox.onChange = [this]
    {
        const int bars = kBarsChoices[juce::jmax (0, barsBox.getSelectedId() - 1)];
        if (bars != proc.getPattern().lengthBars) { proc.getPattern().lengthBars = bars; proc.patternChanged(); curve.fitView(); }
    };
    barsBox.setTooltip ("Pattern length");
    content.addAndMakeVisible (barsBox);
    loopToggle.onClick = [this] { proc.getPattern().loop = loopToggle.getToggleState(); proc.patternChanged(); curve.repaint(); };
    loopToggle.setTooltip ("Loop the pattern along the song, or play it once from bar 1");
    content.addAndMakeVisible (loopToggle);

    const char* tools[] = { "NODE", "DRAW", "ERASE" };
    const char* toolTips[] = { "Node tool: click to add, drag to move, drag the diamond to bend a segment, double-click to delete, right-click for options. Shift-drag selects; Alt bypasses snap",
                               "Draw tool: sketch a freehand curve; it becomes editable nodes",
                               "Erase tool: drag over nodes to remove them" };
    for (int i = 0; i < 3; ++i)
    {
        toolBtn[i].setButtonText (tools[i]);
        toolBtn[i].getProperties().set ("style", "tool");
        toolBtn[i].setTooltip (toolTips[i]);
        toolBtn[i].onClick = [this, i] { curve.setTool ((CurveEditor::Tool) i); updateToolButtons(); };
        content.addAndMakeVisible (toolBtn[i]);
    }
    const char* shapeTips[] = { "Smooth: an eased S-curve between notes (keys 1-4 set the shape of selected nodes)",
                                "Linear: a straight glide; bend it with the diamond handle",
                                "Hold: stay on the note, then jump",
                                "Swoop: hold the note, then slide into the next one, the classic theremin portamento. The handle sets the slide length" };
    for (int i = 0; i < 4; ++i)
    {
        shapeBtn[i].setButtonText (juce::String (shapeName ((Shape) i)).toUpperCase());
        shapeBtn[i].getProperties().set ("style", "tool");
        shapeBtn[i].setTooltip (shapeTips[i]);
        shapeBtn[i].onClick = [this, i]
        {
            curve.setNewShape ((Shape) i);
            curve.applyShapeToSelection ((Shape) i);
            updateToolButtons();
        };
        content.addAndMakeVisible (shapeBtn[i]);
    }
    for (auto* b : { &fitBtn, &clearBtn, &delBtn })
    {
        b->getProperties().set ("style", "tool");
        content.addAndMakeVisible (*b);
    }
    fitBtn.setTooltip ("Fit the whole pattern on screen. Mouse wheel scrolls notes, Ctrl+wheel zooms time, Shift+wheel scrolls time, Alt+wheel zooms notes");
    fitBtn.onClick = [this] { curve.fitView(); };
    clearBtn.setTooltip ("Clear both lanes");
    clearBtn.onClick = [this]
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Clear pattern",
            "Remove every node from the pitch and volume lanes?", "Clear", "Cancel", this,
            juce::ModalCallbackFunction::create ([this] (int r) { if (r == 1) curve.clearAll(); }));
    };
    delBtn.setTooltip ("Delete the selected nodes (or press Delete)");
    delBtn.onClick = [this] { curve.deleteSelection(); };

    for (auto& n : CurveEditor::timeSnapNames()) snapBox.addItem ("SNAP " + n, snapBox.getNumItems() + 1);
    snapBox.setSelectedId (4, juce::dontSendNotification);
    snapBox.onChange = [this] { curve.setTimeSnap (snapBox.getSelectedId() - 1); };
    snapBox.setTooltip ("Time grid for placing and moving nodes (hold Alt to bypass)");
    pitchSnapBox.addItem ("FREE PITCH", 1);
    pitchSnapBox.addItem ("SEMITONES", 2);
    pitchSnapBox.addItem ("SCALE", 3);
    pitchSnapBox.setSelectedId (2, juce::dontSendNotification);
    pitchSnapBox.onChange = [this] { curve.setPitchSnap (pitchSnapBox.getSelectedId() - 1); };
    pitchSnapBox.setTooltip ("Where new or moved nodes land: anywhere, on semitones, or on notes of the chosen scale");
    content.addAndMakeVisible (snapBox);
    content.addAndMakeVisible (pitchSnapBox);

    curve.onEdited = [this] { updateToolButtons(); };
    content.addAndMakeVisible (curve);
    content.addChildComponent (field);

    // ---- sound panel
    struct K { const char* id; const char* label; const char* tip; bool bip; };
    const K ks[] = {
        { "timbre", "TIMBRE", "Coupling of the two heterodyne oscillators: pure sine to the reedy, pulled theremin tone", false },
        { "character", "CHARACTER", "Detector phase: changes how lopsided the beat waveform is", false },
        { "warmth", "VALVE", "Valve warmth: soft, slightly asymmetric saturation", false },
        { "formant", "FORMANT", "Vocal formant filters, like the original RCA tone shaping", false },
        { "brightness", "BRIGHT", "Darker (left) or brighter (right)", true },
        { "glide", "GLIDE", "How smoothly pitch follows the curve, hand or keyboard", false },
        { "correction", "CORRECT", "Pitch correction: 0 is fully free, 100% snaps to the scale; in between, notes slide but settle", false },
        { "vibDepth", "DEPTH", "Vibrato depth in cents", false },
        { "vibRate", "RATE", "Vibrato speed", false },
        { "echoTime", "TIME", "Echo time (when Sync is off)", false },
        { "echoFeedback", "REPEATS", "Echo feedback", false },
        { "echoMix", "MIX", "Echo level", false },
        { "verbSize", "SIZE", "Reverb size and decay", false },
        { "verbDamp", "DAMP", "Reverb high-frequency damping", false },
        { "verbMix", "MIX", "Reverb level", false },
        { "response", "RESPONSE", "How quickly volume follows the volume curve, trigger or key", false },
        { "output", "LEVEL", "Output level", false },
    };
    for (auto& k : ks)
    {
        knobs.push_back (std::make_unique<Knob> (s, k.id, k.label, k.tip, k.bip));
        content.addAndMakeVisible (*knobs.back());
    }
    fillChoices (scaleBox, "scale");
    fillChoices (keyBox, "key");
    fillChoices (echoDivBox, "echoDiv");
    scaleAtt = std::make_unique<Combo> (s, "scale", scaleBox);
    keyAtt = std::make_unique<Combo> (s, "key", keyBox);
    echoDivAtt = std::make_unique<Combo> (s, "echoDiv", echoDivBox);
    scaleBox.setTooltip ("Scale for pitch correction, scale snapping and the highlighted grid rows");
    keyBox.setTooltip ("Key of the scale");
    echoDivBox.setTooltip ("Echo time as a note length, synced to the song tempo");
    content.addAndMakeVisible (smartToggle);
    content.addAndMakeVisible (echoSyncToggle);
    smartAtt = std::make_unique<Toggle> (s, "smartVib", smartToggle);
    echoSyncAtt = std::make_unique<Toggle> (s, "echoSync", echoSyncToggle);
    smartToggle.setTooltip ("Expressive vibrato: blooms on held notes and backs off during swoops, as a player's hand does");
    echoSyncToggle.setTooltip ("Sync echo time to the song tempo");

    content.addAndMakeVisible (badge);

    layout();
    refreshFromPattern();
    updateToolButtons();

    {
        apt::VersionStyle vs;
        vs.veil = juce::Colour (0x99241512); vs.panel = ui::col::enamel; vs.page = juce::Colour (0xfff7f0dd); vs.edge = ui::col::inkSoft;
        vs.text = ui::col::ink; vs.dim = ui::col::inkSoft; vs.accent = ui::col::cherry;
        vs.titleFont = ui::fonts().labelFont (26.0f); vs.bodyFont = ui::fonts().labelFont (17.0f); vs.badgeFont = ui::fonts().monoFont (13.0f);
        vs.productName = "Auto Theremin";
        versionUi.attach (content, vs, { 1150, 62, 64, 18 });
    }

    setResizable (true, true);
    setResizeLimits (designW * 6 / 10, designH * 6 / 10, designW * 3 / 2, designH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / designH);
    setSize (designW * 9 / 10, designH * 9 / 10);
    startTimerHz (30);
}

Editor::~Editor()
{
    stopTimer();
    proc.auditionOn.store (false);
    proc.fieldActive.store (false);
    setLookAndFeel (nullptr);
}

void Editor::resized()
{
    const float sc = (float) getWidth() / (float) designW;
    content.setTransform (juce::AffineTransform::scale (sc));
}

void Editor::layout()
{
    using R = juce::Rectangle<int>;
    // header
    for (int i = 0; i < 3; ++i) modeBtn[i].setBounds (548 + i * 96, 24, 90, 36);
    prevBtn.setBounds (880, 26, 30, 32);
    presetBox.setBounds (914, 26, 290, 32);
    nextBtn.setBounds (1208, 26, 30, 32);
    valve.setBounds (1300, 6, 52, 74);

    // left column
    scope.setBounds (35, 96, 290, 290);
    const int lx = 40, lw = 280;
    livePitchBox.setBounds (lx + 96, 440, lw - 96, 26);
    liveVolBox.setBounds (lx + 96, 474, lw - 96, 26);
    liveLowBox.setBounds (lx + 96, 508, 74, 26);
    liveRangeBox.setBounds (lx + 96 + 134, 508, lw - 96 - 134, 26);
    invertToggle.setBounds (lx, 548, 150, 26);

    // CRT
    tabCurve.setBounds (360, 92, 104, 30);
    tabField.setBounds (468, 92, 104, 30);
    barsBox.setBounds (1022, 93, 104, 28);
    loopToggle.setBounds (1132, 94, 72, 28);
    playBtn.setBounds (1210, 92, 80, 30);
    recBtn.setBounds (1296, 92, 80, 30);

    const R bezel (356, 128, 1024, 484);
    const R inner = bezel.reduced (10);
    R bar = inner.withHeight (30).translated (0, 2);
    bar.removeFromLeft (8);
    for (int i = 0; i < 3; ++i) toolBtn[i].setBounds (bar.removeFromLeft (64).reduced (2, 2));
    bar.removeFromLeft (14);
    for (int i = 0; i < 4; ++i) shapeBtn[i].setBounds (bar.removeFromLeft (76).reduced (2, 2));
    bar.removeFromLeft (14);
    snapBox.setBounds (bar.removeFromLeft (118).reduced (2, 2));
    pitchSnapBox.setBounds (bar.removeFromLeft (124).reduced (2, 2));
    bar.removeFromRight (6);
    clearBtn.setBounds (bar.removeFromRight (66).reduced (2, 2));
    delBtn.setBounds (bar.removeFromRight (74).reduced (2, 2));
    fitBtn.setBounds (bar.removeFromRight (48).reduced (2, 2));
    const R screen = inner.withTrimmedTop (36);
    curve.setBounds (screen);
    field.setBounds (screen);

    // sound panel
    sections.clear();
    const int y0 = 636, boxH = 210, cell = 72, kh = 96;
    int x = 20;
    auto section = [&] (const juce::String& title, int nKnobs, int firstKnob)
    {
        const int w = nKnobs * cell + 20;
        sections.push_back ({ juce::Rectangle<float> ((float) x, (float) y0, (float) w, (float) boxH), title });
        for (int i = 0; i < nKnobs; ++i) knobs[(size_t) (firstKnob + i)]->setBounds (x + 10 + i * cell, y0 + 26, cell, kh);
        const int sx = x;
        x += w + 9;
        return sx;
    };
    section ("HETERODYNE OSCILLATOR", 5, 0);
    const int px = section ("PITCH  &  VIBRATO", 4, 5);
    scaleBox.setBounds (px + 12, y0 + 138, 128, 26);
    keyBox.setBounds (px + 144, y0 + 138, 56, 26);
    smartToggle.setBounds (px + 204, y0 + 136, 100, 30);
    const int ex = section ("TAPE ECHO", 3, 9);
    echoSyncToggle.setBounds (ex + 14, y0 + 136, 70, 30);
    echoDivBox.setBounds (ex + 90, y0 + 138, 130, 26);
    section ("REVERB", 3, 12);
    const int ox = section ("OUTPUT", 2, 15);
    badge.setBounds (ox + 8, y0 + boxH - 46, 148, 32);
}

void Editor::setView (bool f)
{
    showingField = f;
    field.setVisible (f);
    curve.setVisible (! f);
    tabCurve.setToggleState (! f, juce::dontSendNotification);
    tabField.setToggleState (f, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &toolBtn[0], &toolBtn[1], &toolBtn[2], &shapeBtn[0], &shapeBtn[1],
                                                             &shapeBtn[2], &shapeBtn[3], &snapBox, &pitchSnapBox, &fitBtn, &clearBtn, &delBtn })
        c->setVisible (! f);
    content.repaint();
}

void Editor::refreshFromPattern()
{
    auto& pat = proc.getPattern();
    int id = 4;
    for (int i = 0; i < (int) std::size (kBarsChoices); ++i) if (kBarsChoices[i] == pat.lengthBars) id = i + 1;
    barsBox.setSelectedId (id, juce::dontSendNotification);
    loopToggle.setToggleState (pat.loop, juce::dontSendNotification);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    curve.fitView();
}

void Editor::updateToolButtons()
{
    for (int i = 0; i < 3; ++i) toolBtn[i].setToggleState ((int) curve.getTool() == i, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i) shapeBtn[i].setToggleState ((int) curve.getNewShape() == i, juce::dontSendNotification);
    delBtn.setEnabled (curve.hasSelection());
}

void Editor::timerCallback()
{
    if (proc.patternReplacedCount.load() != lastPatternCount)
    {
        lastPatternCount = proc.patternReplacedCount.load();
        refreshFromPattern();
    }
    const int mode = (int) proc.getMode();
    if (mode != lastMode)
    {
        if (lastMode != -1 || mode == (int) Mode::Live) setView (mode == (int) Mode::Live);
        else setView (false);
        lastMode = mode;
    }

    scope.refresh();
    valve.setLevel (proc.displayVol.load());
    curve.tick();
    if (showingField) field.tick();

    ++blink;
    const bool rec = proc.recordArmed.load();
    recBtn.setToggleState (rec && (! proc.displayPlaying.load() || (blink / 8) % 2 == 0), juce::dontSendNotification);
    playBtn.setToggleState (proc.displayPlaying.load(), juce::dontSendNotification);

    const auto snap = proc.gamepad().read();
    if (snap.connected != padConnected)
    {
        padConnected = snap.connected;
        padName = padConnected ? proc.gamepad().getDeviceName() : juce::String();
        content.repaint (20, 388, 320, 60);
    }
}

// ---------------------------------------------------------------- painting
void Editor::paintContent (juce::Graphics& g)
{
    const auto& F = fonts();
    g.drawImageAt (textures.enamel (designW, designH), 0, 0);

    // ---- header: chrome band
    const juce::Rectangle<float> head (0, 0, (float) designW, 84);
    juce::ColourGradient hg (juce::Colour (0xfff3f5f6), 0, 0, juce::Colour (0xff8b939a), 0, 84, false);
    hg.addColour (0.42, juce::Colour (0xffd9dee2));
    hg.addColour (0.55, juce::Colour (0xffaab2b8));
    hg.addColour (0.9, juce::Colour (0xffc6ccd1));
    g.setGradientFill (hg);
    g.fillRect (head);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    for (int y = 2; y < 84; y += 2) g.drawHorizontalLine (y, 0, (float) designW);
    g.setColour (col::mintDark);
    g.fillRect (0.0f, 84.0f, (float) designW, 3.0f);
    g.setColour (col::cherry);
    g.fillRect (0.0f, 87.0f, (float) designW, 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (0.0f, 88.5f, (float) designW, 2.0f);
    for (float rx : { 10.0f, (float) designW - 10.0f }) { drawRivet (g, { rx, 12 }, 3.2f); drawRivet (g, { rx, 72 }, 3.2f); }

    // logo
    g.setFont (F.logoFont (44.0f));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawText ("AUTO THEREMIN", juce::Rectangle<float> (31, 9, 520, 52), juce::Justification::centredLeft);
    g.setColour (col::cherryDark);
    g.drawText ("AUTO THEREMIN", juce::Rectangle<float> (30, 8, 520, 52), juce::Justification::centredLeft);
    g.setFont (F.labelFont (9.5f).withExtraKerningFactor (0.32f));
    g.setColour (col::ink.withAlpha (0.75f));
    g.drawText ("ETHERWAVE CURVE SYNTHESIZER", juce::Rectangle<float> (32, 58, 420, 16), juce::Justification::centredLeft);

    g.setFont (F.labelFont (8.0f).withExtraKerningFactor (0.2f));
    g.setColour (col::ink.withAlpha (0.6f));
    g.drawText ("OPERATING MODE", juce::Rectangle<float> (548, 64, 282, 14), juce::Justification::centred);
    g.drawText ("PROGRAMME", juce::Rectangle<float> (880, 64, 358, 14), juce::Justification::centred);
    drawChromeBezel (g, juce::Rectangle<float> (543, 19, 292, 46), 6, 3);

    // ---- left column
    drawEngravedBox (g, juce::Rectangle<float> (20, 396, 320, 216), 6);
    drawSectionPlate (g, juce::Rectangle<float> (40, 388, 160, 18), "LIVE INPUT");
    drawLamp (g, { 224, 397 }, 4.5f, col::mint, padConnected);
    g.setFont (F.monoFont (13.0f));
    g.setColour (col::ink.withAlpha (0.8f));
    g.drawText (padConnected ? padName.toUpperCase().substring (0, 18) : juce::String ("NO CONTROLLER"),
                juce::Rectangle<float> (234, 390, 104, 16), juce::Justification::centredLeft);
    g.setFont (F.labelFont (8.2f));
    g.setColour (col::ink);
    g.drawText ("PITCH BY", juce::Rectangle<float> (40, 440, 92, 26), juce::Justification::centredLeft);
    g.drawText ("VOLUME BY", juce::Rectangle<float> (40, 474, 92, 26), juce::Justification::centredLeft);
    g.drawText ("RANGE FROM", juce::Rectangle<float> (40, 508, 92, 26), juce::Justification::centredLeft);
    g.drawText ("SPAN", juce::Rectangle<float> (40 + 96 + 80, 508, 50, 26), juce::Justification::centredLeft);
    const int shift = proc.liveOctaveShift.load();
    g.setFont (F.monoFont (13.0f));
    g.setColour (col::inkSoft);
    g.drawText ("OCTAVE " + juce::String (shift >= 0 ? "+" : "") + juce::String (shift),
                juce::Rectangle<float> (200, 548, 120, 26), juce::Justification::centredRight);
    g.setFont (F.labelFont (7.6f));
    g.setColour (col::inkSoft.withAlpha (0.9f));
    g.drawFittedText ("PS5 DUALSENSE: R2 = VOLUME, TOUCHPAD = PITCH\nOR CHOOSE STICKS / GYRO TILT ABOVE",
                      juce::Rectangle<int> (40, 576, 280, 30), juce::Justification::centredLeft, 2);

    // ---- CRT bezel
    const juce::Rectangle<float> bezel (356, 128, 1024, 484);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (bezel.translated (2, 5), 14);
    drawChromeBezel (g, bezel, 14, 8);
    g.setColour (juce::Colour (0xff10140f));
    g.fillRoundedRectangle (bezel.reduced (9), 9);
    drawRivet (g, { bezel.getX() + 6, bezel.getY() + 6 }, 2.4f);
    drawRivet (g, { bezel.getRight() - 6, bezel.getY() + 6 }, 2.4f);
    drawRivet (g, { bezel.getX() + 6, bezel.getBottom() - 6 }, 2.4f);
    drawRivet (g, { bezel.getRight() - 6, bezel.getBottom() - 6 }, 2.4f);
    g.setFont (F.labelFont (8.0f).withExtraKerningFactor (0.25f));
    g.setColour (col::ink.withAlpha (0.65f));
    g.drawText ("CATHODE-RAY PLOTTER", juce::Rectangle<float> (590, 92, 260, 30), juce::Justification::centredLeft);

    // ---- sound panel sections
    for (auto& s : sections)
    {
        drawEngravedBox (g, s.r, 6);
        const float pw = juce::jmin (s.r.getWidth() - 24, 40.0f + 8.2f * (float) s.title.length());
        drawSectionPlate (g, juce::Rectangle<float> (s.r.getCentreX() - pw * 0.5f, s.r.getY() - 9, pw, 18), s.title);
    }
    if (! sections.empty())
    {
        auto osc = sections[0].r;
        g.setFont (F.labelFont (7.6f).withExtraKerningFactor (0.18f));
        g.setColour (col::inkSoft.withAlpha (0.8f));
        g.drawFittedText ("TWO COUPLED OSCILLATORS  -  THE AUDIBLE TONE IS THEIR DIFFERENCE",
                          osc.reduced (14, 0).withTrimmedTop (140).withHeight (26).toNearestInt(), juce::Justification::centred, 2);
    }
}
} // namespace at
