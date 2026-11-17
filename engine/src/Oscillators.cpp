#include "complicator/Oscillators.h"

#include <cmath>

namespace complicator
{
namespace
{
    constexpr double kTwoPi = 6.283185307179586476925;
    constexpr double kHalfPi = 1.570796326794896619231;

    inline double wrap01 (double v) { v -= std::floor (v); return v >= 1.0 ? 0.0 : v; }
    inline float sawOf (double p) { return (float) (2.0 * p - 1.0); }
    inline float triOf (double p) { return (float) (p < 0.5 ? -1.0 + 4.0 * p : 3.0 - 4.0 * p); }
    inline float sineOf (double p) { return (float) std::sin (kTwoPi * p); }

    // The pulse has no edges when the duty cycle is at (or beyond) either end: it is then a constant.
    inline bool pulseHasEdges (float duty) { return duty > 0.001f && duty < 0.999f; }
    inline float pulseOf (double p, float duty)
    {
        if (! pulseHasEdges (duty)) return duty >= 0.999f ? 1.0f : -1.0f;
        return p < (double) duty ? 1.0f : -1.0f;
    }
    inline float clampD (float d) { return d < 0.0f ? 0.0f : (d >= 1.0f ? 0.99999f : d); }

    // Staircase triangle: four levels. 'w' is where the top and bottom steps start.
    inline float steppedOf (double x, float w)
    {
        if (x > w) return 1.0f;
        if (x > 0.0) return 1.0f / 3.0f;
        if (x > -w) return -1.0f / 3.0f;
        return -1.0f;
    }
}

// ------------------------------------------------------------------------------------------------ Osc A

void OscA::reset()
{
    p = 0.0;
    bSine.reset (0.0f); bTri.reset (-1.0f); bSaw.reset (-1.0f); bPulse.reset (1.0f);
    lastPulse = 1.0f; pulseIssued = 0.0f;
}

void OscA::segment (double pa, double inc, float t0, float t1, float duty, bool pulseOn, float& wrapAt)
{
    if (inc == 0.0 || t1 <= t0) return;
    const double pb = pa + inc * (double) (t1 - t0);
    const bool up = inc > 0.0;
    const float dir = up ? 1.0f : -1.0f;
    const float absInc = (float) std::fabs (inc);

    auto crossed = [&] (double c) { return up ? (c > pa && c <= pb) : (c < pa && c >= pb); };
    auto ago = [&] (double c) { return clampD (1.0f - (t0 + (float) ((c - pa) / inc))); };

    for (int k = -1; k <= 2; ++k)
    {
        const double cw = (double) k;            // phase wraps round: sawtooth jumps, triangle bottom corner, pulse rises
        if (crossed (cw))
        {
            const float d = ago (cw);
            bSaw.step (-2.0f * dir, d);
            bTri.ramp (8.0f * absInc, d);
            if (pulseOn) { bPulse.step (2.0f * dir, d); pulseIssued += 2.0f * dir; }
            if (wrapAt < 0.0f) wrapAt = d;
        }
        const double ch = k + 0.5;               // triangle top corner
        if (crossed (ch))
            bTri.ramp (-8.0f * absInc, ago (ch));
        if (pulseOn)
        {
            const double cd = k + (double) duty; // pulse falls
            if (crossed (cd))
            {
                bPulse.step (-2.0f * dir, ago (cd));
                pulseIssued -= 2.0f * dir;
            }
        }
    }
}

float OscA::process (double inc, float duty, float resetAt, OscAOut& out)
{
    float wrapAt = -1.0f;
    const bool pulseOn = pulseHasEdges (duty);

    if (resetAt < 0.0f)
    {
        segment (p, inc, 0.0f, 1.0f, duty, pulseOn, wrapAt);
        p = wrap01 (p + inc);
    }
    else
    {
        const float d = clampD (resetAt);
        const float tr = 1.0f - d;
        segment (p, inc, 0.0f, tr, duty, pulseOn, wrapAt);
        const double pr = wrap01 (p + inc * (double) tr);

        // The jump back to phase 0, for every waveform.
        bSaw.step (-1.0f - sawOf (pr), d);
        bTri.step (-1.0f - triOf (pr), d);
        bSine.step (0.0f - sineOf (pr), d);
        const float resetJump = pulseOf (0.0, duty) - pulseOf (pr, duty);
        bPulse.step (resetJump, d);
        pulseIssued += resetJump;
        // ... and the change of slope it causes.
        const float triBefore = (float) ((pr < 0.5 ? 4.0 : -4.0) * inc);
        bTri.ramp ((float) (4.0 * inc) - triBefore, d);
        const float sineBefore = (float) (kTwoPi * std::cos (kTwoPi * pr) * inc);
        bSine.ramp ((float) (kTwoPi * inc) - sineBefore, d);

        segment (0.0, inc, tr, 1.0f, duty, pulseOn, wrapAt);
        p = wrap01 (inc * (double) d);
    }

    // A pulse edge that moved past the phase (audio-rate PWM) was not caught above: smooth it here, centred on the sample.
    const float pulseNow = pulseOf (p, duty);
    const float residual = pulseNow - (lastPulse + pulseIssued);
    if (std::fabs (residual) > 0.5f) bPulse.step (residual, 0.5f);
    lastPulse = pulseNow;
    pulseIssued = 0.0f;

    out.sine  = bSine.push (sineOf (p));
    out.tri   = bTri.push (triOf (p));
    out.saw   = bSaw.push (sawOf (p));
    out.pulse = bPulse.push (pulseNow);
    return wrapAt;
}

// ------------------------------------------------------------------------------------------------ Osc B

void OscB::reset()
{
    x = 0.0; dir = 1; sub1 = 1; sub2 = 1;
    bSine.reset (0.0f); bTri.reset (0.0f); bSquare.reset (1.0f); bStep.reset (-1.0f / 3.0f);
    lastStep = -1.0f / 3.0f; stepIssued = 0.0f;
}

void OscB::linearPiece (double xa, double xb, float ta, float tb, float w)
{
    if (xa == xb) return;
    const double cs[3] = { -(double) w, 0.0, (double) w };
    const bool rising = xb > xa;
    for (double c : cs)
    {
        if (rising ? (c >= xa && c < xb) : (c < xa && c >= xb))
        {
            const float te = ta + (float) ((c - xa) / (xb - xa)) * (tb - ta);
            const float h = rising ? (2.0f / 3.0f) : (-2.0f / 3.0f);
            bStep.step (h, clampD (1.0f - te));
            stepIssued += h;
        }
    }
}

void OscB::turnRound (double speed, float d, int subMode, float& edge)
{
    const double v = dir * speed;                                   // slope before the turn
    bTri.ramp ((float) (-2.0 * v), d);
    bSine.ramp ((float) (std::cos (kHalfPi * x) * kHalfPi * -2.0 * v), d);

    dir = -dir;
    if (subMode == 0) bSquare.step (2.0f * (float) dir, d);
    if (dir == 1)                                                   // the square rose: count octaves down
    {
        sub1 = -sub1;
        if (subMode == 1) bSquare.step (2.0f * (float) sub1, d);
        if (sub1 == 1)
        {
            sub2 = -sub2;
            if (subMode == 2) bSquare.step (2.0f * (float) sub2, d);
        }
    }
    const bool selRose = (subMode == 0 && dir == 1) || (subMode == 1 && dir == 1 && sub1 == 1)
                      || (subMode == 2 && dir == 1 && sub1 == 1 && sub2 == 1);
    if (selRose && edge < 0.0f) edge = d;
}

void OscB::move (double speed, float t0, float t1, float w, int subMode, float& edge)
{
    float t = t0;
    for (int guard = 0; guard < 4 && t < t1; ++guard)
    {
        const double v = dir * speed;
        if (v == 0.0) break;
        const double xe = x + v * (double) (t1 - t);
        if (xe > 1.0 && v > 0.0)
        {
            const float tc = t + (float) ((1.0 - x) / v);
            linearPiece (x, 1.0, t, tc, w);
            x = 1.0;
            turnRound (speed, clampD (1.0f - tc), subMode, edge);
            t = tc;
        }
        else if (xe < -1.0 && v < 0.0)
        {
            const float tc = t + (float) ((-1.0 - x) / v);
            linearPiece (x, -1.0, t, tc, w);
            x = -1.0;
            turnRound (speed, clampD (1.0f - tc), subMode, edge);
            t = tc;
        }
        else
        {
            linearPiece (x, xe, t, t1, w);
            x = xe;
            t = t1;
        }
    }
    if (x > 1.0) x = 1.0;
    if (x < -1.0) x = -1.0;
}

void OscB::process (double speed, float w, int subMode, float reverseAt, OscBOut& out)
{
    float edge = -1.0f;
    if (reverseAt < 0.0f)
    {
        move (speed, 0.0f, 1.0f, w, subMode, edge);
    }
    else
    {
        // Soft sync: when Osc A's ramp restarts, Osc B turns round early, but only if it is on its way up and already
        // in the upper half. An Osc B tuned a little flat of Osc A (or of a whole multiple of it) is pulled into step
        // and stops beating; anything else is only nudged.
        const float d = clampD (reverseAt);
        move (speed, 0.0f, 1.0f - d, w, subMode, edge);
        if (dir * speed > 0.0 && dir * x > 0.0)
            turnRound (speed, d, subMode, edge);
        move (speed, 1.0f - d, 1.0f, w, subMode, edge);
    }

    const int sq = subMode == 0 ? dir : (subMode == 1 ? sub1 : sub2);
    const float stepNow = steppedOf (x, w);
    const float residual = stepNow - (lastStep + stepIssued);        // step edges moved by audio-rate PWM
    if (std::fabs (residual) > 0.3f) bStep.step (residual, 0.5f);
    lastStep = stepNow;
    stepIssued = 0.0f;

    out.tri     = bTri.push ((float) x);
    out.sine    = bSine.push ((float) std::sin (kHalfPi * x));
    out.square  = bSquare.push ((float) sq);
    out.stepped = bStep.push (stepNow);
    out.syncEdge = edge;
}

} // namespace complicator
