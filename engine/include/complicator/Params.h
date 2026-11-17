// COMPLICATOR engine: every setting the engine reads. Plain values, no plugin code.
//
// The list matches the plugin's full parameter list, which was fixed in the first build (milestone 1) so that
// saved projects never change. Settings marked "M2" or "M3" are stored from the start but only used from that
// milestone on.

#pragma once

namespace complicator
{
    // The output mixer: one level per output of the module.
    enum MixSlot
    {
        MixASine, MixATri, MixASaw, MixAPwm, MixAFinal,
        MixMult,
        MixBSine, MixBTri, MixBSquare, MixBPwm, MixBFinal,
        kNumMix
    };

    // The inputs of the module that the Patch row can feed (M2).
    enum PatchInput
    {
        InFmA, InFmB, InPwmA, InPwmB, InFoldA, InFoldB, InSymA, InSymB,
        InIndex, InCarrier, InModulator, InHardSync, InSoftSync,
        kNumInputs
    };

    // What can feed an input (M2). 0 = Normal: as on the module (for inputs that have no normal: nothing).
    enum PatchSource
    {
        SrcNormal, SrcOff,
        SrcASine, SrcATri, SrcASaw, SrcAPwm, SrcAFinal,
        SrcBSine, SrcBTri, SrcBSquare, SrcBPwm, SrcBFinal,
        SrcMultiply, SrcEnvelope, SrcVelocity, SrcModWheel, SrcAftertouch, SrcSidechain,
        kNumSources
    };

    enum FmCurve { FmLin = 0, FmExp = 1, FmThroughZero = 2 };
    enum Keys { KeysA = 0, KeysAB = 1, KeysB = 2 };
    enum SyncMode { SyncOff = 0, SyncSoft = 1, SyncHard = 2 };
    enum AmpMode { AmpDrone = 0, AmpEnv = 1 };

    struct OscParams
    {
        float coarse = 0.0f;   // semitones, -48..+48 (Pitch Snap / Scale applied by the engine)
        float fine = 0.0f;     // semitones, -1..+1
        float pwm = 0.5f;      // 0..1
        float fm = 0.0f;       // 0..1 (FM amount)
        int fmCurve = FmExp;
        float fold = 0.5f;     // 0..1, 0.5 = plain sine (M2)
        float foldCv = 0.0f;   // -1..+1 (M2)
        float sym = 0.0f;      // -1..+1 (M2)
    };

    struct Params
    {
        OscParams a, b;
        bool aLfo = false;
        int bSub = 0;          // 0, 1 = one octave down, 2 = two octaves down

        int keys = KeysAB;
        int snap = 0;          // 0 Off, 1 Semitones, 2 Steps
        int scale = 0;         // 0 Off, 1..28
        int key = 0;           // 0 = C .. 11 = B
        int sync = SyncOff;

        float multDepth = 0.75f;   // M2
        int multMode = 0;          // M2: 0 Ring, 1 Half-wave, 2 Full-wave
        float index = 0.5f;        // M2
        float indexCv = 0.0f;      // M2
        bool routeFmA = false, routeFmB = false, routeFoldA = false, routeFoldB = false,
             routeSymA = false, routeSymB = false;   // M2
        int routePwm = 0;          // M2: 0 Off, 1 Both, 2 A to B only
        int patch[kNumInputs] = {};// M2

        float mix[kNumMix] = {};   // 0..1 each
        float envAttack = 0.15f, envDecay = 0.55f, envSustain = 0.7f, envRelease = 0.5f;   // 0..1 each
        int ampMode = AmpEnv;
        float volume = 0.6f;       // 0..1
        int quality = 1;           // M3: 0 = 2x, 1 = 4x, 2 = 8x oversampling
    };
} // namespace complicator
