// COMPLICATOR engine: factory presets. Plain C++, no plugin code. Header only.
//
// The presets are my own settings that rebuild the example patches in the Cs-L manual (using only what COMPLICATOR has:
// there is no filter or VCA, so the Amp envelope plays the part of the VCA, and the "sequencer or keyboard" is the keys).
// Where an example needs an external signal, the sidechain input stands in for it.
//
// A preset sets everything that makes the sound (every oscillator, mixer, Patch and envelope setting, and Volume).
// It never changes Pitch Snap, Scale, Key or Quality: those are the player's own set-up and stay as they are.
//
// Every preset sets the Final levels it needs: the mixer levels start at 0, so a preset that listens to a wavefolder
// (directly in the mix, or through the Patch row into the multiplier) says so itself.

#pragma once
#include "Params.h"

namespace complicator
{
namespace presets
{
    constexpr int kNumFactory = 9;

    inline const char* name (int i)
    {
        static const char* const names[kNumFactory] = {
            "East Coast Voice", "Complete Voice", "Synced Growl", "Formant Voice", "Spooky Theremin",
            "Wavefold Multiply", "Racket Voice", "Five Modules", "Wavefolder (sidechain in)" };
        return (i >= 0 && i < kNumFactory) ? names[i] : "";
    }

    // The manual page the preset comes from, for the README.
    inline const char* source (int i)
    {
        static const char* const src[kNumFactory] = {
            "East Coast Synth Voice", "Complete Synth Voice", "Sync'd Growl", "Formant Voice", "Spooky Theremin Voice",
            "Wavefold to multiply with sync and FM", "The Bonnie Racket Voice", "Five Separate Modules At Once (parts)",
            "Five Separate Modules At Once: Osc A as wavefolder" };
        return (i >= 0 && i < kNumFactory) ? src[i] : "";
    }

    // What the plugin's Init program sets: every control at its default value (the defaults in Parameters.cpp).
    inline Params init()
    {
        Params p;
        p.multDepth = 0.72f;
        p.mix[MixASaw] = 0.7f;
        return p;
    }

    // All the defaults, but with the whole mixer silent: where every preset starts.
    inline Params blank()
    {
        Params p = init();
        for (auto& m : p.mix) m = 0.0f;
        return p;
    }

    inline Params factory (int i)
    {
        Params p = blank();
        switch (i)
        {
            case 0:   // East Coast Synth Voice: Osc B's square (one octave down), PWM and triangle mixed; the keys play B;
                      // the envelope opens the "VCA".
                p.keys = KeysB;
                p.bSub = 1;
                p.b.pwm = 0.5f;
                p.mix[MixBSquare] = 0.70f; p.mix[MixBPwm] = 0.60f; p.mix[MixBTri] = 0.50f;
                p.envAttack = 0.20f; p.envDecay = 0.55f; p.envSustain = 0.65f; p.envRelease = 0.50f;
                p.volume = 0.52f;
                break;

            case 1:   // Complete Synth Voice: Final A into the multiplier's Modulator; the envelope into Fold A's CV and into
                      // the Carrier (the multiplier is the VCA); only the Multiply output is heard.
                p.keys = KeysA;
                p.a.fold = 0.35f; p.a.foldCv = 0.80f;
                p.patch[InFoldA] = SrcEnvelope;
                p.patch[InModulator] = SrcAFinal;
                p.patch[InCarrier] = SrcEnvelope;
                p.multDepth = 0.72f; p.multMode = 0;
                p.mix[MixMult] = 0.90f;
                p.ampMode = AmpDrone;
                p.envAttack = 0.30f; p.envDecay = 0.60f; p.envSustain = 0.60f; p.envRelease = 0.55f;
                p.volume = 0.62f;
                break;

            case 2:   // Sync'd Growl: both Finals ring-modulated; each sine moves the other oscillator's fold and symmetry
                      // (through the Index); Osc B's square hard-syncs Osc A; Osc B's sine frequency-modulates Osc A;
                      // the keys play both.
                p.keys = KeysAB;
                p.sync = SyncHard;
                p.b.coarse = 7.0f;
                p.a.fm = 0.25f; p.a.fmCurve = FmExp;
                p.a.fold = 0.55f; p.b.fold = 0.55f;
                p.a.foldCv = 0.50f; p.b.foldCv = 0.50f;
                p.a.sym = 0.40f; p.b.sym = -0.40f;
                p.routeFoldA = p.routeFoldB = p.routeSymA = p.routeSymB = true;
                p.index = 0.45f;
                p.patch[InModulator] = SrcAFinal;
                p.patch[InCarrier] = SrcBFinal;
                p.multDepth = 0.72f; p.multMode = 0;
                p.mix[MixMult] = 0.80f;
                p.envAttack = 0.20f; p.envDecay = 0.55f; p.envSustain = 0.70f; p.envRelease = 0.50f;
                p.volume = 1.00f;
                break;

            case 3:   // Formant Voice: Osc A's sine, half-wave rectified, times Osc B's square (two octaves down), which
                      // also hard-syncs Osc A. The keys play Osc B (raised two octaves so the played note is the pitch you
                      // hear); Osc A's pitch is the timbre.
                p.keys = KeysB;
                p.sync = SyncHard;
                p.bSub = 2;
                p.b.coarse = 24.0f;
                p.a.coarse = 24.0f;
                p.multMode = 1;
                p.patch[InCarrier] = SrcBSquare;
                p.multDepth = 0.72f;
                p.mix[MixMult] = 0.90f;
                p.envAttack = 0.20f; p.envDecay = 0.55f; p.envSustain = 0.70f; p.envRelease = 0.50f;
                p.volume = 0.63f;
                break;

            case 4:   // Spooky Theremin Voice: Osc B's sine, frequency-modulated a little (linear) by Osc A as a slow LFO
                      // (about 4 Hz around middle C).
                p.keys = KeysAB;
                p.aLfo = true;
                p.a.coarse = 12.0f;
                p.b.fmCurve = FmLin; p.b.fm = 0.09f;
                p.mix[MixBSine] = 0.85f;
                p.envAttack = 0.50f; p.envDecay = 0.50f; p.envSustain = 1.00f; p.envRelease = 0.60f;
                p.volume = 0.47f;
                break;

            case 5:   // Wavefold to multiply with sync and FM: Osc A hard-synced to Osc B's square; Final A times Osc B's
                      // sine; Osc B's sine also frequency-modulates Osc A; the keys play Osc B; Osc A is the timbre.
                p.keys = KeysB;
                p.sync = SyncHard;
                p.a.coarse = 9.0f;
                p.a.fm = 0.30f; p.a.fmCurve = FmExp;
                p.a.fold = 0.70f; p.a.sym = 0.30f;
                p.patch[InModulator] = SrcAFinal;
                p.multDepth = 0.72f; p.multMode = 0;
                p.mix[MixMult] = 0.90f;
                p.envAttack = 0.20f; p.envDecay = 0.55f; p.envSustain = 0.70f; p.envRelease = 0.50f;
                p.volume = 0.75f;
                break;

            case 6:   // The Bonnie Racket Voice: Osc A's pulse (Carrier) ring-modulated by Osc B's staircase (Modulator); the
                      // product into Osc A's symmetry input, then folded; both oscillators frequency-modulate each other
                      // (exponential); Osc B hard-syncs Osc A; the Index moves both PWMs; Final A is heard.
                p.keys = KeysAB;
                p.sync = SyncHard;
                p.b.coarse = 12.0f;
                p.a.fmCurve = FmExp; p.b.fmCurve = FmExp;
                p.a.fm = 0.30f; p.b.fm = 0.30f;
                p.a.pwm = 0.60f; p.b.pwm = 0.40f;
                p.routePwm = 1;
                p.index = 0.50f;
                p.a.fold = 0.60f; p.a.sym = 0.50f;
                p.patch[InCarrier] = SrcAPwm;
                p.patch[InModulator] = SrcBPwm;
                p.patch[InSymA] = SrcMultiply;
                p.multDepth = 0.72f; p.multMode = 0;
                p.mix[MixAFinal] = 0.80f;
                p.envAttack = 0.20f; p.envDecay = 0.55f; p.envSustain = 0.70f; p.envRelease = 0.50f;
                p.volume = 0.68f;
                break;

            case 7:   // Five Separate Modules At Once, from inside: Osc A as a slow LFO moves Osc B's wavefolder, Osc B is
                      // the oscillator (with its sub square one octave down), and the multiplier is a VCA opened by the
                      // envelope.
                p.keys = KeysB;
                p.a.coarse = 15.0f; p.aLfo = true;
                p.bSub = 1;
                p.b.fold = 0.50f; p.b.foldCv = 0.60f;
                p.patch[InFoldB] = SrcATri;
                p.patch[InCarrier] = SrcBFinal;
                p.patch[InModulator] = SrcEnvelope;
                p.multDepth = 0.72f; p.multMode = 0;
                p.mix[MixMult] = 0.85f; p.mix[MixBSquare] = 0.30f;
                p.envAttack = 0.25f; p.envDecay = 0.55f; p.envSustain = 0.70f; p.envRelease = 0.55f;
                p.volume = 0.80f;
                break;

            case 8:   // Osc A as a wavefolder for outside audio: the fader is fully left, the audio comes in at the symmetry
                      // input (here: the sidechain) and the Symmetry knob sets how hard it is folded. Silent until a
                      // sidechain signal arrives.
                p.a.fold = 0.0f;
                p.a.sym = 1.0f;
                p.patch[InSymA] = SrcSidechain;
                p.mix[MixAFinal] = 0.80f;
                p.ampMode = AmpDrone;
                p.volume = 0.53f;
                break;

            default:
                break;
        }
        return p;
    }
} // namespace presets
} // namespace complicator
