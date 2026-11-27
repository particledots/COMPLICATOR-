// The panel's own controls: knobs, the Wavefold fader, switches (pill rows, lamp lists, option lamps), menus, and the look.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Style.h"

class CxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CxLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return CxStyle::menuFont(); }
    juce::Font getPopupMenuFont() override { return CxStyle::font (13.0f); }
    juce::Font getSliderPopupFont (juce::Slider&) override { return CxStyle::font (13.0f); }
    juce::Font getLabelFont (juce::Label&) override { return CxStyle::menuFont(); }

    // The room a closed menu has for its text (panel units): its width less the arrow.
    static float menuTextRoom (const juce::ComboBox& box) { return (float) box.getWidth() - 16.0f; }
};

// A knob bound to one parameter: a black skirt with a cream cap and a dark pointer line. Its gold range arc is part of
// the panel's fixed artwork. The bounds are the arc's square, so the whole ring can be grabbed.
class CxKnob : public juce::Slider
{
public:
    CxKnob (juce::AudioProcessorValueTreeState&, const char* paramId);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// A switch bound to one parameter, drawn in one of three ways:
//   Pill:   a row of choices in a gold pill; the chosen one is lit by a blue lamp. Segments are as wide as their labels need.
//   List:   choices stacked with a lamp each.
//   Option: one on/off lamp with its label.
class CxSegments : public juce::Component
{
public:
    enum class Kind { Pill, List, Option };
    CxSegments (juce::RangedAudioParameter&, juce::StringArray labels, Kind kind);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    const juce::StringArray& getLabels() const { return labels; }
    // True when every label fits its segment / row (for the check program).
    bool labelsFit() const;
    // The width a pill needs at least for its labels; the width an Option needs.
    static float pillMinWidth (const juce::StringArray&);
    static float optionWidth (const juce::String& label);

private:
    std::vector<float> pillEdges() const;      // x of every segment border, 0 .. width
    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    Kind kind;
    int selected = 0;
    juce::ParameterAttachment attachment;
};

// A drop-down menu bound to a choice parameter.
class CxMenu : public juce::ComboBox
{
public:
    CxMenu (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& name);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};
