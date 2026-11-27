// The COMPLICATOR panel in the "gold sections" look. Laid out at 680 x 708 units like the approved mock-up (2026-11-26)
// and scaled to the window (drag the corner to resize; the shape stays the same).
//
// Drawing is split in two so the panel stays light to draw:
//   - the fixed artwork (frames, titles, labels, knob arcs, flow lines) is a child component drawn once into a stored
//     picture (redrawn only when the window opens or is resized);
//   - 30 times a second only the live areas are redrawn: the two scopes and the Coarse value texts.

#pragma once

#include "PluginProcessor.h"
#include "Controls.h"
#include <array>

class ComplicatorPanel : public juce::Component, private juce::Timer
{
public:
    explicit ComplicatorPanel (ComplicatorAudioProcessor&);
    ~ComplicatorPanel() override;

    void paintOverChildren (juce::Graphics&) override;
    void attachPopups (juce::Component* parent);
    void tickNow() { timerCallback(); }     // for the check program: what the timer does 30 times a second

    // For the check program: the fixed artwork layer, and the areas redrawn 30 times a second (panel units).
    juce::Component& getBackdrop() { return backdrop; }
    static juce::Array<juce::Rectangle<int>> liveAreas();
    // For the check program: the box of every text the fixed artwork draws (panel units).
    static std::vector<juce::Rectangle<float>> fixedTextBoxes();

private:
    struct Backdrop : public juce::Component
    {
        void paint (juce::Graphics& g) override { ComplicatorPanel::paintArt (g); }
    };

    static void paintArt (juce::Graphics&);
    void timerCallback() override;
    CxKnob& knob (const char* id, float cx, float cy, float ring);
    CxSegments& segs (const char* id, juce::StringArray labels, juce::Rectangle<float> r, CxSegments::Kind kind);
    CxMenu& menu (const char* id, const juce::String& name, juce::Rectangle<float> r);
    void updateScope (int which);
    void updatePresetMenu();

    ComplicatorAudioProcessor& proc;
    CxLookAndFeel lnf;
    Backdrop backdrop;
    std::vector<std::unique_ptr<CxKnob>> knobs;
    std::vector<std::unique_ptr<CxSegments>> segments;
    std::vector<std::unique_ptr<CxMenu>> menus;
    juce::Slider foldA, foldB;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> foldAAtt, foldBAtt;
    juce::ComboBox presetMenu;
    CxMenu* qualityMenu = nullptr;
    int shownPresetId = -2;
    float shownA = 0.0f, shownB = 0.0f;
    static constexpr int kScopeW = 174;                          // scope columns (panel units)
    std::array<std::array<float, kScopeW + 1>, 2> scopeLine {};  // Final A / Final B, -1..+1
};

class ComplicatorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ComplicatorAudioProcessorEditor (ComplicatorAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    ComplicatorPanel& getPanel() { return panel; }

private:
    ComplicatorPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComplicatorAudioProcessorEditor)
};
