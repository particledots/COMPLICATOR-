#include "PluginEditor.h"

using namespace CxStyle;

namespace
{
    constexpr float kDefaultScale = 1.3f;

    // Section geometry (panel units), shared by the controls and the painting.
    constexpr int kOscX[2] = { 8, 436 };

    // A folded sine for the (dimmed) scope placeholders.
    float fold (float v) { while (v > 1.0f || v < -1.0f) v = v > 1.0f ? 2.0f - v : -2.0f - v; return v; }

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

    // Header (M3)
    presetMenu.addItem ("Preset: Init", 1);
    presetMenu.setSelectedId (1, juce::dontSendNotification);
    presetMenu.setBounds (424, 11, 150, 22);
    addAndMakeVisible (presetMenu);
    dimIt (presetMenu);
    menu (quality, "Quality", { 582, 11, 86, 22 }, true);

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

        // Milestone 2: wavefolder, its CV amount, symmetry.
        auto& f = isA ? foldA : foldB;
        f.setSliderStyle (juce::Slider::LinearHorizontal);
        f.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        f.setBounds ((int) ox + 12, 296, 132, 20);
        addAndMakeVisible (f);
        (isA ? foldAAtt : foldBAtt) = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, isA ? aFold : bFold, f);
        dimIt (f);
        knob (isA ? aFoldCv : bFoldCv, c, ox + 170, 306, 12, true);
        knob (isA ? aSym : bSym,       c, ox + 210, 306, 12, true);
    }

    // Centre: pitch, sync, multiply, index
    segs (keys, { "A", "A+B", "B" }, shared(), { 262, 74, 156, 20 });
    segs (pitchSnap, { "Off", "Semi", "Steps" }, shared(), { 262, 118, 156, 20 });
    menu (pitchScale, "Scale", { 262, 146, 156, 20 });
    menu (pitchKey, "Key", { 262, 172, 156, 20 });
    segs (CxParams::sync, { "Off", "Soft", "Hard" }, shared(), { 262, 218, 156, 20 });
    knob (multDepth, shared(), 286, 300, 14, true);
    segs (multMode, { "Ring", "Half", "Full" }, shared(), { 312, 291, 106, 20 }, false, true);
    knob (CxParams::index, shared(), 358, 336, 15, true);
    knob (indexCv, shared(), 398, 336, 10, true);

    // Index routing row (M2)
    const char* routeIds[6] = { routeFmA, routeFmB, routeFoldA, routeFoldB, routeSymA, routeSymB };
    const char* routeLabels[6] = { "FM \xe2\x86\x92 A", "FM \xe2\x86\x92 B", "Fold A", "Fold B", "Sym A", "Sym B" };
    for (int i = 0; i < 6; ++i)
        segs (routeIds[i], { juce::String::fromUTF8 (routeLabels[i]) }, shared(), { 18 + i * 78, 402, 72, 22 }, true, true);
    segs (routePwm, { "Off", "Both", juce::String::fromUTF8 ("A \xe2\x86\x92 B") }, shared(), { 498, 402, 164, 22 }, false, true);

    // Patch row (M2): 13 menus in 7 columns
    for (int i = 0; i < numPatch; ++i)
        menu (patchIds[i], patchShort[i], { 14 + (i % 7) * 93, 470 + (i / 7) * 26, 88, 21 }, true);

    // Output mixer
    const float mx[numMix] = { 30, 66, 102, 138, 174, 218, 262, 298, 334, 370, 406 };
    for (int i = 0; i < numMix; ++i)
    {
        const auto c = i < 5 ? oscA() : (i == 5 ? shared() : oscB());
        const bool later = i == 4 || i == 5 || i == 10;      // Final A, Multiply, Final B: milestone 2
        knob (mixIds[i], c, mx[i], 578, 11, later);
    }
    knob (envAttack,  muted(), 452, 578, 10);
    knob (envDecay,   muted(), 484, 578, 10);
    knob (envSustain, muted(), 516, 578, 10);
    knob (envRelease, muted(), 548, 578, 10);
    segs (ampMode, { "Drone", "Env" }, shared(), { 574, 558, 90, 20 });
    knob (volume, text(), 618, 602, 13);

    startTimerHz (15);
}

ComplicatorPanel::~ComplicatorPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
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
}

void ComplicatorPanel::timerCallback()
{
    const float a = proc.getEngine().playedCoarseA();
    const float b = proc.getEngine().playedCoarseB();
    if (std::fabs (a - shownA) > 0.004f) { shownA = a; repaint (kOscX[0] + 4, 150, 80, 18); }
    if (std::fabs (b - shownB) > 0.004f) { shownB = b; repaint (kOscX[1] + 4, 150, 80, 18); }
}

void ComplicatorPanel::paint (juce::Graphics& g)
{
    g.fillAll (window());
    g.setColour (panel());
    g.fillRoundedRectangle (0.0f, 0.0f, (float) panelW, (float) panelH, 10.0f);

    // Header
    textAt (g, "COMPLICATOR", 16, 27, text(), 18.0f, juce::Justification::centredLeft);
    textAt (g, "particledots", 156, 27, muted(), 11.0f, juce::Justification::centredLeft);
    tag (g, 392, 27, "M3");
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

        // Milestone 2 area: scope, wavefold, fold CV, symmetry
        {
            g.beginTransparencyLayer (dimAlpha);
            const juce::Rectangle<float> sc (ox + 12, 240, 212, 44);
            g.setColour (scopeBack());
            g.fillRoundedRectangle (sc, 4.0f);
            g.setColour (line());
            g.drawRoundedRectangle (sc, 4.0f, 1.0f);
            juce::Path path;
            const float gain = isA ? 2.2f : 1.2f;
            for (int i = 0; i <= 212; i += 2)
            {
                const float v = fold (gain * std::sin ((float) i / 212.0f * 4.0f * juce::MathConstants<float>::pi) + 0.3f);
                const juce::Point<float> pt (sc.getX() + (float) i, sc.getCentreY() - v * 44.0f * 0.4f);
                if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
            }
            g.setColour (c);
            g.strokePath (path, juce::PathStrokeType (1.5f));
            textAt (g, "WAVEFOLD", ox + 78, 330, muted());
            textAt (g, "FOLD CV", ox + 170, 331, muted());
            textAt (g, "SYM", ox + 210, 331, muted());
            textAt (g, isA ? "Final A scope" : "Final B scope", ox + 46, 358, muted());
            g.endTransparencyLayer();
        }
        tag (g, ox + 198, 355, "M2");
    }

    // Centre
    box (g, 250, 50, 180, 318, shared());
    textAt (g, "Keys play", 340, 68, muted());
    textAt (g, "Pitch Snap", 340, 112, muted());
    textAt (g, "Sync", 340, 212, muted());
    textAt (g, juce::String::fromUTF8 ("Soft: A \xe2\x86\x92 B  \xc2\xb7  Hard: B \xe2\x86\x92 A"), 340, 252, muted());
    {
        g.beginTransparencyLayer (dimAlpha);
        textAt (g, "MULTIPLY", 262, 276, shared(), 11.0f, juce::Justification::centredLeft);
        textAt (g, "DEPTH", 286, 327, muted());
        textAt (g, "INDEX", 262, 352, shared(), 11.0f, juce::Justification::centredLeft);
        textAt (g, "INDEX", 358, 362, muted());
        textAt (g, "CV amt", 398, 362, muted());
        g.endTransparencyLayer();
    }
    tag (g, 398, 276, "M2");

    // Index routing
    box (g, 8, 376, 664, 62, shared());
    {
        g.beginTransparencyLayer (dimAlpha);
        textAt (g, juce::String::fromUTF8 ("INDEX ROUTING  \xc2\xb7  Index-scaled sines feed:"), 18, 393, shared(), 11.0f, juce::Justification::centredLeft);
        textAt (g, "PWM (Mod button)", 560, 393, muted());
        g.endTransparencyLayer();
    }
    tag (g, 640, 393, "M2");

    // Patch
    box (g, 8, 446, 664, 78, shared());
    {
        g.beginTransparencyLayer (dimAlpha);
        textAt (g, juce::String::fromUTF8 ("PATCH  \xc2\xb7  what feeds each input (\"Normal\" = as on the module)"), 18, 463, shared(), 11.0f,
                juce::Justification::centredLeft);
        g.endTransparencyLayer();
    }
    tag (g, 640, 463, "M2");

    // Output mix
    box (g, 8, 530, 664, 126, muted());
    textAt (g, "OUTPUT MIX", 18, 547, text(), 11.0f, juce::Justification::centredLeft);
    const char* mixLabels[11] = { "Sine", "Tri", "Saw", "PWM", "Final", "Mult", "Sine", "Tri", "Sq", "PWM", "Final" };
    const float mx[11] = { 30, 66, 102, 138, 174, 218, 262, 298, 334, 370, 406 };
    for (int i = 0; i < 11; ++i)
    {
        const bool later = i == 4 || i == 5 || i == 10;
        textAt (g, mixLabels[i], mx[i], 602, later ? muted().withAlpha (dimAlpha) : muted());
    }
    textAt (g, "Osc A", 102, 626, oscA());
    textAt (g, "Osc B", 334, 626, oscB());
    tag (g, 196, 641, "M2");
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
