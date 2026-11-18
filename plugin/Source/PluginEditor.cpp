#include "PluginEditor.h"

using namespace CxStyle;

namespace
{
    constexpr float kDefaultScale = 1.3f;

    // Section geometry (panel units), shared by the controls and the painting.
    constexpr int kOscX[2] = { 8, 436 };

    void tag (juce::Graphics& g, float x, float y, const char* m)
    {
        g.setColour (tagLine());
        g.drawRoundedRectangle (x, y - 11.0f, 26.0f, 15.0f, 3.0f, 1.0f);
        textAt (g, m, x + 13.0f, y, tagText());
    }

    void box (juce::Graphics& g, float x, float y, float w, float h, juce::Colour c)
    {
        g.setColour (c.withAlpha (0.55f));
        g.drawRoundedRectangle (x, y, w, h, 8.0f, 1.0f);
    }

    juce::String semisText (float v)
    {
        const bool whole = std::fabs (v - std::round (v)) < 0.005f;
        const juce::String num = whole ? juce::String (juce::roundToInt (v)) : juce::String (v, 2);
        return (v > 0.0049f ? "+" : "") + num + " st";
    }
}

// ================================================================================================ panel

ComplicatorPanel::ComplicatorPanel (ComplicatorAudioProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.getValueTreeState();
    using namespace CxParams;

    // Header: the Preset menu (Init + the factory presets) and the Quality menu
    presetMenu.setName ("Preset");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetMenu.addItem (proc.getProgramName (i), i + 1);
    presetMenu.setBounds (424, 11, 150, 22);
    addAndMakeVisible (presetMenu);
    presetMenu.onChange = [this]
    {
        const int id = presetMenu.getSelectedId();
        if (id > 0)
            proc.setCurrentProgram (id - 1);
    };
    updatePresetMenu();
    qualityMenu = &menu (quality, "Quality", { 582, 11, 86, 22 });
    qualityMenu->changeItemText (3, "8x heavy");                      // the list's third entry (8x) is the CPU-hungry one
    qualityParam = st.getRawParameterValue (quality);
    updateQualityMenu();

    // The two oscillators
    for (int o = 0; o < 2; ++o)
    {
        const bool isA = o == 0;
        const float ox = (float) kOscX[o];
        const auto c = isA ? oscA() : oscB();
        knob (isA ? aCoarse : bCoarse, c, ox + 44, 112, 24);
        knob (isA ? aFine : bFine,     c, ox + 110, 112, 16);
        knob (isA ? aPwm : bPwm,       c, ox + 164, 112, 16);
        if (isA) segs (aLfo, { "LFO" }, c, { (int) ox + 192, 94, 36, 20 }, true);
        else     segs (bSub, { "0", "-1", "-2" }, c, { (int) ox + 180, 96, 52, 18 });
        knob (isA ? aFm : bFm, c, ox + 44, 198, 16);
        segs (isA ? aFmCurve : bFmCurve, { "LIN", "EXP", "TZ" }, c, { (int) ox + 76, 189, 96, 20 });

        // Wavefolder fader, its CV amount, symmetry.
        auto& f = isA ? foldA : foldB;
        f.setSliderStyle (juce::Slider::LinearHorizontal);
        f.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        f.setBounds ((int) ox + 12, 296, 132, 20);
        addAndMakeVisible (f);
        (isA ? foldAAtt : foldBAtt) = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, isA ? aFold : bFold, f);
        f.setDoubleClickReturnValue (true, 0.5);                                              // back to the plain sine
        f.setVelocityModeParameters (0.6, 1, 0.0, true, juce::ModifierKeys::shiftModifier);   // hold Shift for fine moves
        knob (isA ? aFoldCv : bFoldCv, c, ox + 170, 306, 12);
        knob (isA ? aSym : bSym,       c, ox + 210, 306, 12);
    }

    // Centre: pitch, sync, multiply, index
    segs (keys, { "A", "A+B", "B" }, shared(), { 262, 74, 156, 20 });
    segs (pitchSnap, { "Off", "Semi", "Steps" }, shared(), { 262, 118, 156, 20 });
    menu (pitchScale, "Scale", { 262, 146, 156, 20 });
    menu (pitchKey, "Key", { 262, 172, 156, 20 });
    segs (CxParams::sync, { "Off", "Soft", "Hard" }, shared(), { 262, 218, 156, 20 });
    knob (multDepth, shared(), 286, 300, 14);
    segs (multMode, { "Ring", "Half", "Full" }, shared(), { 312, 291, 106, 20 });
    knob (CxParams::index, shared(), 358, 336, 15);
    knob (indexCv, shared(), 398, 336, 10);

    // Index routing row
    const char* routeIds[6] = { routeFmA, routeFmB, routeFoldA, routeFoldB, routeSymA, routeSymB };
    const char* routeLabels[6] = { "FM \xe2\x86\x92 A", "FM \xe2\x86\x92 B", "Fold A", "Fold B", "Sym A", "Sym B" };
    for (int i = 0; i < 6; ++i)
        segs (routeIds[i], { juce::String::fromUTF8 (routeLabels[i]) }, shared(), { 18 + i * 78, 402, 72, 22 }, true);
    segs (routePwm, { "Off", "Both", juce::String::fromUTF8 ("A \xe2\x86\x92 B") }, shared(), { 498, 402, 164, 22 });

    // Patch row: 13 menus in 7 columns
    for (int i = 0; i < numPatch; ++i)
        menu (patchIds[i], patchShort[i], { 14 + (i % 7) * 93, 470 + (i / 7) * 26, 88, 21 });

    // Output mixer
    const float mx[numMix] = { 30, 66, 102, 138, 174, 218, 262, 298, 334, 370, 406 };
    for (int i = 0; i < numMix; ++i)
    {
        const auto c = i < 5 ? oscA() : (i == 5 ? shared() : oscB());
        knob (mixIds[i], c, mx[i], 578, 11);
    }
    knob (envAttack,  muted(), 452, 578, 10);
    knob (envDecay,   muted(), 484, 578, 10);
    knob (envSustain, muted(), 516, 578, 10);
    knob (envRelease, muted(), 548, 578, 10);
    segs (ampMode, { "Drone", "Env" }, shared(), { 574, 558, 90, 20 });
    knob (volume, text(), 618, 602, 13);

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

// 8x runs the whole engine at 8 times the project's sample rate: it is marked in the list and its label turns amber.
void ComplicatorPanel::updateQualityMenu()
{
    const int q = qualityParam != nullptr ? juce::roundToInt (qualityParam->load()) : 1;
    qualityMenu->setColour (juce::ComboBox::textColourId, q == 2 ? tagText() : text());
    qualityMenu->repaint();
    shownQuality = q;
}

void ComplicatorPanel::dimIt (juce::Component& c)
{
    c.setEnabled (false);
    c.setAlpha (dimAlpha);
}

CxKnob& ComplicatorPanel::knob (const char* id, juce::Colour c, float cx, float cy, float r, bool dim)
{
    auto k = std::make_unique<CxKnob> (proc.getValueTreeState(), id, c);
    k->setBounds (juce::roundToInt (cx - r - 2.0f), juce::roundToInt (cy - r - 2.0f),
                  juce::roundToInt (2.0f * r + 4.0f), juce::roundToInt (2.0f * r + 4.0f));
    addAndMakeVisible (*k);
    if (dim) dimIt (*k);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

CxSegments& ComplicatorPanel::segs (const char* id, juce::StringArray labels, juce::Colour c, juce::Rectangle<int> r,
                                    bool toggle, bool dim)
{
    auto* p = proc.getValueTreeState().getParameter (id);
    jassert (p != nullptr);
    auto s = std::make_unique<CxSegments> (*p, std::move (labels), c, toggle);
    s->setBounds (r);
    addAndMakeVisible (*s);
    if (dim) dimIt (*s);
    segments.push_back (std::move (s));
    return *segments.back();
}

CxMenu& ComplicatorPanel::menu (const char* id, const juce::String& prefix, juce::Rectangle<int> r, bool dim)
{
    auto m = std::make_unique<CxMenu> (proc.getValueTreeState(), id, prefix);
    m->setBounds (r);
    addAndMakeVisible (*m);
    if (dim) dimIt (*m);
    menus.push_back (std::move (m));
    return *menus.back();
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

void ComplicatorPanel::timerCallback()
{
    if (proc.shownPreset() != shownPresetId) updatePresetMenu();
    if (qualityParam != nullptr && juce::roundToInt (qualityParam->load()) != shownQuality) updateQualityMenu();
    const float a = proc.getEngine().playedCoarseA();
    const float b = proc.getEngine().playedCoarseB();
    if (std::fabs (a - shownA) > 0.004f) { shownA = a; repaint (kOscX[0] + 4, 150, 80, 18); }
    if (std::fabs (b - shownB) > 0.004f) { shownB = b; repaint (kOscX[1] + 4, 150, 80, 18); }
    for (int w = 0; w < 2; ++w)
    {
        updateScope (w);
        repaint (kOscX[w] + 12, 240, kScopeW, 44);
    }
}

void ComplicatorPanel::paint (juce::Graphics& g)
{
    g.fillAll (window());
    g.setColour (panel());
    g.fillRoundedRectangle (0.0f, 0.0f, (float) panelW, (float) panelH, 10.0f);

    // Header
    textAt (g, "COMPLICATOR", 16, 27, text(), 18.0f, juce::Justification::centredLeft);
    textAt (g, "particledots", 156, 27, muted(), 11.0f, juce::Justification::centredLeft);
    g.setColour (line());
    g.drawLine (8.0f, 42.0f, 672.0f, 42.0f, 1.0f);

    // Oscillators
    for (int o = 0; o < 2; ++o)
    {
        const bool isA = o == 0;
        const float ox = (float) kOscX[o];
        const auto c = isA ? oscA() : oscB();
        box (g, ox, 50, 236, 318, c);
        textAt (g, juce::String::fromUTF8 (isA ? "OSC A  \xc2\xb7  saw core" : "OSC B  \xc2\xb7  triangle core"), ox + 12, 70, c, 12.0f,
                juce::Justification::centredLeft);
        textAt (g, "COARSE", ox + 44, 149, muted());
        textAt (g, semisText (isA ? shownA : shownB), ox + 44, 163, muted());
        textAt (g, "FINE", ox + 110, 141, muted());
        textAt (g, "PWM", ox + 164, 141, muted());
        if (! isA) textAt (g, "SUB", ox + 206, 90, muted());
        textAt (g, "FM", ox + 44, 227, muted());
        textAt (g, "FM curve", ox + 124, 226, muted());

        // Final scope, wavefold, fold CV, symmetry
        {
            const juce::Rectangle<float> sc (ox + 12, 240, (float) kScopeW, 44);
            g.setColour (scopeBack());
            g.fillRoundedRectangle (sc, 4.0f);
            g.setColour (line());
            g.drawRoundedRectangle (sc, 4.0f, 1.0f);
            g.setColour (line().withAlpha (0.5f));
            g.drawLine (sc.getX() + 2.0f, sc.getCentreY(), sc.getRight() - 2.0f, sc.getCentreY(), 1.0f);
            juce::Path path;
            const auto& pts = scopeLine[(size_t) o];
            for (int i = 0; i <= kScopeW; ++i)
            {
                const float v = juce::jlimit (-1.15f, 1.15f, pts[(size_t) i]);
                const juce::Point<float> pt (sc.getX() + (float) i, sc.getCentreY() - v * 44.0f * 0.4f);
                if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
            }
            g.setColour (c);
            g.strokePath (path, juce::PathStrokeType (1.5f));
            textAt (g, "WAVEFOLD", ox + 78, 330, muted());
            textAt (g, "FOLD CV", ox + 170, 331, muted());
            textAt (g, "SYM", ox + 210, 331, muted());
            textAt (g, isA ? "Final A scope" : "Final B scope", ox + 46, 358, muted());
        }
    }

    // Centre
    box (g, 250, 50, 180, 318, shared());
    textAt (g, "Keys play", 340, 68, muted());
    textAt (g, "Pitch Snap", 340, 112, muted());
    textAt (g, "Sync", 340, 212, muted());
    textAt (g, juce::String::fromUTF8 ("Soft: A \xe2\x86\x92 B  \xc2\xb7  Hard: B \xe2\x86\x92 A"), 340, 252, muted());
    textAt (g, "MULTIPLY", 262, 276, shared(), 11.0f, juce::Justification::centredLeft);
    textAt (g, "DEPTH", 286, 327, muted());
    textAt (g, "INDEX", 262, 352, shared(), 11.0f, juce::Justification::centredLeft);
    textAt (g, "INDEX", 358, 362, muted());
    textAt (g, "CV amt", 398, 362, muted());

    // Index routing
    box (g, 8, 376, 664, 62, shared());
    textAt (g, juce::String::fromUTF8 ("INDEX ROUTING  \xc2\xb7  Index-scaled sines feed:"), 18, 393, shared(), 11.0f, juce::Justification::centredLeft);
    textAt (g, "PWM (Mod button)", 560, 393, muted());

    // Patch
    box (g, 8, 446, 664, 78, shared());
    textAt (g, juce::String::fromUTF8 ("PATCH  \xc2\xb7  what feeds each input (\"Normal\" = as on the module)"), 18, 463, shared(), 11.0f,
            juce::Justification::centredLeft);

    // Output mix
    box (g, 8, 530, 664, 126, muted());
    textAt (g, "OUTPUT MIX", 18, 547, text(), 11.0f, juce::Justification::centredLeft);
    const char* mixLabels[11] = { "Sine", "Tri", "Saw", "PWM", "Final", "Mult", "Sine", "Tri", "Sq", "PWM", "Final" };
    const float mx[11] = { 30, 66, 102, 138, 174, 218, 262, 298, 334, 370, 406 };
    for (int i = 0; i < 11; ++i)
        textAt (g, mixLabels[i], mx[i], 602, muted());
    textAt (g, "Osc A", 102, 626, oscA());
    textAt (g, "Osc B", 334, 626, oscB());
    const char* envLabels[4] = { "A", "D", "S", "R" };
    for (int i = 0; i < 4; ++i)
        textAt (g, envLabels[i], 452.0f + 32.0f * (float) i, 601, muted());
    textAt (g, "Envelope", 500, 626, muted());
    textAt (g, "Amp", 618, 552, muted());
    textAt (g, "Volume", 618, 637, muted());
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
