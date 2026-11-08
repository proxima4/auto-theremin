#include "CurveEditor.h"
#include "../PluginProcessor.h"

namespace at::ui
{
static const double kSnapBeats[] = { 0.0, 0.125, 0.25, 0.5, 1.0, 2.0, -1.0 };   // -1 = one bar

CurveEditor::CurveEditor (Processor& p) : proc (p), pat (p.getPattern())
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    fitView();
}

double CurveEditor::L() const { return (double) pat.lengthBars * juce::jmax (1, proc.hostQuarterPerBar.load()); }

void CurveEditor::resized()
{
    auto r = getLocalBounds().toFloat().reduced (6, 6);
    ruler = r.removeFromBottom (18);
    piano = r.removeFromLeft (38);
    ruler.removeFromLeft (38);
    volR = r.removeFromBottom (r.getHeight() * 0.2f);
    r.removeFromBottom (8);
    pitchR = r;
    volR = volR.reduced (0, 3);
}

void CurveEditor::fitView()
{
    viewStart = 0;
    viewBeats = juce::jmax (1.0, L());
    float lo = 1000, hi = -1000;
    for (auto& n : pat.pitch.nodes) { lo = juce::jmin (lo, n.value); hi = juce::jmax (hi, n.value); }
    if (lo > hi) { lo = 52; hi = 76; }
    const float span = juce::jmax (18.0f, hi - lo + 8.0f);
    viewNotes = juce::jmin (span, 96.0f);
    viewLow = juce::jlimit (12.0f, 120.0f - viewNotes, (lo + hi) * 0.5f - viewNotes * 0.5f);
    repaint();
}

// ---------------------------------------------------------------- coordinates
float CurveEditor::beatToX (double b) const { return pitchR.getX() + (float) ((b - viewStart) / viewBeats) * pitchR.getWidth(); }
double CurveEditor::xToBeat (float x) const { return viewStart + (double) ((x - pitchR.getX()) / pitchR.getWidth()) * viewBeats; }
float CurveEditor::pitchToY (float p) const { return pitchR.getBottom() - (p - viewLow) / viewNotes * pitchR.getHeight(); }
float CurveEditor::yToPitch (float y) const { return viewLow + (pitchR.getBottom() - y) / pitchR.getHeight() * viewNotes; }
float CurveEditor::volToY (float v) const { return volR.getBottom() - v * volR.getHeight(); }
float CurveEditor::yToVol (float y) const { return juce::jlimit (0.0f, 1.0f, (volR.getBottom() - y) / volR.getHeight()); }

double CurveEditor::snapBeat (double b, bool bypass) const
{
    b = juce::jlimit (0.0, juce::jmax (0.0, L() - 1e-3), b);
    if (bypass || timeSnap == 0) return b;
    const double q = kSnapBeats[timeSnap] < 0 ? (double) juce::jmax (1, proc.hostQuarterPerBar.load()) : kSnapBeats[timeSnap];
    return juce::jlimit (0.0, juce::jmax (0.0, L() - 1e-3), std::round (b / q) * q);
}

float CurveEditor::snapPitch (float p, bool bypass) const
{
    p = juce::jlimit (0.0f, 127.0f, p);
    if (bypass || pitchSnap == 0) return p;
    if (pitchSnap == 1) return std::round (p);
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load();
    const auto mask = scales()[(size_t) (int) proc.apvts.getRawParameterValue ("scale")->load()].mask;
    int lo, hi;
    scaleNeighbours (p, key, mask, lo, hi);
    return (p - (float) lo) < ((float) hi - p) ? (float) lo : (float) hi;
}

int CurveEditor::hitNode (bool pl, juce::Point<float> pos) const
{
    const auto& ns = pl ? pat.pitch.nodes : pat.volume.nodes;
    int best = -1; float bestD = 8.0f;
    for (int i = 0; i < (int) ns.size(); ++i)
    {
        const float d = pos.getDistanceFrom ({ beatToX (ns[(size_t) i].beat), valueToY (pl, ns[(size_t) i].value) });
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

juce::Point<float> CurveEditor::segmentHandle (bool pl, int seg) const
{
    const auto& ln = pl ? pat.pitch : pat.volume;
    const auto& a = ln.nodes[(size_t) seg];
    const auto& b = ln.nodes[(size_t) seg + 1];
    const double mid = 0.5 * (a.beat + b.beat);
    const float v = a.value + (b.value - a.value) * Lane::shapeSegment (0.5f, a.shape, a.tension);
    return { beatToX (mid), valueToY (pl, v) };
}

int CurveEditor::hitTensionHandle (bool pl, juce::Point<float> pos) const
{
    const auto& ns = pl ? pat.pitch.nodes : pat.volume.nodes;
    for (int i = 0; i + 1 < (int) ns.size(); ++i)
    {
        if (ns[(size_t) i].shape == Shape::Hold) continue;
        if (beatToX (ns[(size_t) i + 1].beat) - beatToX (ns[(size_t) i].beat) < 22) continue;
        if (pos.getDistanceFrom (segmentHandle (pl, i)) < 6.5f) return i;
    }
    return -1;
}

void CurveEditor::selectNone()
{
    for (auto& n : pat.pitch.nodes) n.selected = false;
    for (auto& n : pat.volume.nodes) n.selected = false;
}

bool CurveEditor::hasSelection() const
{
    for (auto& n : pat.pitch.nodes) if (n.selected) return true;
    for (auto& n : pat.volume.nodes) if (n.selected) return true;
    return false;
}

void CurveEditor::edited()
{
    proc.patternChanged();
    if (onEdited) onEdited();
    repaint();
}

void CurveEditor::applyShapeToSelection (Shape s)
{
    bool any = false;
    for (auto* ln : { &pat.pitch, &pat.volume })
        for (auto& n : ln->nodes) if (n.selected) { n.shape = s; any = true; }
    if (any) edited();
}

void CurveEditor::deleteSelection()
{
    for (auto* ln : { &pat.pitch, &pat.volume })
        ln->nodes.erase (std::remove_if (ln->nodes.begin(), ln->nodes.end(), [] (const Node& n) { return n.selected; }), ln->nodes.end());
    edited();
}

void CurveEditor::clearAll()
{
    pat.clear();
    edited();
}

void CurveEditor::updateCursor()
{
    setMouseCursor (tool == DrawTool ? juce::MouseCursor::CrosshairCursor
                  : tool == EraseTool ? juce::MouseCursor::PointingHandCursor
                  : juce::MouseCursor::NormalCursor);
}

// ---------------------------------------------------------------- recording
void CurveEditor::tick()
{
    std::vector<RecPoint> pts;
    proc.readRecorded (pts);
    const auto now = juce::Time::getMillisecondCounter();
    for (auto& pt : pts)
    {
        if (! recPass.empty() && pt.beat < recPass.back().beat - 0.01) commitRecordPass();   // loop wrapped
        recPass.push_back (pt);
        lastRecMs = now;
    }
    if (! recPass.empty() && (now - lastRecMs > 300 || recPass.size() > 6000)) commitRecordPass();
    repaint();
}

void CurveEditor::commitRecordPass()
{
    if (recPass.size() < 4) { recPass.clear(); return; }
    const double b0 = recPass.front().beat, b1 = recPass.back().beat;
    std::vector<std::pair<double, float>> pp, vv;
    for (auto& r : recPass) { pp.push_back ({ r.beat, r.pitch }); vv.push_back ({ r.beat, r.vol }); }
    pat.pitch.removeRange (b0, b1);
    pat.volume.removeRange (b0, b1);
    for (auto& n : simplifyTrace (pp, 0.09f)) pat.pitch.insert (n);
    for (auto& n : simplifyTrace (vv, 0.025f)) pat.volume.insert (n);
    recPass.clear();
    edited();
}

// ---------------------------------------------------------------- mouse
void CurveEditor::showNodeMenu (bool pl, int idx)
{
    juce::PopupMenu m;
    auto& n = lane (pl).nodes[(size_t) idx];
    for (int s = 0; s < 4; ++s) m.addItem (1 + s, shapeName ((Shape) s), true, (int) n.shape == s);
    m.addSeparator();
    m.addItem (10, "Reset curve tension", std::abs (n.tension) > 0.001f);
    m.addItem (11, "Delete node");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
        [this, pl, idx] (int r)
        {
            auto& ns = lane (pl).nodes;
            if (! juce::isPositiveAndBelow (idx, (int) ns.size()) || r == 0) return;
            if (r >= 1 && r <= 4)
            {
                const bool sel = ns[(size_t) idx].selected;
                if (sel) applyShapeToSelection ((Shape) (r - 1));
                else { ns[(size_t) idx].shape = (Shape) (r - 1); edited(); }
            }
            else if (r == 10) { ns[(size_t) idx].tension = 0; edited(); }
            else if (r == 11) { ns.erase (ns.begin() + idx); edited(); }
        });
}

void CurveEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    downPos = curPos = pos;
    if (! pitchR.contains (pos) && ! volR.contains (pos)) return;
    const bool pl = pitchR.contains (pos);
    dragInPitch = pl;
    auto& ln = lane (pl);

    if (e.mods.isPopupMenu())
    {
        const int hit = hitNode (pl, pos);
        if (hit >= 0) showNodeMenu (pl, hit);
        return;
    }

    if (tool == DrawTool)
    {
        drag = Drawing;
        drawPts.clear();
        drawPts.push_back ({ juce::jlimit (0.0, L(), xToBeat (pos.x)), yToValue (pl, pos.y) });
        return;
    }
    if (tool == EraseTool) { drag = Erasing; mouseDrag (e); return; }

    // node tool
    const int th = hitTensionHandle (pl, pos);
    int hit = hitNode (pl, pos);
    if (hit < 0 && th >= 0)
    {
        drag = Tension; tensionIdx = th; tensionOrig = ln.nodes[(size_t) th].tension;
        return;
    }
    if (hit < 0 && e.mods.isAltDown())
    {
        // alt-drag anywhere on a segment bends it
        const double b = xToBeat (pos.x);
        for (int i = 0; i + 1 < (int) ln.nodes.size(); ++i)
            if (ln.nodes[(size_t) i].beat <= b && b < ln.nodes[(size_t) i + 1].beat)
            { drag = Tension; tensionIdx = i; tensionOrig = ln.nodes[(size_t) i].tension; return; }
    }
    if (hit < 0 && (e.mods.isShiftDown() || e.mods.isCommandDown()))
    {
        drag = Band;
        if (! e.mods.isShiftDown()) selectNone();
        return;
    }

    if (hit < 0)
    {
        // place a new node
        selectNone();
        Node n;
        n.beat = snapBeat (xToBeat (pos.x), e.mods.isAltDown());
        n.value = pl ? snapPitch (yToPitch (pos.y), e.mods.isAltDown()) : yToVol (pos.y);
        n.shape = newShape;
        n.selected = true;
        hit = ln.insert (n);
        edited();
    }
    else
    {
        auto& n = ln.nodes[(size_t) hit];
        if (e.mods.isShiftDown()) n.selected = ! n.selected;
        else if (! n.selected) { selectNone(); n.selected = true; }
        if (! n.selected) { repaint(); return; }
    }

    // start moving the selection in this lane
    primaryOrig = ln.nodes[(size_t) hit];
    dragBase.clear(); dragMoved.clear();
    for (auto& n : ln.nodes) (n.selected ? dragMoved : dragBase).push_back (n);
    drag = Move;
    if (pl && ! proc.displayPlaying.load())
    {
        proc.auditionPitch.store (primaryOrig.value);
        proc.auditionOn.store (true);
        auditioning = true;
    }
    repaint();
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    curPos = e.position;
    mousePos = e.position;
    const bool pl = dragInPitch;
    auto& ln = lane (pl);
    const bool bypass = e.mods.isAltDown();

    switch (drag)
    {
        case Move:
        {
            const double nb = snapBeat (xToBeat (beatToX (primaryOrig.beat) + (curPos.x - downPos.x)), bypass);
            const double dB = nb - primaryOrig.beat;
            float dV;
            if (pl) dV = snapPitch (primaryOrig.value + (yToPitch (curPos.y) - yToPitch (downPos.y)), bypass) - primaryOrig.value;
            else    dV = (yToVol (curPos.y) - yToVol (downPos.y));
            ln.nodes = dragBase;
            for (auto n : dragMoved)
            {
                n.beat = juce::jlimit (0.0, juce::jmax (0.0, L() - 1e-3), n.beat + dB);
                n.value = pl ? juce::jlimit (0.0f, 127.0f, n.value + dV) : juce::jlimit (0.0f, 1.0f, n.value + dV);
                ln.nodes.push_back (n);
            }
            ln.sort();
            if (auditioning) proc.auditionPitch.store (primaryOrig.value + dV);
            edited();
            break;
        }
        case Tension:
        {
            if (! juce::isPositiveAndBelow (tensionIdx, (int) ln.nodes.size())) break;
            auto& a = ln.nodes[(size_t) tensionIdx];
            const bool rising = tensionIdx + 1 < (int) ln.nodes.size() && ln.nodes[(size_t) tensionIdx + 1].value > a.value;
            const float d = (curPos.y - downPos.y) / 90.0f * (rising ? 1.0f : -1.0f);
            a.tension = juce::jlimit (-1.0f, 1.0f, tensionOrig + d);
            edited();
            break;
        }
        case Drawing:
        {
            const double b = juce::jlimit (0.0, juce::jmax (0.0, L() - 1e-3), xToBeat (curPos.x));
            const float v = pl ? juce::jlimit (0.0f, 127.0f, yToPitch (curPos.y)) : yToVol (curPos.y);
            drawPts.push_back ({ b, v });
            repaint();
            break;
        }
        case Erasing:
        {
            bool any = false;
            for (bool lanePl : { true, false })
            {
                auto& ns = lane (lanePl).nodes;
                for (int i = (int) ns.size() - 1; i >= 0; --i)
                    if (curPos.getDistanceFrom ({ beatToX (ns[(size_t) i].beat), valueToY (lanePl, ns[(size_t) i].value) }) < 9.0f)
                    { ns.erase (ns.begin() + i); any = true; }
            }
            if (any) edited();
            break;
        }
        case Band:
        {
            const auto band = juce::Rectangle<float> (downPos, curPos);
            for (auto& n : ln.nodes)
                if (band.contains (beatToX (n.beat), valueToY (pl, n.value))) n.selected = true;
                else if (! e.mods.isShiftDown()) n.selected = false;
            repaint();
            break;
        }
        case None: break;
    }
}

void CurveEditor::mouseUp (const juce::MouseEvent&)
{
    if (auditioning) { proc.auditionOn.store (false); auditioning = false; }
    if (drag == Drawing && drawPts.size() > 1)
    {
        // order by time and merge points drawn back over themselves
        std::stable_sort (drawPts.begin(), drawPts.end(), [] (auto& a, auto& b) { return a.first < b.first; });
        std::vector<std::pair<double, float>> clean;
        for (auto& pt : drawPts)
            if (clean.empty() || pt.first - clean.back().first > 1e-4) clean.push_back (pt);
            else clean.back().second = pt.second;
        if (clean.size() > 1)
        {
            auto& ln = lane (dragInPitch);
            ln.removeRange (clean.front().first, clean.back().first);
            for (auto& n : simplifyTrace (clean, dragInPitch ? 0.18f : 0.025f)) ln.insert (n);
            edited();
        }
    }
    else if (drag == Drawing && drawPts.size() == 1)
    {
        Node n; n.beat = snapBeat (drawPts[0].first, false);
        n.value = dragInPitch ? snapPitch (drawPts[0].second, false) : drawPts[0].second;
        n.shape = newShape;
        lane (dragInPitch).insert (n);
        edited();
    }
    drawPts.clear();
    if (drag == Band || drag == Move) { if (onEdited) onEdited(); }
    drag = None;
    repaint();
}

void CurveEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (tool != NodeTool) return;
    for (bool pl : { true, false })
    {
        const auto& r = pl ? pitchR : volR;
        if (! r.contains (e.position)) continue;
        const int hit = hitNode (pl, e.position);
        if (hit >= 0) { lane (pl).nodes.erase (lane (pl).nodes.begin() + hit); edited(); }
    }
}

void CurveEditor::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;
    hoverInPitch = pitchR.contains (e.position);
    hoverNode = (hoverInPitch || volR.contains (e.position)) ? hitNode (hoverInPitch, e.position) : -1;
    hoverSeg = hoverNode < 0 && tool == NodeTool && (hoverInPitch || volR.contains (e.position)) ? hitTensionHandle (hoverInPitch, e.position) : -1;
    repaint();
}

void CurveEditor::mouseExit (const juce::MouseEvent&) { mousePos = { -100, -100 }; hoverNode = hoverSeg = -1; repaint(); }

void CurveEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float dy = (std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double anchor = xToBeat (e.position.x);
        const double nv = juce::jlimit (1.0, juce::jmax (1.0, L()), viewBeats * (dy > 0 ? 0.85 : 1.0 / 0.85));
        viewStart = anchor - (anchor - viewStart) * (nv / viewBeats);
        viewBeats = nv;
    }
    else if (e.mods.isShiftDown())
        viewStart -= dy * viewBeats * 0.25;
    else if (e.mods.isAltDown())
    {
        const float anchor = yToPitch (e.position.y);
        const float nv = juce::jlimit (6.0f, 96.0f, viewNotes * (dy > 0 ? 0.88f : 1.0f / 0.88f));
        viewLow = anchor - (anchor - viewLow) * (nv / viewNotes);
        viewNotes = nv;
    }
    else
        viewLow += dy * viewNotes * 0.2f;

    viewStart = juce::jlimit (0.0, juce::jmax (0.0, L() - viewBeats), viewStart);
    viewLow = juce::jlimit (0.0f, 127.0f - viewNotes, viewLow);
    repaint();
}

bool CurveEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { deleteSelection(); return true; }
    if (k == juce::KeyPress::escapeKey) { selectNone(); repaint(); return true; }
    if (k.getModifiers().isCommandDown() && k.getKeyCode() == 'A')
    {
        for (auto* ln : { &pat.pitch, &pat.volume }) for (auto& n : ln->nodes) n.selected = true;
        repaint(); return true;
    }
    const auto c = k.getTextCharacter();
    if (c >= '1' && c <= '4') { applyShapeToSelection ((Shape) (c - '1')); return true; }
    return false;
}

// ---------------------------------------------------------------- painting
void CurveEditor::paint (juce::Graphics& g)
{
    auto all = getLocalBounds().toFloat();
    juce::ColourGradient bg (juce::Colour (0xff072414), all.getCentreX(), all.getCentreY(), col::crtBlack, all.getX(), all.getY(), true);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (all, 10);

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (all.reduced (2).toNearestInt());
        paintGrid (g);
        paintLanes (g);
        paintOverlay (g);
    }
    static Textures tex;
    g.setOpacity (0.55f);
    g.drawImageAt (tex.scanlines (getWidth(), getHeight()), 0, 0);
    g.setOpacity (1.0f);
    drawCrtGlass (g, all, 10);
}

void CurveEditor::paintGrid (juce::Graphics& g)
{
    const auto green = col::phosphor;
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load();
    const int scaleIdx = (int) proc.apvts.getRawParameterValue ("scale")->load();
    const auto mask = scales()[(size_t) scaleIdx].mask;
    const float rowH = pitchR.getHeight() / viewNotes;

    // note rows
    for (int nnum = (int) std::floor (viewLow); nnum <= (int) std::ceil (viewLow + viewNotes); ++nnum)
    {
        const float y0 = pitchToY ((float) nnum + 0.5f), y1 = pitchToY ((float) nnum - 0.5f);
        if (y1 < pitchR.getY() || y0 > pitchR.getBottom()) continue;
        const int pc = ((nnum % 12) + 12) % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        if (scaleIdx != 0 && inScale (nnum, key, mask))
        {
            g.setColour (green.withAlpha (0.045f));
            g.fillRect (juce::Rectangle<float> (pitchR.getX(), y0, pitchR.getWidth(), y1 - y0).getIntersection (pitchR));
        }
        // piano strip
        auto keyR = juce::Rectangle<float> (piano.getX(), y0, piano.getWidth() - 4, y1 - y0).getIntersection (pitchR.withX (piano.getX()).withWidth (piano.getWidth()));
        g.setColour (black ? green.withAlpha (0.10f) : green.withAlpha (0.26f));
        g.fillRect (keyR.reduced (0, rowH > 5 ? 0.5f : 0.0f).withWidth (black ? keyR.getWidth() * 0.62f : keyR.getWidth()));
        if (rowH > 4.5f)
        {
            g.setColour (green.withAlpha (pc == 0 ? 0.20f : 0.06f));
            g.drawHorizontalLine ((int) y1, pitchR.getX(), pitchR.getRight());
        }
        if (pc == 0 && rowH * 12 > 30)
        {
            g.setColour (green.withAlpha (0.9f));
            g.setFont (fonts().monoFont (juce::jlimit (10.0f, 13.0f, rowH * 1.6f)));
            g.drawText (noteName (nnum), juce::Rectangle<float> (piano.getX(), y1 - 13, piano.getWidth() - 6, 13), juce::Justification::centredRight);
        }
    }

    // beat / bar lines
    const int qpb = juce::jmax (1, proc.hostQuarterPerBar.load());
    const float pxPerBeat = pitchR.getWidth() / (float) viewBeats;
    double sub = kSnapBeats[timeSnap] > 0 ? kSnapBeats[timeSnap] : 1.0;
    while (sub * pxPerBeat < 9.0) sub *= 2.0;
    const double endB = juce::jmin (viewStart + viewBeats, L());
    for (double b = std::ceil (viewStart / sub) * sub; b <= viewStart + viewBeats + 1e-6; b += sub)
    {
        const float x = beatToX (b);
        const bool bar = std::abs (std::fmod (b, (double) qpb)) < 1e-6;
        const bool beat = std::abs (std::fmod (b, 1.0)) < 1e-6;
        g.setColour (green.withAlpha (bar ? 0.30f : beat ? 0.12f : 0.05f));
        g.drawVerticalLine ((int) x, pitchR.getY(), pitchR.getBottom());
        g.drawVerticalLine ((int) x, volR.getY(), volR.getBottom());
        if (bar && b < L() - 1e-6)
        {
            g.setColour (green.withAlpha (0.85f));
            g.setFont (fonts().monoFont (13.0f));
            g.drawText (juce::String ((int) (b / qpb) + 1), juce::Rectangle<float> (x + 3, ruler.getY(), 40, ruler.getHeight()), juce::Justification::centredLeft);
        }
    }
    // beyond the pattern end
    if (endB < viewStart + viewBeats)
    {
        const float x = beatToX (L());
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (juce::Rectangle<float> (x, pitchR.getY(), pitchR.getRight() - x, volR.getBottom() - pitchR.getY()));
        g.setColour (col::amber.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, pitchR.getY(), volR.getBottom());
        g.setFont (fonts().monoFont (12.0f));
        g.drawText (pat.loop ? "LOOP" : "END", juce::Rectangle<float> (x + 4, pitchR.getY() + 2, 60, 14), juce::Justification::centredLeft);
    }

    // lane frames
    g.setColour (green.withAlpha (0.35f));
    g.drawRect (pitchR, 1.0f);
    g.drawRect (volR, 1.0f);
    g.setFont (fonts().monoFont (12.0f));
    g.setColour (green.withAlpha (0.7f));
    g.drawText ("VOL", juce::Rectangle<float> (piano.getX(), volR.getY(), piano.getWidth() - 6, volR.getHeight()), juce::Justification::centredRight);
}

void CurveEditor::paintLanes (juce::Graphics& g)
{
    const auto green = col::phosphor;
    const double len = L();
    const bool hasVol = ! pat.volume.nodes.empty();

    // ---- volume lane: filled area
    if (hasVol)
    {
        juce::Path area, line;
        int h = 0;
        bool started = false;
        for (float x = volR.getX(); x <= volR.getRight(); x += 2.0f)
        {
            const double b = xToBeat (x);
            if (b < 0 || b > len) continue;
            const float y = volToY (pat.volume.evaluate (b, h));
            if (! started) { area.startNewSubPath (x, volR.getBottom()); area.lineTo (x, y); line.startNewSubPath (x, y); started = true; }
            else { area.lineTo (x, y); line.lineTo (x, y); }
        }
        if (started)
        {
            area.lineTo (line.getCurrentPosition().x, volR.getBottom());
            area.closeSubPath();
            g.setColour (green.withAlpha (0.12f));
            g.fillPath (area);
            drawGlowPath (g, line, green, 1.4f, 0.6f);
        }
    }
    else
    {
        g.setColour (green.withAlpha (0.4f));
        g.setFont (fonts().monoFont (12.0f));
        g.drawText ("no volume nodes: full volume", volR.reduced (8, 0), juce::Justification::centredLeft);
    }

    // ---- pitch lane: brightness follows the volume so silent parts look dim
    if (! pat.pitch.nodes.empty())
    {
        juce::Path ghost;
        juce::Path runs[5];
        int hp = 0, hv = 0, prevLvl = -1;
        juce::Point<float> prev;
        bool started = false;
        for (float x = pitchR.getX(); x <= pitchR.getRight() + 1; x += 1.5f)
        {
            const double b = xToBeat (x);
            if (b < 0 || b > len) continue;
            const juce::Point<float> pt (x, pitchToY (pat.pitch.evaluate (b, hp)));
            const float v = hasVol ? pat.volume.evaluate (b, hv) : 0.8f;
            const int lvl = v < 0.02f ? -1 : juce::jlimit (0, 4, (int) (v * 5.0f));
            if (! started) { ghost.startNewSubPath (pt); started = true; }
            else
            {
                ghost.lineTo (pt);
                if (lvl >= 0)
                {
                    if (lvl != prevLvl || runs[lvl].isEmpty()) runs[lvl].startNewSubPath (prev);
                    runs[lvl].lineTo (pt);
                }
            }
            prev = pt; prevLvl = lvl;
        }
        g.setColour (green.withAlpha (0.22f));
        g.strokePath (ghost, juce::PathStrokeType (1.0f));
        for (int i = 0; i < 5; ++i)
            if (! runs[i].isEmpty()) drawGlowPath (g, runs[i], green.withAlpha (0.45f + 0.11f * i), 1.6f + 0.35f * i, 0.5f + 0.12f * i);
    }

    // ---- nodes and tension handles
    for (bool pl : { true, false })
    {
        const auto& ns = pl ? pat.pitch.nodes : pat.volume.nodes;
        for (int i = 0; i < (int) ns.size(); ++i)
        {
            const auto& n = ns[(size_t) i];
            const juce::Point<float> c (beatToX (n.beat), valueToY (pl, n.value));
            if (c.x < pitchR.getX() - 6 || c.x > pitchR.getRight() + 6) continue;
            const bool hov = hoverNode == i && hoverInPitch == pl;
            const float r = hov ? 5.5f : 4.2f;
            if (n.selected)
            {
                g.setColour (green.withAlpha (0.25f)); g.fillEllipse (c.x - r * 2, c.y - r * 2, r * 4, r * 4);
                g.setColour (green); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
                g.setColour (juce::Colours::white); g.fillEllipse (c.x - 1.6f, c.y - 1.6f, 3.2f, 3.2f);
            }
            else
            {
                g.setColour (col::crtBlack); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
                g.setColour (green.withAlpha (hov ? 1.0f : 0.85f)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.5f);
            }
        }
    }
    if (hoverSeg >= 0 || drag == Tension)
    {
        const int seg = drag == Tension ? tensionIdx : hoverSeg;
        const bool pl = drag == Tension ? dragInPitch : hoverInPitch;
        if (seg >= 0 && seg + 1 < (int) lane (pl).nodes.size())
        {
            const auto c = segmentHandle (pl, seg);
            juce::Path d; d.addPolygon (c, 4, 5.0f, juce::MathConstants<float>::pi * 0.25f);
            g.setColour (col::amber); g.strokePath (d, juce::PathStrokeType (1.5f));
        }
    }

    // ---- freehand stroke in progress
    if (drag == Drawing && drawPts.size() > 1)
    {
        juce::Path p;
        for (size_t i = 0; i < drawPts.size(); ++i)
        {
            const juce::Point<float> pt (beatToX (drawPts[i].first), valueToY (dragInPitch, drawPts[i].second));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        drawGlowPath (g, p, col::amber, 1.6f);
    }
    // ---- recording in progress
    if (! recPass.empty())
    {
        juce::Path p;
        for (size_t i = 0; i < recPass.size(); ++i)
        {
            const juce::Point<float> pt (beatToX (recPass[i].beat), pitchToY (recPass[i].pitch));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        drawGlowPath (g, p, juce::Colour (0xffff5a46), 1.8f);
    }
    if (drag == Band)
    {
        g.setColour (green.withAlpha (0.12f));
        g.fillRect (juce::Rectangle<float> (downPos, curPos));
        g.setColour (green.withAlpha (0.6f));
        g.drawRect (juce::Rectangle<float> (downPos, curPos), 1.0f);
    }
}

void CurveEditor::paintOverlay (juce::Graphics& g)
{
    const auto green = col::phosphor;
    // playhead
    if (proc.displayPlaying.load())
    {
        const double b = proc.displayBeat.load();
        const float x = beatToX (b);
        if (x >= pitchR.getX() && x <= pitchR.getRight())
        {
            juce::Path ph; ph.startNewSubPath (x, pitchR.getY()); ph.lineTo (x, volR.getBottom());
            drawGlowPath (g, ph, col::amber, 1.2f, 0.7f);
            const float y = pitchToY (proc.displayPitch.load());
            const float v = proc.displayVol.load();
            if (y > pitchR.getY() && y < pitchR.getBottom())
            {
                const float r = 4.0f + 6.0f * v;
                juce::ColourGradient gl (col::amber.withAlpha (0.8f), x, y, col::amber.withAlpha (0.0f), x + r * 2.5f, y, true);
                g.setGradientFill (gl);
                g.fillEllipse (x - r * 2.5f, y - r * 2.5f, r * 5, r * 5);
                g.setColour (juce::Colours::white);
                g.fillEllipse (x - 2.5f, y - 2.5f, 5, 5);
            }
        }
    }

    // cursor readout
    g.setFont (fonts().monoFont (14.0f));
    if (pitchR.contains (mousePos) || volR.contains (mousePos))
    {
        const int qpb = juce::jmax (1, proc.hostQuarterPerBar.load());
        const double b = xToBeat (mousePos.x);
        juce::String s = "BAR " + juce::String ((int) (b / qpb) + 1) + "." + juce::String ((int) std::fmod (b, (double) qpb) + 1);
        if (pitchR.contains (mousePos))
        {
            const float p = yToPitch (mousePos.y);
            const int nn = juce::roundToInt (p);
            s << "   " << noteName (nn) << " " << (p - nn >= 0 ? "+" : "") << juce::roundToInt ((p - nn) * 100) << "c";
        }
        else s << "   VOL " << juce::roundToInt (yToVol (mousePos.y) * 100) << "%";
        g.setColour (green.withAlpha (0.85f));
        g.drawText (s, pitchR.reduced (8, 4).removeFromTop (16), juce::Justification::topLeft);
    }

    // status, top right
    juce::String st;
    const auto mode = proc.getMode();
    st << (mode == Mode::Auto ? "AUTO" : mode == Mode::Live ? "LIVE" : "MIDI");
    st << "  " << juce::String (proc.currentBpm.load(), 1) << " BPM";
    if (proc.recordArmed.load() && mode != Mode::Auto) st << "  REC";
    g.setColour (proc.recordArmed.load() && mode != Mode::Auto ? juce::Colour (0xffff5a46) : green.withAlpha (0.75f));
    g.drawText (st, pitchR.reduced (8, 4).removeFromTop (16), juce::Justification::topRight);

    if (pat.pitch.nodes.empty() && drag != Drawing)
    {
        g.setColour (green.withAlpha (0.65f));
        g.setFont (fonts().monoFont (18.0f));
        g.drawFittedText ("CLICK TO PLACE NOTES  -  DRAG TO SHAPE THE CURVE\nDRAW TOOL FOR FREEHAND  -  OR RECORD A LIVE TAKE",
                          pitchR.toNearestInt(), juce::Justification::centred, 2);
    }
}
} // namespace at::ui
