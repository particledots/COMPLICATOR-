// COMPLICATOR engine: the two oscillator cores. Original code.
//
// Osc A is a "sawtooth core": a phase that rises from 0 to 1 and jumps back. Sine, triangle, sawtooth and pulse are
// all read from that phase. Hard sync resets the phase.
//
// Osc B is a "triangle core": a value that runs up to +1, turns round, runs down to -1, turns round. Its square wave
// is the direction of travel, its sine is the triangle bent into a sine shape, and its "PWM" output is a staircase
// triangle whose top and bottom steps get wider with the PWM knob. Soft sync makes it turn round early
// (only while it is rising through the upper half, so it locks rather than scrambles).
//
// Both run at the oversampled rate and smooth every jump and corner (see Blep.h).

#pragma once
#include "Blep.h"

namespace complicator
{
    struct OscAOut { float sine = 0, tri = 0, saw = 0, pulse = 0; };
    struct OscBOut { float sine = 0, tri = 0, square = 0, stepped = 0; float syncEdge = -1.0f; };

    class OscA
    {
    public:
        void reset();

        // inc: phase change per (oversampled) sample; negative runs backwards (through-zero FM).
        // duty: 0..1. resetAt: < 0 for no hard sync in this sample, otherwise how long ago the reset happened (0..1).
        // Returns < 0, or how long ago the phase wrapped round (used for soft sync of Osc B).
        float process (double inc, float duty, float resetAt, OscAOut& out);

        double phase() const { return p; }

    private:
        void segment (double pa, double inc, float t0, float t1, float duty, bool pulseOn, float& wrapAt);

        double p = 0.0;
        BlepOut bSine, bTri, bSaw, bPulse;
    };

    class OscB
    {
    public:
        void reset();

        // speed: change of the triangle per (oversampled) sample = 4 x frequency / sample rate (negative: backwards).
        // stepWidth: where the staircase's top and bottom steps start (0..1, smaller = wider steps).
        // subMode: 0 = square at the oscillator's pitch, 1 = one octave down, 2 = two octaves down.
        // reverseAt: < 0 for no soft sync in this sample, otherwise how long ago the turn-round happened (0..1).
        // out.syncEdge: < 0, or how long ago the (sub) square rose (used for hard sync of Osc A).
        void process (double speed, float stepWidth, int subMode, float reverseAt, OscBOut& out);

    private:
        void move (double speed, float t0, float t1, float stepWidth, int subMode, float& edge);
        void linearPiece (double xa, double xb, float ta, float tb, float stepWidth);
        void turnRound (double speed, float d, int subMode, float& edge);

        double x = 0.0;
        int dir = 1;          // +1 rising, -1 falling; this is also the square wave
        int sub1 = 1, sub2 = 1;
        BlepOut bSine, bTri, bSquare, bStep;
    };
} // namespace complicator
