// COMPLICATOR engine: one voice of the complex oscillator. Plain C++17, no plugin code.
//
// The plugin wrapper hands it the settings (Params), the notes and pitch bend, and asks for audio.
// Everything inside runs at 4x the project's sample rate and is brought back down with a steep filter.

#pragma once
#include "Decimator.h"
#include "Oscillators.h"
#include "Params.h"

#include <atomic>
#include <vector>

namespace complicator
{
    class Engine
    {
    public:
        static constexpr int kMaxFactor = 8;

        void prepare (double sampleRate, int maxBlockSize);
        void reset();

        // Audio thread only (the wrapper calls these from its processBlock, in time order).
        void setParams (const Params& p) { params = p; }
        void noteOn (int note, float velocity);
        void noteOff (int note);
        void allNotesOff();
        void pitchBend (float semitones);          // already scaled: -2..+2
        void modWheel (float v)   { wheel = v; }   // 0..1 (used from M2 on)
        void aftertouch (float v) { touch = v; }   // 0..1 (used from M2 on)

        // Mono voice; the same signal goes to every output channel.
        void process (float* const* out, int numChannels, int numSamples);

        // For the panel: the tuning offsets really played (after Pitch Snap / Scale), in semitones.
        float playedCoarseA() const { return shownCoarseA.load (std::memory_order_relaxed); }
        float playedCoarseB() const { return shownCoarseB.load (std::memory_order_relaxed); }
        int oversamplingFactor() const { return factor; }
        double latencySamples() const { return decim.latency(); }

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

        float quantizedCoarse (int which, float coarse, int baseNote);
        float fmFrequency (float f, float amount, int curve, float input) const;

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
        float lastSineA = 0.0f, lastSineB = 0.0f;
        float dcX = 0.0f, dcY = 0.0f, dcCoef = 0.9995f;

        Smooth sCoarse[2], sFine[2], sPwm[2], sFm[2], sBend, sVolume, sAmp;
        Smooth sMix[kNumMix];

        // Cache for the Snap / Scale search (it only reruns when its inputs change).
        struct QCache { float coarse = 1.0e9f; int base = -1, snap = -1, scale = -1, key = -1; float result = 0.0f; };
        QCache qcache[2];

        std::atomic<float> shownCoarseA { 0.0f }, shownCoarseB { 0.0f };
    };
} // namespace complicator
