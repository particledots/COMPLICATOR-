// The plugin wrapper. It talks to Logic, reads the control values and the MIDI notes and passes them to the engine.
// It contains no sound-making code.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <complicator/Engine.h>
#include <complicator/Presets.h>
#include "Parameters.h"

class ComplicatorAudioProcessor : public juce::AudioProcessor
{
public:
    ComplicatorAudioProcessor();
    ~ComplicatorAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }   // the longest Release

    // Programs = the Preset menu: 0 is "Init" (every control at its default), then the factory presets.
    // A preset never touches Pitch Snap, Scale, Key or Quality.
    int getNumPrograms() override { return 1 + complicator::presets::kNumFactory; }
    int getCurrentProgram() override { return programIndex.load(); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    // For the panel: the program last chosen (0 = Init), or -1 for settings loaded from a project saved before presets.
    int shownPreset() const { return presetShown.load(); }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    complicator::Params readParams() const;                              // what the engine is given
    void applyParams (const complicator::Params&);                       // sets the controls (all but Snap, Scale, Key, Quality)
    const complicator::Engine& getEngine() const { return engine; }
    void setScopeActive (bool on) { engine.setScopeActive (on); }     // the panel is open: record the scopes

private:
    void handleMidi (const juce::MidiMessage&);

    std::atomic<int> programIndex { 0 }, presetShown { 0 };
    juce::AudioProcessorValueTreeState apvts;
    complicator::Engine engine;
    std::vector<float> sidechainMono;        // the sidechain input summed to mono, copied before the output is written

    // Fast access to the current control values (audio thread).
    struct OscPtrs { std::atomic<float> *coarse, *fine, *pwm, *fm, *fmCurve, *fold, *foldCv, *sym; };
    OscPtrs pa {}, pb {};
    std::atomic<float> *aLfo {}, *bSub {}, *keys {}, *snap {}, *scale {}, *key {}, *sync {};
    std::atomic<float> *multDepth {}, *multMode {}, *index {}, *indexCv {};
    std::atomic<float> *routes[7] {};
    std::atomic<float> *patch[CxParams::numPatch] {};
    std::atomic<float> *mix[CxParams::numMix] {};
    std::atomic<float> *envA {}, *envD {}, *envS {}, *envR {}, *ampMode {}, *volume {}, *quality {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComplicatorAudioProcessor)
};
