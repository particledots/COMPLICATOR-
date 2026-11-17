// The plugin wrapper. It talks to Logic, reads the control values and the MIDI notes and passes them to the engine.
// It contains no sound-making code.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <complicator/Engine.h>
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

    int getNumPrograms() override { return 1; }                      // factory presets arrive in milestone 3
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    const complicator::Engine& getEngine() const { return engine; }
    void setScopeActive (bool on) { engine.setScopeActive (on); }     // the panel is open: record the scopes

private:
    complicator::Params readParams() const;
    void handleMidi (const juce::MidiMessage&);

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
