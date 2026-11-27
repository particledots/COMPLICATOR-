#include "PluginEditor.h"

using namespace CxStyle;
using Kind = CxSegments::Kind;

namespace
{
    constexpr float kDefaultScale = 1.2f;         // the window opens at 120% (resizable from 90% to 180%)

    // Section geometry (panel units), shared by the controls and the artwork.
    constexpr float kOscX[2] = { 14.5f, 459.5f };
    constexpr float kOscW = 206.0f, kCentreX = 240.5f, kCentreW = 199.0f;
    constexpr float kPresetY = 48.5f, kPresetH = 36.0f;
    constexpr float kOscY = 90.5f, kOscH = 300.0f;
    constexpr float kMultY = kOscY + 216.0f;
    constexpr float kIndexY = kOscY + kOscH + 6.0f, kIndexH = 78.0f;
    constexpr float kPatchY = kIndexY + kIndexH + 6.0f, kPatchH = 100.0f;
    constexpr float kMixY = kPatchY + kPatchH + 6.0f, kMixH = 108.0f;

    // Rows inside an oscillator (relative to the frame's top)
    constexpr float kTuneCy = 48.0f, kFmCy = 136.0f, kScopeTop = 192.0f, kScopeH = 40.0f, kFoldCy = 264.0f;
    constexpr float kScopeLeft = 12.0f, kScopeWidth = 182.0f;

    constexpr float kMixX[11] = { 46, 80, 114, 148, 182, 226, 270, 304, 338, 372, 406 };
    constexpr float kEnvX[4] = { 470, 504, 538, 572 };
    constexpr float kMixBus = kMixY + 94.0f;
    constexpr float kVolumeX = 622.0f;

    juce::String semisText (float v)
    {
        const bool whole = std::fabs (v - std::round (v)) < 0.005f;
        const juce::String num = whole ? juce::String (juce::roundToInt (v)) : juce::String (v, 2);
        return (v > 0.0049f ? "+" : "") + num + " st";
    }

    // The frame of a section with its title set into the top line.
    void frame (juce::Graphics& g, float x, float y, float w, float h, const juce::String& title)
    {
        g.setColour (gold());
        g.drawRoundedRectangle (x, y, w, h, 10.0f, 0.9f);
        const float tw = widthOf (title, capsFont());
        const float tx = x + 18.0f;
        g.setColour (panel());
        g.fillRect (tx - 6.0f, y - 5.0f, tw + 12.0f, 10.0f);
        textIn (g, title, capsFont(), tx, y + 4.0f, goldText(), juce::Justification::centredLeft);
    }

    void node (juce::Graphics& g, float x, float y)
    {
        g.setColour (panel());
        g.fillEllipse (x - 2.6f, y - 2.6f, 5.2f, 5.2f);
        g.setColour (gold());
        g.drawEllipse (x - 2.6f, y - 2.6f, 5.2f, 5.2f, 0.9f);
    }

    void stroke (juce::Graphics& g, float x1, float y1, float x2, float y2)
    {
        g.setColour (gold());
        if (x1 == x2) g.fillRect (x1 - 0.45f, juce::jmin (y1, y2), 0.9f, std::fabs (y2 - y1));
        else          g.fillRect (juce::jmin (x1, x2), y1 - 0.45f, std::fabs (x2 - x1), 0.9f);
    }

    // A knob's gold range arc (7:30 round to 4:30) with a dot at each end.
    void arc (juce::Graphics& g, float cx, float cy, float ring)
    {
        juce::Path p;
        p.addCentredArc (cx, cy, ring, ring, 0.0f, juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        g.setColour (gold());
        g.strokePath (p, juce::PathStrokeType (0.9f));
        const float d = ring * 0.70710678f;
        g.fillEllipse (cx - d - 1.8f, cy + d - 1.8f, 3.6f, 3.6f);
        g.fillEllipse (cx + d - 1.8f, cy + d - 1.8f, 3.6f, 3.6f);
    }

    // A knob with its range arc and its label under it.
    void knobArt (juce::Graphics& g, float cx, float cy, float ring, const char* label, float labelY)
    {
        arc (g, cx, cy, ring);
        textIn (g, label, labelFont(), cx, labelY, text(), juce::Justification::centred);
    }

    void rowTitle (juce::Graphics& g, const char* s, float x, float y, juce::Justification j = juce::Justification::centredLeft)
    {
        textIn (g, s, rowFont(), x, y, goldText(), j);
    }

    void note (juce::Graphics& g, const juce::String& s, float x, float y, juce::Justification j)
    {
        textIn (g, s, font (10.0f, false, 0.2f), x, y, muted(), j);
    }
}

// ================================================================================================ panel

ComplicatorPanel::ComplicatorPanel (ComplicatorAudioProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.getValueTreeState();
    using namespace CxParams;

    // The fixed artwork, drawn once into a stored picture
    backdrop.setBounds (0, 0, panelW, panelH);
    backdrop.setInterceptsMouseClicks (false, false);
    backdrop.setBufferedToImage (true);
    addAndMakeVisible (backdrop);

    // Preset row: the Preset menu (Init + the factory presets) and the Quality menu
    presetMenu.setName ("Preset");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetMenu.addItem (proc.getProgramName (i), i + 1);
    presetMenu.setBounds (30, 58, 420, 22);
    addAndMakeVisible (presetMenu);
    presetMenu.onChange = [this]
    {
        const int id = presetMenu.getSelectedId();
        if (id > 0)
            proc.setCurrentProgram (id - 1);
    };
    updatePresetMenu();
    auto& q = menu (quality, "Quality", { 552, 58, 100, 22 });
    qualityMenu = &q;
    q.changeItemText (3, "8x heavy");                                 // the list's third entry (8x) is the CPU-hungry one

    // The two oscillators
    for (int o = 0; o < 2; ++o)
    {
        const bool isA = o == 0;
        const float fx = kOscX[o], fy = kOscY;
        knob (isA ? aCoarse : bCoarse, fx + 34, fy + kTuneCy, 22);
        knob (isA ? aFine : bFine,     fx + 82, fy + kTuneCy, 15);
        knob (isA ? aPwm : bPwm,       fx + 118, fy + kTuneCy, 15);
        if (isA) segs (aLfo, { "LFO" }, { fx + 142, fy + kTuneCy - 13, CxSegments::optionWidth ("LFO"), 26 }, Kind::Option);
        else     segs (bSub, { "OFF", "-1 OCT", "-2 OCT" }, { fx + 138, fy + 38, 62, 45 }, Kind::List);
        knob (isA ? aFm : bFm, fx + 34, fy + kFmCy, 15);
        segs (isA ? aFmCurve : bFmCurve, { "LIN", "EXP", "TZ" }, { fx + 66, fy + kFmCy - 10, 132, 20 }, Kind::Pill);

        // Wavefold fader, its CV amount, symmetry.
        auto& f = isA ? foldA : foldB;
        f.setSliderStyle (juce::Slider::LinearHorizontal);
        f.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        f.setBounds (juce::roundToInt (fx + 12), juce::roundToInt (fy + kFoldCy - 10), 96, 20);
        addAndMakeVisible (f);
        (isA ? foldAAtt : foldBAtt) = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, isA ? aFold : bFold, f);
        f.setDoubleClickReturnValue (true, 0.5);                                              // back to the plain sine
        f.setVelocityModeParameters (0.6, 1, 0.0, true, juce::ModifierKeys::shiftModifier);   // hold Shift for fine moves
        knob (isA ? aFoldCv : bFoldCv, fx + 142, fy + kFoldCy, 12);
        knob (isA ? aSym : bSym,       fx + 182, fy + kFoldCy, 12);
    }

    // Centre: pitch and sync, multiply
    const float cx = kCentreX + 12.0f;
    auto rowY = [] (int i) { return kOscY + 26.0f + 37.0f * (float) i + 5.0f; };
    segs (keys, { "A", "A+B", "B" }, { cx, rowY (0), 175, 20 }, Kind::Pill);
    segs (pitchSnap, { "OFF", "SEMI", "STEPS" }, { cx, rowY (1), 175, 20 }, Kind::Pill);
    menu (pitchScale, "Scale", { cx, rowY (2), 175, 20 });
    menu (pitchKey, "Key", { cx, rowY (3), 175, 20 });
    segs (CxParams::sync, { "OFF", "SOFT", "HARD" }, { cx, rowY (4), 175, 20 }, Kind::Pill);
    knob (multDepth, kCentreX + 40, kMultY + 40, 15);
    segs (multMode, { "RING", "HALF", "FULL" }, { kCentreX + 100, kMultY + 27, 70, 45 }, Kind::List);

    // Index: its CV amount and the routes
    knob (indexCv, 46, kIndexY + 40, 12);
    knob (CxParams::index, 104, kIndexY + 40, 17);
    const char* routeA[3] = { routeFmA, routeFoldA, routeSymA };
    const char* routeB[3] = { routeFmB, routeFoldB, routeSymB };
    const float routeX[3] = { 176, 266, 356 };
    for (int i = 0; i < 3; ++i)
    {
        segs (routeA[i], { "A" }, { routeX[i], kIndexY + 40 - 9, CxSegments::optionWidth ("A"), 18 }, Kind::Option);
        segs (routeB[i], { "B" }, { routeX[i], kIndexY + 58 - 9, CxSegments::optionWidth ("B"), 18 }, Kind::Option);
    }
    segs (routePwm, { "OFF", "BOTH", juce::String::fromUTF8 ("A \xe2\x86\x92 B") }, { 452, kIndexY + 31, 70, 45 }, Kind::List);

    // Patch row: 13 menus in 7 columns
    for (int i = 0; i < numPatch; ++i)
        menu (patchIds[i], patchShort[i], { 26.5f + (float) (i % 7) * 90.5f, kPatchY + 26.0f + (float) (i / 7) * 38.0f + 4.0f, 84, 22 });

    // Output mix
    for (int i = 0; i < numMix; ++i)
        knob (mixIds[i], kMixX[i], kMixY + 54, 14);
    knob (envAttack,  kEnvX[0], kMixY + 52, 11);
    knob (envDecay,   kEnvX[1], kMixY + 52, 11);
    knob (envSustain, kEnvX[2], kMixY + 52, 11);
    knob (envRelease, kEnvX[3], kMixY + 52, 11);
    segs (ampMode, { "DRONE", "ENV" }, { 454, kMixBus - 10, 114, 20 }, Kind::Pill);
    knob (volume, kVolumeX, kMixY + 52, 17);

    proc.setScopeActive (true);          // the engine records Final A / Final B while the panel is open
    startTimerHz (30);
}

ComplicatorPanel::~ComplicatorPanel()
{
    proc.setScopeActive (false);
    stopTimer();
    setLookAndFeel (nullptr);
}

void ComplicatorPanel::updatePresetMenu()
{
    const int shown = proc.shownPreset();
    if (shown >= 0)
        presetMenu.setSelectedId (shown + 1, juce::dontSendNotification);
    else
    {
        presetMenu.setSelectedId (0, juce::dontSendNotification);
        presetMenu.setText ("Custom", juce::dontSendNotification);   // settings from a project saved before presets
    }
    shownPresetId = shown;
}

CxKnob& ComplicatorPanel::knob (const char* id, float cx, float cy, float ring)
{
    knobs.push_back (std::make_unique<CxKnob> (proc.getValueTreeState(), id));
    auto& k = *knobs.back();
    k.setBounds (juce::roundToInt (cx - ring), juce::roundToInt (cy - ring), juce::roundToInt (2.0f * ring), juce::roundToInt (2.0f * ring));
    addAndMakeVisible (k);
    return k;
}

CxSegments& ComplicatorPanel::segs (const char* id, juce::StringArray labels, juce::Rectangle<float> r, Kind kind)
{
    auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.getValueTreeState().getParameter (id));
    jassert (p != nullptr);
    segments.push_back (std::make_unique<CxSegments> (*p, std::move (labels), kind));
    auto& s = *segments.back();
    s.setBounds (r.toNearestInt());
    addAndMakeVisible (s);
    return s;
}

CxMenu& ComplicatorPanel::menu (const char* id, const juce::String& name, juce::Rectangle<float> r)
{
    menus.push_back (std::make_unique<CxMenu> (proc.getValueTreeState(), id, name));
    auto& m = *menus.back();
    m.setBounds (r.toNearestInt());
    addAndMakeVisible (m);
    return m;
}

void ComplicatorPanel::attachPopups (juce::Component* parent)
{
    for (auto& k : knobs)
        k->setPopupDisplayEnabled (true, true, parent);
    foldA.setPopupDisplayEnabled (true, true, parent);
    foldB.setPopupDisplayEnabled (true, true, parent);
}

// Reads the newest part of one Final's recording and turns it into 213 points: a steady picture of one or two cycles
// (starting at a rising crossing of the average), or, for signals too slow for that (an LFO), the newest 1024 samples.
void ComplicatorPanel::updateScope (int which)
{
    constexpr int N = 2048;
    float raw[N];
    proc.getEngine().scopeCopy (which, raw, N);

    double mean = 0.0;
    for (float v : raw) mean += (double) v;
    mean /= (double) N;

    // rising crossings of the average, with a little hysteresis so that ripple does not count
    double first = -1.0, last = -1.0;
    int count = 0;
    bool below = raw[0] < mean - 0.02;
    for (int i = 1; i < N; ++i)
    {
        if (raw[i] < mean - 0.02) below = true;
        else if (below && raw[i] >= mean)
        {
            const double pos = (double) (i - 1) + (mean - raw[i - 1]) / std::max (1.0e-9, (double) raw[i] - raw[i - 1]);
            if (first < 0.0) first = pos;
            last = pos;
            ++count;
            below = false;
        }
    }

    double start = (double) (N - 1024), window = 1024.0;
    if (count >= 2)
    {
        const double period = (last - first) / (double) (count - 1);
        const int cycles = std::min (2, (int) std::floor (((double) N - 2.0 - first) / period));
        if (period >= 4.0 && cycles >= 1) { start = first; window = period * (double) cycles; }
    }

    auto& line = scopeLine[(size_t) which];
    for (int i = 0; i <= kScopeW; ++i)
    {
        const double pos = start + window * (double) i / (double) kScopeW;
        const int k = juce::jlimit (0, N - 2, (int) pos);
        const float fr = (float) (pos - (double) k);
        line[(size_t) i] = raw[k] + (raw[k + 1] - raw[k]) * fr;
    }
}

// The areas redrawn 30 times a second (panel units): the two scopes and the two Coarse value texts.
juce::Array<juce::Rectangle<int>> ComplicatorPanel::liveAreas()
{
    juce::Array<juce::Rectangle<int>> a;
    for (int o = 0; o < 2; ++o)
    {
        a.add (juce::Rectangle<float> (kOscX[o] + kScopeLeft, kOscY + kScopeTop, kScopeWidth, kScopeH).expanded (1.0f).getSmallestIntegerContainer());
        a.add (juce::Rectangle<float> (kOscX[o] + 34 - 45, kOscY + 86, 90, 14).getSmallestIntegerContainer());
    }
    return a;
}

void ComplicatorPanel::timerCallback()
{
    if (proc.shownPreset() != shownPresetId) updatePresetMenu();
    {
        const auto t = qualityMenu->getItemText (qualityMenu->getSelectedItemIndex());     // "8x heavy" also on the closed menu
        if (t.isNotEmpty() && qualityMenu->getText() != t) qualityMenu->setText (t, juce::dontSendNotification);
    }
    const auto areas = liveAreas();
    const float a = proc.getEngine().playedCoarseA();
    const float b = proc.getEngine().playedCoarseB();
    if (std::fabs (a - shownA) > 0.004f) { shownA = a; repaint (areas[1]); }
    if (std::fabs (b - shownB) > 0.004f) { shownB = b; repaint (areas[3]); }
    for (int w = 0; w < 2; ++w)
    {
        updateScope (w);
        repaint (areas[(size_t) w * 2]);
    }
}

// ------------------------------------------------------------------------------------------------ fixed artwork

std::vector<juce::Rectangle<float>> ComplicatorPanel::fixedTextBoxes()
{
    std::vector<juce::Rectangle<float>> boxes;
    textLog() = &boxes;
    juce::Image img (juce::Image::ARGB, panelW, panelH, true);
    juce::Graphics g (img);
    paintArt (g);
    textLog() = nullptr;
    return boxes;
}

void ComplicatorPanel::paintArt (juce::Graphics& g)
{
    using J = juce::Justification;
    g.setColour (panel());
    g.fillRoundedRectangle (0.0f, 0.0f, (float) panelW, (float) panelH, 12.0f);

    // Header
    textIn (g, "COMPLICATOR", titleFont(), 340.0f, titleBaseline, goldText(), J::centred);
    g.setColour (gold());
    g.fillRect (132.0f, 24.6f, ornamentLeftEnd - 132.0f, 0.8f);
    g.fillRect (ornamentRightStart, 24.6f, 548.0f - ornamentRightStart, 0.8f);
    g.fillEllipse (130.2f, 23.2f, 3.6f, 3.6f);
    g.fillEllipse (546.2f, 23.2f, 3.6f, 3.6f);
    textIn (g, "PARTICLEDOTS", font (11.0f, false, 2.5f), 22.0f, 29.0f, muted(), J::centredLeft);

    // Preset row
    frame (g, 14.5f, kPresetY, 651.0f, kPresetH, juce::String::fromUTF8 ("PRESET  \xc2\xb7  QUALITY"));
    rowTitle (g, "QUALITY", 542.0f, kPresetY + 25.0f, J::centredRight);

    // Oscillators
    for (int o = 0; o < 2; ++o)
    {
        const bool isA = o == 0;
        const float fx = kOscX[o], fy = kOscY;
        frame (g, fx, fy, kOscW, kOscH, juce::String::fromUTF8 (isA ? "OSC A  \xc2\xb7  SAW CORE" : "OSC B  \xc2\xb7  TRIANGLE CORE"));
        knobArt (g, fx + 34, fy + kTuneCy, 22, "COARSE", fy + 84);
        knobArt (g, fx + 82, fy + kTuneCy, 15, "FINE", fy + 84);
        knobArt (g, fx + 118, fy + kTuneCy, 15, "PWM", fy + 84);
        if (! isA) rowTitle (g, "SUB", fx + 138, fy + 33);
        knobArt (g, fx + 34, fy + kFmCy, 15, "FM", fy + kFmCy + 28);
        rowTitle (g, "FM CURVE", fx + 66, fy + kFmCy - 15);

        // Final scope: its frame and centre line (the trace is drawn live), fed by the three folder controls
        rowTitle (g, isA ? "FINAL A" : "FINAL B", fx + 12, fy + kScopeTop - 6);
        const juce::Rectangle<float> sc (fx + kScopeLeft, fy + kScopeTop, kScopeWidth, kScopeH);
        g.setColour (gold().withAlpha (0.6f));
        g.drawRoundedRectangle (sc.reduced (0.5f), 5.0f, 0.6f);
        g.setColour (text().withAlpha (0.12f));
        g.fillRect (sc.getX() + 3.0f, sc.getCentreY() - 0.4f, sc.getWidth() - 6.0f, 0.8f);
        knobArt (g, fx + 142, fy + kFoldCy, 12, "FOLD CV", fy + kFoldCy + 25);
        knobArt (g, fx + 182, fy + kFoldCy, 12, "SYM", fy + kFoldCy + 25);
        textIn (g, "WAVEFOLD", labelFont(), fx + 60, fy + kFoldCy + 25, text(), J::centred);
        const float trackY = fy + kFoldCy;
        for (const float xx : { fx + 60, fx + 142, fx + 182 })
        {
            stroke (g, xx, sc.getBottom(), xx, xx == fx + 60 ? trackY - 10.0f : trackY - 12.0f);
            node (g, xx, sc.getBottom());
        }
    }

    // Centre: pitch and sync
    frame (g, kCentreX, kOscY, kCentreW, 208.0f, "PITCH AND SYNC");
    const char* rows[5] = { "KEYS PLAY", "PITCH SNAP", "SCALE", "KEY", "SYNC" };
    for (int i = 0; i < 5; ++i)
        rowTitle (g, rows[i], kCentreX + 12.0f, kOscY + 26.0f + 37.0f * (float) i);
    note (g, juce::String::fromUTF8 ("Soft: A \xe2\x86\x92 B  \xc2\xb7  Hard: B \xe2\x86\x92 A"), kCentreX + 187.0f, kOscY + 26.0f + 37.0f * 4.0f, J::centredRight);

    // Centre: multiply, fed by Osc A (the modulator) and Osc B (the carrier)
    frame (g, kCentreX, kMultY, kCentreW, kOscH - 216.0f, "MULTIPLY");
    knobArt (g, kCentreX + 40, kMultY + 40, 15, "DEPTH", kMultY + 70);
    rowTitle (g, "MODE", kCentreX + 100, kMultY + 22);
    {
        const float ys = kMultY + 40.0f;
        stroke (g, kOscX[0] + kOscW, ys, kCentreX, ys);
        node (g, kOscX[0] + kOscW, ys);
        node (g, kCentreX, ys);
        stroke (g, kCentreX + kCentreW, ys, kOscX[1], ys);
        node (g, kCentreX + kCentreW, ys);
        node (g, kOscX[1], ys);
        note (g, "A", kOscX[0] + kOscW + 10.0f, ys - 6.0f, J::centred);
        note (g, "B", kOscX[1] - 10.0f, ys - 6.0f, J::centred);
    }

    // Index: its CV amount adds to it, and it feeds all the routes
    frame (g, 14.5f, kIndexY, 651.0f, kIndexH, "INDEX");
    {
        const float cy = kIndexY + 40.0f;
        knobArt (g, 46, cy, 12, "CV AMT", kIndexY + 69);
        knobArt (g, 104, cy, 17, "INDEX", kIndexY + 69);
        stroke (g, 58, cy, 87, cy);
        node (g, 87, cy);
        stroke (g, 121, cy, 156, cy);
        stroke (g, 156, kIndexY + 24, 156, kIndexY + 62);
        node (g, 156, cy);
        const char* cols[3] = { "FM", "FOLD", "SYM" };
        const float colX[3] = { 176, 266, 356 };
        for (int i = 0; i < 3; ++i) rowTitle (g, cols[i], colX[i], kIndexY + 26);
        rowTitle (g, "PWM", 452, kIndexY + 26);
        note (g, "Index-scaled sines feed:", 650, kIndexY + 26, J::centredRight);
    }

    // Patch
    frame (g, 14.5f, kPatchY, 651.0f, kPatchH, "PATCH");
    {
        const char* names[13] = { "FM A", "FM B", "PWM A", "PWM B", "FOLD A", "FOLD B", "SYM A", "SYM B", "INDEX", "CARRIER",
                                  "MODULATOR", "HARD SYNC", "SOFT SYNC" };
        for (int i = 0; i < 13; ++i)
            rowTitle (g, names[i], 26.5f + (float) (i % 7) * 90.5f + 2.0f, kPatchY + 26.0f + (float) (i / 7) * 38.0f);
        const float nx = 26.5f + 6.0f * 90.5f + 2.0f;
        note (g, "Normal = as on", nx, kPatchY + 26.0f + 38.0f + 16.0f, J::centredLeft);
        note (g, "the module", nx, kPatchY + 26.0f + 38.0f + 28.0f, J::centredLeft);
    }

    // Output mix: all eleven levels sum into the amp, then the volume
    frame (g, 14.5f, kMixY, 651.0f, kMixH, "OUTPUT MIX");
    {
        const char* labels[11] = { "SINE", "TRI", "SAW", "PWM", "FINAL", "MULT", "SINE", "TRI", "SQ", "PWM", "FINAL" };
        for (int i = 0; i < 11; ++i)
        {
            knobArt (g, kMixX[i], kMixY + 54, 14, labels[i], kMixY + 82);
            stroke (g, kMixX[i], kMixY + 86.0f, kMixX[i], kMixBus);
        }
        rowTitle (g, "OSC A", 32, kMixY + 24);
        rowTitle (g, "OSC B", 256, kMixY + 24);
        stroke (g, kMixX[0], kMixBus, 454, kMixBus);
        g.setColour (gold());
        for (int i = 0; i < 11; ++i) g.fillEllipse (kMixX[i] - 1.6f, kMixBus - 1.6f, 3.2f, 3.2f);
        node (g, 454, kMixBus);
        rowTitle (g, "ENVELOPE", 458, kMixY + 24);
        const char* env[4] = { "A", "D", "S", "R" };
        for (int i = 0; i < 4; ++i) knobArt (g, kEnvX[i], kMixY + 52, 11, env[i], kMixY + 76);
        rowTitle (g, "AMP", 586, kMixBus - 5);
        stroke (g, 568, kMixBus, kVolumeX, kMixBus);
        rowTitle (g, "VOLUME", kVolumeX, kMixY + 24, J::centred);
        arc (g, kVolumeX, kMixY + 52, 17);
        stroke (g, kVolumeX, kMixBus, kVolumeX, kMixY + 52 + 17);
        node (g, kVolumeX, kMixBus);
    }
}

// ------------------------------------------------------------------------------------------------ live drawing

void ComplicatorPanel::paintOverChildren (juce::Graphics& g)
{
    for (int o = 0; o < 2; ++o)
    {
        const float fx = kOscX[o], fy = kOscY;
        // the scope trace
        const juce::Rectangle<float> sc (fx + kScopeLeft, fy + kScopeTop, kScopeWidth, kScopeH);
        juce::Path path;
        const auto& pts = scopeLine[(size_t) o];
        for (int i = 0; i <= kScopeW; ++i)
        {
            const float v = juce::jlimit (-1.15f, 1.15f, pts[(size_t) i]);
            const juce::Point<float> pt (sc.getX() + 4.0f + (float) i, sc.getCentreY() - v * kScopeH * 0.38f);
            if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
        }
        g.setColour (text().withAlpha (0.85f));
        g.strokePath (path, juce::PathStrokeType (1.3f));
        // the Coarse value (the pitch that is playing)
        textIn (g, semisText (o == 0 ? shownA : shownB), font (11.0f), fx + 34, fy + 97, muted(), juce::Justification::centred);
    }
}

// ================================================================================================ editor

ComplicatorAudioProcessorEditor::ComplicatorAudioProcessorEditor (ComplicatorAudioProcessor& p)
    : juce::AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    panel.setBounds (0, 0, panelW, panelH);
    panel.attachPopups (this);

    setResizable (true, true);
    setResizeLimits ((int) (panelW * 0.9f), (int) (panelH * 0.9f), (int) (panelW * 1.8f), (int) (panelH * 1.8f));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) panelW / (double) panelH);
    setSize (juce::roundToInt (panelW * kDefaultScale), juce::roundToInt (panelH * kDefaultScale));
}

void ComplicatorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (window());
}

void ComplicatorAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) panelW;
    panel.setTransform (juce::AffineTransform::scale (s));
}
