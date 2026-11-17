// The panel's own controls: knobs (look only), segmented switches, menus with a name in front, and the look.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Style.h"

// Draws knobs, menus and the value bubble in the panel's style.
class CxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CxLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return CxStyle::font (11.0f); }
    juce::Font getPopupMenuFont() override { return CxStyle::font (13.0f); }
    juce::Font getSliderPopupFont (juce::Slider&) override { return CxStyle::font (13.0f); }
    juce::Font getLabelFont (juce::Label&) override { return CxStyle::font (11.0f); }
};

// A knob bound to one parameter. The outline colour marks its section.
class CxKnob : public juce::Slider
{
public:
    CxKnob (juce::AudioProcessorValueTreeState&, const char* paramId, juce::Colour accent);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// A row of segments, one per choice of a parameter; or a single on/off segment for a switch.
class CxSegments : public juce::Component
{
public:
    CxSegments (juce::RangedAudioParameter&, juce::StringArray labels, juce::Colour accent, bool isToggle = false);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    juce::Colour accent;
    bool toggle;
    int selected = 0;
    juce::ParameterAttachment attachment;
};

// A drop-down menu bound to a choice parameter, with an optional short name drawn in front ("FM A: Normal").
class CxMenu : public juce::ComboBox
{
public:
    CxMenu (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& prefix);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};
