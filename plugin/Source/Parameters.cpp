#include "Parameters.h"
#include <complicator/Scales.h>

namespace CxParams
{

const char* const patchIds[numPatch] = {
    "patchFmA", "patchFmB", "patchPwmA", "patchPwmB", "patchFoldA", "patchFoldB", "patchSymA", "patchSymB",
    "patchIndex", "patchCarrier", "patchModulator", "patchHardSync", "patchSoftSync" };
const char* const patchNames[numPatch] = {
    "Patch: FM A in", "Patch: FM B in", "Patch: PWM A CV", "Patch: PWM B CV", "Patch: Fold A CV", "Patch: Fold B CV",
    "Patch: Symmetry A in", "Patch: Symmetry B in", "Patch: Index CV", "Patch: Multiply carrier",
    "Patch: Multiply modulator", "Patch: Hard sync A in", "Patch: Soft sync B in" };
const char* const patchShort[numPatch] = {
    "FM A", "FM B", "PWM A", "PWM B", "Fold A", "Fold B", "Sym A", "Sym B", "Index", "Carr.", "Mod.", "Hard", "Soft" };

const char* const mixIds[numMix] = {
    "mixASine", "mixATri", "mixASaw", "mixAPwm", "mixAFinal", "mixMult",
    "mixBSine", "mixBTri", "mixBSquare", "mixBPwm", "mixBFinal" };
const char* const mixNames[numMix] = {
    "Mix: A sine", "Mix: A triangle", "Mix: A sawtooth", "Mix: A PWM", "Mix: A final", "Mix: Multiply",
    "Mix: B sine", "Mix: B triangle", "Mix: B square", "Mix: B PWM", "Mix: B final" };

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::String percent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
    juce::String bipolar (float v, int) { const int p = juce::roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + juce::String (p) + "%"; }

    void addFloat (Layout& l, const char* id, const char* name, float lo, float hi, float def,
                   std::function<juce::String (float, int)> text)
    {
        l.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, 0.0f), def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (text))));
    }
    void addChoice (Layout& l, const char* id, const char* name, const juce::StringArray& items, int def)
    {
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
    }
    void addBool (Layout& l, const char* id, const char* name, bool def)
    {
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    }

    juce::String semis (float v, int)
    {
        return (v > 0.0049f ? "+" : "") + juce::String (v, 2) + " st";
    }
    juce::String seconds (float knob, int)
    {
        const float s = 0.001f * std::pow (10000.0f, knob);           // same mapping as the engine: 1 ms .. 10 s
        return s < 1.0f ? juce::String (juce::roundToInt (s * 1000.0f)) + " ms" : juce::String (s, 2) + " s";
    }
    juce::String decibels (float v, int)
    {
        const float g = v * v * 2.0f;                                 // same mapping as the engine
        return g <= 0.00001f ? juce::String ("-inf dB") : juce::String (juce::Decibels::gainToDecibels (g), 1) + " dB";
    }

    void addOsc (Layout& l, bool isA)
    {
        const char* osc = isA ? "Osc A " : "Osc B ";
        auto n = [osc] (const char* what) { return juce::String (osc) + what; };
        addFloat (l, isA ? aCoarse : bCoarse, n ("Coarse").toRawUTF8(), -48.0f, 48.0f, 0.0f, semis);
        addFloat (l, isA ? aFine : bFine,     n ("Fine").toRawUTF8(),   -1.0f, 1.0f, 0.0f, semis);
        addFloat (l, isA ? aPwm : bPwm,       n ("PWM").toRawUTF8(),     0.0f, 1.0f, 0.5f, percent);
        addFloat (l, isA ? aFm : bFm,         n ("FM amount").toRawUTF8(), 0.0f, 1.0f, 0.0f, percent);
        addChoice (l, isA ? aFmCurve : bFmCurve, n ("FM curve").toRawUTF8(), { "Lin", "Exp", "Through-zero" }, 1);
        if (isA) addBool (l, aLfo, "Osc A LFO mode", false);
        else     addChoice (l, bSub, "Osc B Sub", { "Off", "-1 octave", "-2 octaves" }, 0);
        addFloat (l, isA ? aFold : bFold,     n ("Wavefold").toRawUTF8(), 0.0f, 1.0f, 0.5f, percent);
        addFloat (l, isA ? aFoldCv : bFoldCv, n ("Wavefold CV amount").toRawUTF8(), -1.0f, 1.0f, 0.0f, bipolar);
        addFloat (l, isA ? aSym : bSym,       n ("Symmetry").toRawUTF8(), -1.0f, 1.0f, 0.0f, bipolar);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    Layout l;

    addOsc (l, true);
    addOsc (l, false);

    addChoice (l, keys, "Keys play", { "A", "A+B", "B" }, 1);
    addChoice (l, pitchSnap, "Pitch Snap", { "Off", "Semitones", "Steps" }, 0);
    juce::StringArray scaleNames { "Off" };
    for (int i = 0; i < complicator::scales::kNumScales; ++i)
        scaleNames.add (complicator::scales::kScales[i].name);
    addChoice (l, pitchScale, "Pitch Scale", scaleNames, 0);
    addChoice (l, pitchKey, "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    addChoice (l, sync, "Sync", { "Off", "Soft (A to B)", "Hard (B to A)" }, 0);

    addFloat (l, multDepth, "Multiply depth", 0.0f, 1.0f, 0.72f, percent);
    addChoice (l, multMode, "Multiply mode", { "Ring", "Half-wave", "Full-wave" }, 0);
    addFloat (l, index, "Index", 0.0f, 1.0f, 0.5f, percent);
    addFloat (l, indexCv, "Index CV amount", -1.0f, 1.0f, 0.0f, bipolar);

    addBool (l, routeFmA,   "Route: Index to FM A", false);
    addBool (l, routeFmB,   "Route: Index to FM B", false);
    addBool (l, routeFoldA, "Route: Index to Fold A", false);
    addBool (l, routeFoldB, "Route: Index to Fold B", false);
    addBool (l, routeSymA,  "Route: Index to Symmetry A", false);
    addBool (l, routeSymB,  "Route: Index to Symmetry B", false);
    addChoice (l, routePwm, "Route: Index to PWM", { "Off", "Both", "A to B only" }, 0);

    const juce::StringArray sources { "Normal", "Off", "A Sine", "A Tri", "A Saw", "A PWM", "A Final",
                                      "B Sine", "B Tri", "B Square", "B PWM", "B Final",
                                      "Multiply", "Envelope", "Velocity", "Mod wheel", "Aftertouch", "Sidechain" };
    for (int i = 0; i < numPatch; ++i)
        addChoice (l, patchIds[i], patchNames[i], sources, 0);

    for (int i = 0; i < numMix; ++i)
        addFloat (l, mixIds[i], mixNames[i], 0.0f, 1.0f, i == 2 ? 0.7f : 0.0f, percent);   // A sawtooth on to start

    addFloat (l, envAttack,  "Attack",  0.0f, 1.0f, 0.15f, seconds);
    addFloat (l, envDecay,   "Decay",   0.0f, 1.0f, 0.55f, seconds);
    addFloat (l, envSustain, "Sustain", 0.0f, 1.0f, 0.7f, percent);
    addFloat (l, envRelease, "Release", 0.0f, 1.0f, 0.5f, seconds);
    addChoice (l, ampMode, "Amp", { "Drone", "Env" }, 1);
    addFloat (l, volume, "Volume", 0.0f, 1.0f, 0.6f, decibels);
    addChoice (l, quality, "Quality", { "2x", "4x", "8x" }, 1);

    return l;
}

} // namespace CxParams
