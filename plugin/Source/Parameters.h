// Every control of COMPLICATOR, as Logic sees it.
//
// IMPORTANT: these ids, their order, ranges and menu lists were fixed in the first build (milestone 1) and must never
// change. Saved projects and automation refer to them. Controls for later milestones are already here (they are
// stored, and shown dimmed on the panel) so nothing has to be added in between.
// If something must be added one day, add it at the END with ParameterID version 2.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace CxParams
{
    // Osc A (sawtooth core)
    inline constexpr const char* aCoarse  = "aCoarse";
    inline constexpr const char* aFine    = "aFine";
    inline constexpr const char* aPwm     = "aPwm";
    inline constexpr const char* aFm      = "aFm";
    inline constexpr const char* aFmCurve = "aFmCurve";
    inline constexpr const char* aLfo     = "aLfo";
    inline constexpr const char* aFold    = "aFold";
    inline constexpr const char* aFoldCv  = "aFoldCv";
    inline constexpr const char* aSym     = "aSym";
    // Osc B (triangle core)
    inline constexpr const char* bCoarse  = "bCoarse";
    inline constexpr const char* bFine    = "bFine";
    inline constexpr const char* bPwm     = "bPwm";
    inline constexpr const char* bFm      = "bFm";
    inline constexpr const char* bFmCurve = "bFmCurve";
    inline constexpr const char* bSub     = "bSub";
    inline constexpr const char* bFold    = "bFold";
    inline constexpr const char* bFoldCv  = "bFoldCv";
    inline constexpr const char* bSym     = "bSym";
    // Pitch and sync
    inline constexpr const char* keys       = "keys";
    inline constexpr const char* pitchSnap  = "pitchSnap";
    inline constexpr const char* pitchScale = "pitchScale";
    inline constexpr const char* pitchKey   = "pitchKey";
    inline constexpr const char* sync       = "sync";
    // Multiply and Index (M2)
    inline constexpr const char* multDepth = "multDepth";
    inline constexpr const char* multMode  = "multMode";
    inline constexpr const char* index     = "index";
    inline constexpr const char* indexCv   = "indexCv";
    // Index routing (M2)
    inline constexpr const char* routeFmA   = "routeFmA";
    inline constexpr const char* routeFmB   = "routeFmB";
    inline constexpr const char* routeFoldA = "routeFoldA";
    inline constexpr const char* routeFoldB = "routeFoldB";
    inline constexpr const char* routeSymA  = "routeSymA";
    inline constexpr const char* routeSymB  = "routeSymB";
    inline constexpr const char* routePwm   = "routePwm";

    // Patch row (M2): 13 inputs, in the engine's PatchInput order.
    inline constexpr int numPatch = 13;
    extern const char* const patchIds[numPatch];
    extern const char* const patchNames[numPatch];     // full names (for Logic)
    extern const char* const patchShort[numPatch];     // short names (for the panel)

    // Output mixer: 11 levels, in the engine's MixSlot order.
    inline constexpr int numMix = 11;
    extern const char* const mixIds[numMix];
    extern const char* const mixNames[numMix];

    // Envelope, amp, volume, quality
    inline constexpr const char* envAttack  = "envAttack";
    inline constexpr const char* envDecay   = "envDecay";
    inline constexpr const char* envSustain = "envSustain";
    inline constexpr const char* envRelease = "envRelease";
    inline constexpr const char* ampMode    = "ampMode";
    inline constexpr const char* volume     = "volume";
    inline constexpr const char* quality    = "quality";   // M3

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
