// COMPLICATOR engine: one voice of the complex oscillator. Plain C++17, no plugin code.
//
// The plugin wrapper hands it the settings (Params), the notes and pitch bend, and asks for audio.
// Everything inside runs at 4x the project's sample rate and is brought back down with a steep filter.
//
// Signal flow per oversampled sample (milestone 2):
//   1. Sources: the oscillators' outputs of the previous sample, the envelope, velocity, mod wheel, aftertouch and the
//      sidechain. (Every loop through the Patch row therefore runs on the previous oversampled sample.)
//   2. Inputs: for each of the 13 inputs, either its "normal" connection (as on the module, see codeFor) or the
//      source chosen in the Patch row. Changes are cross-faded over a few milliseconds.
//   3. Oscillators A and B (FM, PWM, sync).
//   4. Wavefolders (A and B) -> Final A, Final B; multiplier -> Multiply.
//   5. Output mixer.

#pragma once
#include "Decimator.h"
#include "Nonlinear.h"
#include "Oscillators.h"
#include "Params.h"

#include <array>
#include <atomic>
#include <vector>

namespace complicator
{
    class Engine
    {
    public:
        static constexpr int kMaxFactor = 8;
        static constexpr int kScopeSize = 4096;     // samples kept for each scope (at the project's sample rate)

        Engine();

        void prepare (double sampleRate, int maxBlockSize);
        void reset();

        // Audio thread only (the wrapper calls these from its processBlock, in time order).
        void setParams (const Params& p) { params = p; }
        void noteOn (int note, float velocity);
        void noteOff (int note);
        void allNotesOff();
        void pitchBend (float semitones);          // already scaled: -2..+2
        void modWheel (float v)   { wheel = v; }   // 0..1
        void aftertouch (float v) { touch = v; }   // 0..1

        // Mono voice; the same signal goes to every output channel.
        // 'sidechain' (optional): numSamples mono samples of the sidechain input, for the Patch row's "Sidechain".
        void process (float* const* out, int numChannels, int numSamples, const float* sidechain = nullptr);

        // For the panel: the tuning offsets really played (after Pitch Snap / Scale), in semitones.
        float playedCoarseA() const { return shownCoarseA.load (std::memory_order_relaxed); }
        float playedCoarseB() const { return shownCoarseB.load (std::memory_order_relaxed); }
        int oversamplingFactor() const { return factor; }
        double latencySamples() const { return decim.latency(); }

        // Scopes. While the panel is open (setScopeActive (true)) the engine records Final A (which = 0) and Final B
        // (which = 1) just before the amp and volume. scopeCopy runs on the panel's thread: it copies the newest 'n'
        // samples (n <= kScopeSize), oldest first.
        void setScopeActive (bool on) { scopeActive.store (on, std::memory_order_relaxed); }
        void scopeCopy (int which, float* dst, int n) const;

    private:
        struct Smooth
        {
            float v = 0.0f, coef = 0.0f;
            float next (float target) { v += coef * (target - v); return v; }
        };

        struct Envelope
        {
            enum Stage { Idle, Attack, Decay, Sustain, Release };
            Stage stage = Idle;
            float level = 0.0f;
            void gateOn()  { stage = Attack; }
            void gateOff() { if (stage != Idle) stage = Release; }
            float next (float attackStep, float decayCoef, float sustain, float releaseCoef);
        };

        // Finds the rising edge of a patched signal through a threshold, to a fraction of a sample.
        struct EdgeDetector
        {
            float prev = 0.0f;
            bool armed = true;
            // Returns < 0 for no edge, otherwise how long ago (0..1 of a sample) the signal crossed the threshold.
            float next (float x, float threshold);
        };

        static constexpr int kContinuousInputs = 11;    // all inputs except the two sync inputs

        float quantizedCoarse (int which, float coarse, int baseNote);
        float fmFrequency (float f, float amount, int curve, float input) const;
        int codeFor (int input, int selection) const;
        float sourceValue (int code) const;
        float readInput (int k);
        void clearFeedback();

        Params params;
        double sr = 48000.0, osRate = 192000.0;
        int factor = 4;
        int noteStack[128] = {};
        int stackSize = 0;
        int playedNote = 48;
        float velocity = 1.0f, wheel = 0.0f, touch = 0.0f;
        float bendTarget = 0.0f;

        OscA oscA;
        OscB oscB;
        Decimator decim;
        Envelope env;
        float envLast = 0.0f;
        float dcX = 0.0f, dcY = 0.0f, dcCoef = 0.9995f;

        Smooth sCoarse[2], sFine[2], sPwm[2], sFm[2], sBend, sVolume, sAmp;
        Smooth sFold[2], sFoldCv[2], sSym[2], sDepth, sIndex, sIndexCv, sVel, sWheel, sTouch;
        Smooth sMode[3];                                   // multiplier mode weights (Ring, Half-wave, Full-wave)
        Smooth sMix[kNumMix];

        // Patch row. S[] holds every source as of the previous oversampled sample (index = PatchSource).
        std::array<float, kNumSources> S {};
        float curASine = 0.0f, curBSine = 0.0f;           // this sample's sines (the "normal" for the folder / multiplier)
        float idxGain = 0.0f;
        int inCode[kContinuousInputs] = {}, inPrev[kContinuousInputs] = {};
        float inFade[kContinuousInputs] = {};
        float fadeStep = 0.01f;
        EdgeDetector hardEdge, softEdge;

        // Non-linear stages. 'primed' = the stage has been running, so its memory of the previous sample is valid.
        nl::Adaa1<nl::TriFold> foldAdaa[2];
        nl::Adaa1<nl::SoftClip> clipAdaa;
        nl::Adaa1<nl::HalfRect> halfAdaa;
        nl::Adaa1<nl::AbsRect> absAdaa;
        bool foldPrimed[2] = {}, clipPrimed = false, halfPrimed = false, absPrimed = false;

        // Sidechain: the last four samples, for the cubic interpolation up to the oversampled rate.
        float scHist[4] = {};

        // Cache for the Snap / Scale search (it only reruns when its inputs change).
        struct QCache { float coarse = 1.0e9f; int base = -1, snap = -1, scale = -1, key = -1; float result = 0.0f; };
        QCache qcache[2];

        std::atomic<float> shownCoarseA { 0.0f }, shownCoarseB { 0.0f };

        std::atomic<bool> scopeActive { false };
        std::array<std::atomic<float>, kScopeSize> scopeBuf[2];
        std::atomic<int> scopeWrite { 0 };
    };
} // namespace complicator
