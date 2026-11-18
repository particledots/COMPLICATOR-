// COMPLICATOR engine: factory presets. Plain C++, no plugin code. Header only.
//
// The first nine presets are my own settings that rebuild the example patches in the Cs-L manual (using only what
// COMPLICATOR has: there is no filter or VCA, so the Amp envelope plays the part of the VCA, and the "sequencer or
// keyboard" is the keys). Where an example needs an external signal, the sidechain input stands in for it.
//
// The rest (index kNumManual and up) are my own inventions, not from the manual: bells and keys, plucks and brass,
// expressive patches played with velocity / mod wheel / aftertouch, leads, pads and drones, basses, percussion, and
// two for outside audio. They are only ever added at the END of the list: the Preset menu index is saved with a
// project, so the order of existing entries must never change.
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
    constexpr int kNumManual = 9;       // the manual's example patches come first
    constexpr int kNumFactory = 31;

    inline const char* name (int i)
    {
        static const char* const names[kNumFactory] = {
            "East Coast Voice", "Complete Voice", "Synced Growl", "Formant Voice", "Spooky Theremin",
            "Wavefold Multiply", "Racket Voice", "Five Modules", "Wavefolder (sidechain in)",
            "Glass Bell",
            "Tine Piano",
            "Metal Gong",
            "Fold Pluck",
            "Velocity Brass",
            "Reed Organ",
            "Hollow Flute",
            "Pressure Vibrato",
            "Wheel Folder",
            "Sync Sweep Lead",
            "Soft Sync Fifth",
            "Stair Pad",
            "Open Fifth Drone",
            "Deep Sub",
            "Wobble Bass",
            "Acid Fold",
            "Sine Kick",
            "Laser Zap",
            "Metal Hat",
            "Dub Siren",
            "Tangle",
            "Ring Voice (sidechain in)" };
        return (i >= 0 && i < kNumFactory) ? names[i] : "";
    }

    // The manual page the preset comes from, or "Own design: ...", for the README.
    inline const char* source (int i)
    {
        static const char* const src[kNumFactory] = {
            "East Coast Synth Voice", "Complete Synth Voice", "Sync'd Growl", "Formant Voice", "Spooky Theremin Voice",
            "Wavefold to multiply with sync and FM", "The Bonnie Racket Voice", "Five Separate Modules At Once (parts)",
            "Five Separate Modules At Once: Osc A as wavefolder",
            "Own design: FM bell",
            "Own design: FM electric piano",
            "Own design: ring-modulated gong",
            "Own design: envelope-folded pluck",
            "Own design: velocity-folded brass",
            "Own design: organ",
            "Own design: flute",
            "Own design: aftertouch vibrato",
            "Own design: mod wheel morph",
            "Own design: sync sweep lead",
            "Own design: soft sync fifth",
            "Own design: slow PWM pad",
            "Own design: drone",
            "Own design: sub bass",
            "Own design: LFO wavefold bass",
            "Own design: acid bass",
            "Own design: pitch-envelope kick",
            "Own design: downward sync sweep",
            "Own design: metallic hat",
            "Own design: LFO siren",
            "Own design: mutual feedback FM",
            "Own design: ring modulator for outside audio" };
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

            case 9:   // FM bell. Osc B (a ratio of 1 : 1.4 above Osc A, not heard much) frequency-modulates Osc A, and the envelope opens
                      // the Index, so the strike is a bright, inharmonic clang that relaxes into a pure sine as it rings out.
                p.keys = KeysAB;
                p.b.coarse = 5.82f;                       // ratio 1 : 1.4
                p.a.fm = 0.80f; p.a.fmCurve = FmLin;
                p.routeFmA = true; p.index = 0.0f; p.indexCv = 1.0f; p.patch[InIndex] = SrcEnvelope;
                p.mix[MixASine] = 0.85f; p.mix[MixBSine] = 0.15f;
                p.envAttack = 0.0f; p.envDecay = 0.88f; p.envSustain = 0.0f; p.envRelease = 0.88f;
                p.volume = 0.545f;
                break;

            case 10:   // FM electric piano. Same idea as the bell with Osc B at the same pitch (1 : 1): the envelope makes the attack bright and
                      // lets it mellow; velocity pushes Osc A's folder, so harder notes bark.
                p.keys = KeysAB;
                p.b.coarse = 0.0f;
                p.a.fm = 0.62f; p.a.fmCurve = FmLin;
                p.routeFmA = true; p.index = 0.05f; p.indexCv = 0.95f; p.patch[InIndex] = SrcEnvelope;
                p.a.fold = 0.5f; p.a.foldCv = 0.35f; p.patch[InFoldA] = SrcVelocity;
                p.mix[MixAFinal] = 0.9f; p.mix[MixBSine] = 0.12f;
                p.envAttack = 0.0f; p.envDecay = 0.82f; p.envSustain = 0.04f; p.envRelease = 0.72f;
                p.volume = 0.585f;
                break;

            case 11:   // Ring-modulated gong. A folded Osc A times an Osc B tuned to an inharmonic interval; the envelope opens Osc A's folder on
                      // the strike, so it starts harsh and goes dark while it rings for seconds.
                p.keys = KeysAB;
                p.b.coarse = 7.3f;
                p.a.fold = 0.62f; p.a.sym = 0.2f; p.a.foldCv = 0.45f; p.patch[InFoldA] = SrcEnvelope;
                p.patch[InModulator] = SrcAFinal; p.patch[InCarrier] = SrcBSine;
                p.multMode = 0; p.multDepth = 0.72f;
                p.mix[MixMult] = 0.9f;
                p.ampMode = AmpEnv;
                p.envAttack = 0.0f; p.envDecay = 0.93f; p.envSustain = 0.0f; p.envRelease = 0.93f;
                p.volume = 0.651f;
                break;

            case 12:   // The envelope opens Osc A's folder (like a filter sweep): bright at the pluck, a plain sine at the end. Osc B adds an octave
                      // below.
                p.keys = KeysAB;
                p.b.coarse = -12.0f;
                p.a.fold = 0.5f; p.a.foldCv = 0.85f; p.patch[InFoldA] = SrcEnvelope;
                p.mix[MixAFinal] = 0.85f; p.mix[MixBSine] = 0.35f;
                p.envAttack = 0.0f; p.envDecay = 0.66f; p.envSustain = 0.0f; p.envRelease = 0.66f;
                p.volume = 0.909f;
                break;

            case 13:   // Both oscillators (a little detuned) go through their folders, and velocity sets how hard they are folded: soft notes are
                      // dull and quiet, hard notes are bright and loud. The attack swells.
                p.keys = KeysAB;
                p.b.fine = 0.20f;
                p.a.fold = 0.15f; p.a.foldCv = 0.80f; p.patch[InFoldA] = SrcVelocity;
                p.b.fold = 0.15f; p.b.foldCv = 0.80f; p.patch[InFoldB] = SrcVelocity;
                p.mix[MixAFinal] = 0.55f; p.mix[MixBFinal] = 0.32f;
                p.envAttack = 0.55f; p.envDecay = 0.5f; p.envSustain = 0.85f; p.envRelease = 0.5f;
                p.volume = 0.890f;
                break;

            case 14:   // A folded sine an octave down, plus Osc B's square and triangle: a reedy organ with a slow swell.
                p.keys = KeysAB;
                p.a.coarse = -12.0f; p.a.fold = 0.62f;
                p.b.coarse = 0.0f;
                p.mix[MixAFinal] = 0.55f; p.mix[MixBSquare] = 0.3f; p.mix[MixBTri] = 0.3f;
                p.envAttack = 0.3f; p.envDecay = 0.5f; p.envSustain = 1.0f; p.envRelease = 0.4f;
                p.volume = 0.716f;
                break;

            case 15:   // Osc B's sine with a little triangle and staircase, and a small steady vibrato (Osc A as a slow LFO through the Index).
                p.keys = KeysB;
                p.a.coarse = 28.0f; p.aLfo = true;
                p.b.fm = 0.08f; p.b.fmCurve = FmExp; p.routeFmB = true; p.index = 0.3f;
                p.b.pwm = 0.8f;
                p.mix[MixBPwm] = 0.12f; p.mix[MixBTri] = 0.25f; p.mix[MixBSine] = 0.8f;
                p.envAttack = 0.5f; p.envDecay = 0.5f; p.envSustain = 0.9f; p.envRelease = 0.5f;
                p.volume = 0.500f;
                break;

            case 16:   // Osc A as a slow LFO bends Osc B (through the Index), and aftertouch sets how much: no pressure = steady, full pressure =
                      // about +/- half a semitone.
                p.keys = KeysB;
                p.a.coarse = 28.0f; p.aLfo = true;
                p.b.fm = 0.10f; p.b.fmCurve = FmExp;
                p.routeFmB = true; p.index = 0.0f; p.indexCv = 1.0f; p.patch[InIndex] = SrcAftertouch;
                p.mix[MixBTri] = 0.55f; p.mix[MixBSine] = 0.4f;
                p.envAttack = 0.45f; p.envDecay = 0.5f; p.envSustain = 0.9f; p.envRelease = 0.55f;
                p.volume = 0.660f;
                break;

            case 17:   // The mod wheel opens Osc A's folder and its Symmetry: wheel down is a soft sine, wheel up is rich and bright.
                p.keys = KeysAB;
                p.b.coarse = -12.0f;
                p.a.fold = 0.5f; p.a.foldCv = 0.85f; p.patch[InFoldA] = SrcModWheel;
                p.a.sym = 0.35f; p.patch[InSymA] = SrcModWheel;
                p.mix[MixAFinal] = 0.85f; p.mix[MixBTri] = 0.35f;
                p.envAttack = 0.25f; p.envDecay = 0.5f; p.envSustain = 0.9f; p.envRelease = 0.5f;
                p.volume = 0.493f;
                break;

            case 18:   // Osc A is hard-synced to Osc B and the envelope sweeps its pitch, so the sync timbre sweeps on every note.
                p.keys = KeysAB; p.sync = SyncHard;
                p.a.coarse = 4.0f; p.a.fm = 0.45f; p.a.fmCurve = FmExp; p.patch[InFmA] = SrcEnvelope;
                p.mix[MixASaw] = 0.8f;
                p.envAttack = 0.05f; p.envDecay = 0.65f; p.envSustain = 0.4f; p.envRelease = 0.5f;
                p.volume = 0.912f;
                break;

            case 19:   // Soft sync with Osc B a fifth above: the two lock into their natural 2 : 3 pattern, a power-chord sound.
                p.keys = KeysAB; p.sync = SyncSoft;
                p.b.coarse = 7.0f;
                p.mix[MixASaw] = 0.5f; p.mix[MixBTri] = 0.5f; p.mix[MixBSine] = 0.2f;
                p.envAttack = 0.2f; p.envDecay = 0.5f; p.envSustain = 0.9f; p.envRelease = 0.5f;
                p.volume = 0.747f;
                break;

            case 20:   // Osc A as a slow LFO moves Osc B's staircase width (A to B only), so the pad's colour drifts; a slow attack and release.
                p.keys = KeysB;
                p.a.coarse = 26.0f; p.aLfo = true; p.routePwm = 2; p.index = 0.7f;
                p.bSub = 1;
                p.b.pwm = 0.5f;
                p.mix[MixBPwm] = 0.30f; p.mix[MixBTri] = 0.55f; p.mix[MixBSine] = 0.45f; p.mix[MixBSquare] = 0.12f;
                p.envAttack = 0.7f; p.envDecay = 0.5f; p.envSustain = 0.9f; p.envRelease = 0.75f;
                p.volume = 0.571f;
                break;

            case 21:   // A drone (the Amp is Drone): a triangle and a fifth above it, slightly detuned, plus Osc B's sub square. Always sounding.
                p.keys = KeysAB;
                p.b.coarse = 7.0f; p.b.fine = 0.07f; p.bSub = 1;
                p.mix[MixATri] = 0.6f; p.mix[MixBTri] = 0.5f; p.mix[MixBSquare] = 0.15f;
                p.ampMode = AmpDrone;
                p.volume = 0.666f;
                break;

            case 22:   // Osc B's sine with its sub square one octave down: a plain, big bass.
                p.keys = KeysB; p.bSub = 1;
                p.mix[MixBSine] = 0.75f; p.mix[MixBSquare] = 0.4f;
                p.envAttack = 0.2f; p.envDecay = 0.5f; p.envSustain = 1.0f; p.envRelease = 0.45f;
                p.volume = 0.513f;
                break;

            case 23:   // Osc A as an LFO (about 5 Hz) folds Osc B (through its CV input) and B's sub square; a wobble bass.
                p.keys = KeysB;
                p.a.coarse = 27.0f; p.aLfo = true;
                p.b.coarse = -12.0f; p.bSub = 1;
                p.b.fold = 0.40f; p.b.foldCv = 0.55f; p.patch[InFoldB] = SrcATri;
                p.mix[MixBFinal] = 0.8f; p.mix[MixBSquare] = 0.35f;
                p.envAttack = 0.1f; p.envDecay = 0.5f; p.envSustain = 1.0f; p.envRelease = 0.4f;
                p.volume = 0.604f;
                break;

            case 24:   // The envelope opens Osc A's folder, and Symmetry adds the even harmonics: a squelchy acid bass.
                p.keys = KeysA;
                p.a.coarse = 0.0f; p.a.fold = 0.42f; p.a.foldCv = 0.7f; p.patch[InFoldA] = SrcEnvelope;
                p.a.sym = 0.25f;
                p.mix[MixAFinal] = 0.85f;
                p.envAttack = 0.0f; p.envDecay = 0.58f; p.envSustain = 0.25f; p.envRelease = 0.45f;
                p.volume = 0.963f;
                break;

            case 25:   // The envelope drives Osc A's pitch (FM A, exponential) from about four octaves above down to the note, and the Amp envelope
                      // closes: a kick. Play low notes.
                p.keys = KeysA;
                p.a.coarse = -6.0f; p.a.fm = 0.85f; p.a.fmCurve = FmExp;
                p.patch[InFmA] = SrcEnvelope;
                p.mix[MixASine] = 0.95f;
                p.envAttack = 0.0f; p.envDecay = 0.66f; p.envSustain = 0.0f; p.envRelease = 0.66f;
                p.volume = 0.731f;
                break;

            case 26:   // Osc A is hard-synced to Osc B and the envelope sweeps its pitch down about four octaves.
                p.keys = KeysAB; p.sync = SyncHard;
                p.a.coarse = 0.0f; p.a.fm = 0.90f; p.a.fmCurve = FmExp;
                p.patch[InFmA] = SrcEnvelope;
                p.b.coarse = 0.0f;
                p.mix[MixASaw] = 0.85f;
                p.envAttack = 0.0f; p.envDecay = 0.68f; p.envSustain = 0.0f; p.envRelease = 0.68f;
                p.volume = 0.889f;
                break;

            case 27:   // Osc B's square times a folded, very high Osc A, through the multiplier, with a short envelope: a metallic hat.
                p.keys = KeysB;
                p.a.coarse = 30.0f; p.a.fold = 0.8f;
                p.b.coarse = 12.3f;
                p.patch[InCarrier] = SrcBSquare; p.patch[InModulator] = SrcAFinal;
                p.multMode = 0; p.multDepth = 0.72f;
                p.mix[MixMult] = 1.0f;
                p.envAttack = 0.0f; p.envDecay = 0.60f; p.envSustain = 0.0f; p.envRelease = 0.60f;
                p.volume = 0.950f;
                break;

            case 28:   // Osc A as an LFO (a ramp, about 2 Hz) sweeps Osc B's pitch (FM B, exponential, driven by A Saw): a siren.
                p.keys = KeysB;
                p.a.coarse = 12.0f; p.aLfo = true;
                p.b.fm = 0.62f; p.b.fmCurve = FmExp; p.patch[InFmB] = SrcASaw;
                p.mix[MixBSine] = 0.8f; p.mix[MixBTri] = 0.25f;
                p.envAttack = 0.0f; p.envDecay = 0.5f; p.envSustain = 1.0f; p.envRelease = 0.55f;
                p.volume = 0.481f;
                break;

            case 29:   // Each oscillator's folded Final frequency-modulates the other (through the Patch row's one-sample feedback): a dense,
                      // locked-up tone.
                p.keys = KeysAB;
                p.b.coarse = 7.0f;
                p.patch[InFmA] = SrcBFinal; p.patch[InFmB] = SrcAFinal;
                p.a.fm = 0.35f; p.b.fm = 0.35f; p.a.fmCurve = FmExp; p.b.fmCurve = FmExp;
                p.a.fold = 0.65f; p.b.fold = 0.65f;
                p.mix[MixAFinal] = 0.5f; p.mix[MixBFinal] = 0.5f;
                p.envAttack = 0.2f; p.envDecay = 0.5f; p.envSustain = 0.8f; p.envRelease = 0.5f;
                p.volume = 0.831f;
                break;

            case 30:   // The Side Chain signal is the multiplier's Modulator and Osc B's sine the Carrier: a ring modulator for outside audio.
                      // Silent until a Side Chain source is chosen; the keys tune the carrier.
                p.keys = KeysB;
                p.patch[InModulator] = SrcSidechain; p.patch[InCarrier] = SrcBSine;
                p.multMode = 0; p.multDepth = 0.72f;
                p.mix[MixMult] = 0.9f;
                p.ampMode = AmpDrone;
                p.volume = 0.963f;
                break;

            default:
                break;
        }
        return p;
    }
} // namespace presets
} // namespace complicator
