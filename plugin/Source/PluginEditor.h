// The COMPLICATOR panel. Laid out at 680 x 664 units exactly like the approved draft (2026-11-17) and scaled to the
// window (drag the corner to resize; the shape stays the same).
// Controls that arrive in a later milestone are already in place, dimmed and labelled with that milestone.

#pragma once

#include "PluginProcessor.h"
#include "Controls.h"

class ComplicatorPanel : public juce::Component, private juce::Timer
{
public:
    explicit ComplicatorPanel (ComplicatorAudioProcessor&);
    ~ComplicatorPanel() override;

    void paint (juce::Graphics&) override;
    void attachPopups (juce::Component* parent);

private:
    void timerCallback() override;
    CxKnob& knob (const char* id, juce::Colour c, float cx, float cy, float r, bool dim = false);
    CxSegments& segs (const char* id, juce::StringArray labels, juce::Colour c, juce::Rectangle<int> r,
                      bool toggle = false, bool dim = false);
    CxMenu& menu (const char* id, const juce::String& prefix, juce::Rectangle<int> r, bool dim = false);
    void dimIt (juce::Component&);

    ComplicatorAudioProcessor& proc;
    CxLookAndFeel lnf;
    std::vector<std::unique_ptr<CxKnob>> knobs;
    std::vector<std::unique_ptr<CxSegments>> segments;
    std::vector<std::unique_ptr<CxMenu>> menus;
    juce::Slider foldA, foldB;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> foldAAtt, foldBAtt;
    juce::ComboBox presetMenu;
    float shownA = 0.0f, shownB = 0.0f;
};

class ComplicatorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ComplicatorAudioProcessorEditor (ComplicatorAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ComplicatorPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComplicatorAudioProcessorEditor)
};
