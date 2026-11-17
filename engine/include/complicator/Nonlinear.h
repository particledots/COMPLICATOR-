// COMPLICATOR engine: the non-linear stages (wavefolder, multiplier soft clip, rectifiers), band-limited.
//
// Each stage is a function f(x) with a known antiderivative F(x). "First-order antiderivative anti-aliasing" (ADAA)
// replaces f(x[n]) by the average of f over the straight line between the previous and the current input sample:
//
//        y[n] = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])
//
// That is the output the stage would give if its input were a continuous signal, smoothed over one sample. Sharp
// corners (the folds, the clip knee, the rectifier corner) get rounded, so far less of their harmonics fold back into
// the audible range. It adds a delay of half an (oversampled) sample and a very slight high-frequency droop (about
// -0.5 dB at 20 kHz at 4x oversampling), the same for every stage. Header only, no dependencies.
//
// The maths is done in double precision: the difference of two nearly equal F values would lose too much in float.

#pragma once
#include <cmath>

namespace complicator
{
namespace nl
{
    // Triangle fold: f(x) = x while |x| <= 1, then mirrored back at +-1, again at +-3, and so on (period 4).
    // A sine of amplitude 1 passes through unchanged ("centre = sine"); louder inputs fold over.
    struct TriFold
    {
        static double f (double x)
        {
            double u = x + 1.0;
            u -= 4.0 * std::floor (u * 0.25);          // 0 <= u < 4
            const double v = u - 1.0;                   // -1 <= v < 3
            return v <= 1.0 ? v : 2.0 - v;
        }
        // Antiderivative. The area over one period is zero, so F is itself periodic (period 4).
        static double F (double x)
        {
            double u = x + 1.0;
            u -= 4.0 * std::floor (u * 0.25);
            const double v = u - 1.0;
            return v <= 1.0 ? 0.5 * v * v : 0.5 + 2.0 * (v - 1.0) - 0.5 * (v * v - 1.0);
        }
    };

    // Soft clip of the multiplier: exactly linear up to 1.0 (so a multiplier at unity gain passes full-size signals
    // untouched), then rounds off smoothly to a ceiling of 1.25, reached at 1.5 (flat beyond). No sharp corner
    // anywhere: the slope is continuous.
    struct SoftClip
    {
        static double f (double x)
        {
            const double a = std::fabs (x);
            double y;
            if (a <= 1.0) y = a;
            else if (a < 1.5) { const double u = a - 1.0; y = 1.0 + u - u * u; }
            else y = 1.25;
            return x < 0.0 ? -y : y;
        }
        static double F (double x)
        {
            const double a = std::fabs (x);
            if (a <= 1.0) return 0.5 * a * a;
            if (a < 1.5) { const double u = a - 1.0; return 0.5 + u + 0.5 * u * u - u * u * u * (1.0 / 3.0); }
            return 1.083333333333333333 + 1.25 * (a - 1.5);
        }
    };

    struct HalfRect   // negative half cut to zero
    {
        static double f (double x) { return x > 0.0 ? x : 0.0; }
        static double F (double x) { return x > 0.0 ? 0.5 * x * x : 0.0; }
    };

    struct AbsRect    // negative half turned over
    {
        static double f (double x) { return std::fabs (x); }
        static double F (double x) { return 0.5 * x * std::fabs (x); }
    };

    template <class Fn>
    struct Adaa1
    {
        double xp = 0.0, Fp = 0.0;

        void reset (double x = 0.0) { xp = x; Fp = Fn::F (x); }

        double next (double x)
        {
            const double Fx = Fn::F (x);
            const double dx = x - xp;
            const double y = (std::fabs (dx) < 1.0e-6) ? Fn::f (0.5 * (x + xp)) : (Fx - Fp) / dx;
            xp = x;
            Fp = Fx;
            return y;
        }
    };
} // namespace nl
} // namespace complicator
