#include "Controls.h"

using namespace CxStyle;

// ------------------------------------------------------------------------------------------------ look

CxLookAndFeel::CxLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, control());
    setColour (juce::PopupMenu::textColourId, text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, shared().withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, text());
    setColour (juce::ComboBox::textColourId, text());
    setColour (juce::ComboBox::backgroundColourId, control());
    setColour (juce::ComboBox::outlineColourId, line());
    setColour (juce::ComboBox::arrowColourId, muted());
    setColour (juce::BubbleComponent::backgroundColourId, control());
    setColour (juce::BubbleComponent::outlineColourId, line());
    setColour (juce::TooltipWindow::textColourId, text());
    setColour (juce::Label::textColourId, text());
}

void CxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                      juce::Slider& s)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.5f);
    const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const auto c = r.getCentre();
    const auto accent = s.findColour (juce::Slider::rotarySliderOutlineColourId);

    g.setColour (knob());
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (accent);
    g.drawEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    const float a = start + pos * (end - start);
    const juce::Point<float> tip (c.x + 0.8f * radius * std::sin (a), c.y - 0.8f * radius * std::cos (a));
    g.setColour (text());
    g.drawLine ({ c, tip }, 2.0f);
}

void CxLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                      juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (line());
    g.fillRoundedRectangle ((float) x, cy - 3.0f, (float) w, 6.0f, 3.0f);
    g.setColour (text());
    g.fillRoundedRectangle (pos - 6.0f, cy - 8.0f, 12.0f, 16.0f, 2.0f);
}

void CxLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (control());
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (line());
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    const auto prefix = box.getName();
    if (prefix.isNotEmpty())
    {
        g.setColour (muted());
        g.setFont (font (10.0f));
        g.drawText (prefix, juce::Rectangle<float> (5.0f, 0.0f, (float) w, (float) h), juce::Justification::centredLeft, false);
    }
    g.setColour (muted());
    g.setFont (font (10.0f));
    g.drawText (juce::String::fromUTF8 ("\xe2\x96\xbe"), juce::Rectangle<float> ((float) w - 14.0f, 0.0f, 12.0f, (float) h),
                juce::Justification::centred, false);
}

void CxLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    const auto prefix = box.getName();
    int left = 3;
    if (prefix.isNotEmpty())
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (font (10.0f), prefix, 0.0f, 0.0f);
        left = 6 + (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth());
    }
    label.setBorderSize (juce::BorderSize<int> (0, 1, 0, 0));
    label.setBounds (left, 0, box.getWidth() - left - 11, box.getHeight());
    label.setFont (font (11.0f));
    label.setMinimumHorizontalScale (0.7f);
}

// ------------------------------------------------------------------------------------------------ knob

CxKnob::CxKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, juce::Colour accent)
    : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox)
{
    setColour (juce::Slider::rotarySliderOutlineColourId, accent);
    setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    setPopupDisplayEnabled (true, true, nullptr);
    setVelocityModeParameters (0.6, 1, 0.0, true, juce::ModifierKeys::shiftModifier);   // hold Shift for fine moves
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, *this);
    if (auto* p = state.getParameter (paramId))
        setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));   // double-click: back to default
}

// ------------------------------------------------------------------------------------------------ segments

CxSegments::CxSegments (juce::RangedAudioParameter& p, juce::StringArray l, juce::Colour c, bool isToggle)
    : param (p), labels (std::move (l)), accent (c), toggle (isToggle),
      attachment (p, [this] (float v) { selected = juce::roundToInt (v); repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
}

void CxSegments::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (control());
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (line());
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    const int n = labels.size();
    const float cw = r.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const bool on = toggle ? (selected == 1) : (selected == i);
        const auto seg = juce::Rectangle<float> (r.getX() + cw * (float) i, r.getY(), cw, r.getHeight());
        if (on)
        {
            const auto lit = seg.reduced (1.5f);
            g.setColour (accent.withAlpha (0.25f));
            g.fillRoundedRectangle (lit, 3.0f);
            g.setColour (accent);
            g.drawRoundedRectangle (lit, 3.0f, 1.0f);
        }
        g.setColour (on ? text() : muted());
        g.setFont (font (11.0f));
        g.drawFittedText (labels[i], seg.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

void CxSegments::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    if (toggle)
    {
        attachment.setValueAsCompleteGesture (selected == 1 ? 0.0f : 1.0f);
        return;
    }
    const int n = labels.size();
    const int i = juce::jlimit (0, n - 1, (int) ((float) e.x / (float) getWidth() * (float) n));
    attachment.setValueAsCompleteGesture ((float) i);
}

// ------------------------------------------------------------------------------------------------ menu

CxMenu::CxMenu (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& prefix)
{
    setName (prefix);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        addItemList (p->choices, 1);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramId, *this);
}
