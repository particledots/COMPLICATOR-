#include "Controls.h"
#include <CxFontData.h>

using namespace CxStyle;

// ------------------------------------------------------------------------------------------------ fonts

namespace
{
    juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }
    struct Faces
    {
        juce::Typeface::Ptr regular = load (CxFontData::SpaceGroteskRegular_ttf, CxFontData::SpaceGroteskRegular_ttfSize);
        juce::Typeface::Ptr medium  = load (CxFontData::SpaceGroteskMedium_ttf, CxFontData::SpaceGroteskMedium_ttfSize);
        juce::Typeface::Ptr title   = load (CxFontData::MarcellusRegular_ttf, CxFontData::MarcellusRegular_ttfSize);
    };
    Faces& faces() { static Faces f; return f; }

    juce::Font make (juce::Typeface::Ptr face, float size, float spacing)
    {
        // If a font could not be read for any reason, fall back to the system font rather than draw nothing.
        juce::FontOptions o = face != nullptr ? juce::FontOptions (face) : juce::FontOptions();
        juce::Font f (o.withPointHeight (size));
        if (spacing > 0.0f)
            f = f.withExtraKerningFactor (spacing / f.getHeight());
        return f;
    }

    constexpr float kPillPad = 24.4f;      // what a segment needs beside its label (the lamp left of it, and room)
}

juce::Font CxStyle::font (float size, bool medium, float spacing)
{
    return make (medium ? faces().medium : faces().regular, size, spacing);
}

juce::Font CxStyle::titleFont()
{
    return make (faces().title, titleSize, titleSpacing);
}

// ------------------------------------------------------------------------------------------------ look

CxLookAndFeel::CxLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, menu());
    setColour (juce::PopupMenu::textColourId, text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, lampFill());
    setColour (juce::PopupMenu::highlightedTextColourId, text());
    setColour (juce::ComboBox::textColourId, text());
    setColour (juce::ComboBox::backgroundColourId, panel());
    setColour (juce::ComboBox::outlineColourId, gold());
    setColour (juce::ComboBox::arrowColourId, gold());
    setColour (juce::BubbleComponent::backgroundColourId, menu());
    setColour (juce::BubbleComponent::outlineColourId, gold());
    setColour (juce::TooltipWindow::textColourId, text());
    setColour (juce::Label::textColourId, text());
}

void CxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                      juce::Slider&)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float u = juce::jmin (r.getWidth(), r.getHeight()) / 62.0f;     // one panel unit of the 62-unit knob
    const auto c = r.getCentre();
    auto disc = [&] (float radius, juce::Colour fill, juce::Colour line, float lw)
    {
        g.setColour (fill);
        g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (line);
        g.drawEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f, lw);
    };
    disc (22.0f * u, skirt(), skirtLine(), 1.2f * u);
    disc (16.0f * u, cap(), capLine(), 0.8f * u);

    const float a = start + pos * (end - start);
    const float sx = std::sin (a), sy = -std::cos (a);
    juce::Path p;
    p.startNewSubPath (c.x + 5.0f * u * sx, c.y + 5.0f * u * sy);
    p.lineTo (c.x + 15.0f * u * sx, c.y + 15.0f * u * sy);
    g.setColour (pointer());
    g.strokePath (p, juce::PathStrokeType (juce::jmax (2.0f * u, 1.2f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// The Wavefold fader: a gold track with a dot at each end, a mark at the centre (a plain sine), a cream cap.
void CxLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                      juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) h * 0.5f, x0 = (float) x, x1 = (float) (x + w);
    g.setColour (gold());
    g.fillRect (x0, cy - 0.45f, x1 - x0, 0.9f);
    g.fillEllipse (x0 - 1.8f, cy - 1.8f, 3.6f, 3.6f);
    g.fillEllipse (x1 - 1.8f, cy - 1.8f, 3.6f, 3.6f);
    g.fillRect ((x0 + x1) * 0.5f - 0.35f, cy - 6.0f, 0.7f, 12.0f);
    const juce::Rectangle<float> thumb (pos - 6.0f, cy - 8.0f, 12.0f, 16.0f);
    g.setColour (cap());
    g.fillRoundedRectangle (thumb, 3.0f);
    g.setColour (capLine());
    g.drawRoundedRectangle (thumb, 3.0f, 0.8f);
    g.setColour (pointer());
    g.drawLine (pos, cy - 5.0f, pos, cy + 5.0f, 1.6f);
}

void CxLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (gold());
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 0.8f);
    juce::Path chevron;
    const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.5f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 4.0f, cy - 2.5f);
    g.strokePath (chevron, juce::PathStrokeType (0.9f));
}

void CxLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBorderSize (juce::BorderSize<int> (0));
    label.setBounds (0, 0, box.getWidth() - 14, box.getHeight());
    label.setJustificationType (juce::Justification::centred);
    label.setFont (menuFont());
}

// ------------------------------------------------------------------------------------------------ knob

CxKnob::CxKnob (juce::AudioProcessorValueTreeState& state, const char* paramId)
    : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox)
{
    setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    setPopupDisplayEnabled (true, true, nullptr);
    setVelocityModeParameters (0.6, 1, 0.0, true, juce::ModifierKeys::shiftModifier);   // hold Shift for fine moves
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, *this);
    if (auto* p = state.getParameter (paramId))
        setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));   // double-click: back to default
}

// ------------------------------------------------------------------------------------------------ segments

CxSegments::CxSegments (juce::RangedAudioParameter& p, juce::StringArray l, Kind k)
    : param (p), labels (std::move (l)), kind (k),
      attachment (p, [this] (float v) { selected = juce::roundToInt (v); repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
}

float CxSegments::pillMinWidth (const juce::StringArray& l)
{
    float t = 0.0f;
    for (const auto& s : l) t += widthOf (s, pillFont()) + kPillPad;
    return t;
}

float CxSegments::optionWidth (const juce::String& label)
{
    return std::ceil (17.0f + widthOf (label, font (11.0f, false, 1.3f)) + 4.0f);     // lamp, gap, label
}

std::vector<float> CxSegments::pillEdges() const
{
    // Segments share the width in proportion to what their labels need.
    const int n = labels.size();
    std::vector<float> need ((size_t) n), edges ((size_t) n + 1);
    float total = 0.0f;
    for (int i = 0; i < n; ++i) total += (need[(size_t) i] = widthOf (labels[i], pillFont()) + kPillPad);
    float x = 0.0f;
    edges[0] = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        x += need[(size_t) i] * (float) getWidth() / total;
        edges[(size_t) i + 1] = x;
    }
    return edges;
}

bool CxSegments::labelsFit() const
{
    const int n = labels.size();
    if (kind == Kind::Pill)
        return pillMinWidth (labels) <= (float) getWidth() + 0.01f;
    if (kind == Kind::Option)
        return optionWidth (labels[0]) <= (float) getWidth() + 0.01f;
    for (int i = 0; i < n; ++i)
        if (18.0f + widthOf (labels[i], font (11.0f, false, 1.2f)) > (float) getWidth()) return false;
    return (float) n * 15.0f <= (float) getHeight() + 0.01f;
}

void CxSegments::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const int n = labels.size();

    switch (kind)
    {
        case Kind::Option:
        {
            const bool on = selected == 1;
            drawLamp (g, b.getX() + 8.0f, b.getCentreY(), on, on ? 3.2f : 3.0f);
            textIn (g, labels[0], font (11.0f, false, 1.3f), b.getX() + 17.0f, b.getCentreY() + 4.0f,
                    on ? text() : soft(), juce::Justification::centredLeft);
            return;
        }

        case Kind::List:
        {
            const float rowH = 15.0f;
            for (int i = 0; i < n; ++i)
            {
                const float cy = b.getY() + rowH * ((float) i + 0.5f);
                const bool on = selected == i;
                drawLamp (g, b.getX() + 9.0f, cy, on, on ? 3.2f : 3.0f);
                textIn (g, labels[i], font (11.0f, false, 1.2f), b.getX() + 18.0f, cy + 4.0f, on ? text() : muted(),
                        juce::Justification::centredLeft);
            }
            return;
        }

        case Kind::Pill:
        {
            const auto r = b.reduced (0.5f);
            g.setColour (gold());
            g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 0.7f);
            const auto e = pillEdges();
            for (int i = 1; i < n; ++i)
                g.fillRect (e[(size_t) i] - 0.35f, r.getY() + 0.5f, 0.7f, r.getHeight() - 1.0f);
            for (int i = 0; i < n; ++i)
            {
                const bool on = selected == i;
                const float cx = (e[(size_t) i] + e[(size_t) i + 1]) * 0.5f, cy = r.getCentreY();
                textIn (g, labels[i], pillFont(), cx, cy + 4.0f, on ? text() : muted(), juce::Justification::centred);
                if (on)
                    drawLamp (g, cx - widthOf (labels[i], pillFont()) * 0.5f - 7.0f, cy, true, 2.2f);
            }
            return;
        }
    }
}

void CxSegments::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    const int n = labels.size();
    if (kind == Kind::Option)
    {
        attachment.setValueAsCompleteGesture (selected == 1 ? 0.0f : 1.0f);
        return;
    }
    int i = 0;
    if (kind == Kind::List)
        i = juce::jlimit (0, n - 1, (int) ((float) e.y / 15.0f));
    else
    {
        const auto edges = pillEdges();
        while (i < n - 1 && (float) e.x >= edges[(size_t) i + 1]) ++i;
    }
    attachment.setValueAsCompleteGesture ((float) i);
}

// ------------------------------------------------------------------------------------------------ menu

CxMenu::CxMenu (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& name)
{
    setName (name);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        addItemList (p->choices, 1);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramId, *this);
}
