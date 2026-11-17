#include "complicator/Decimator.h"

#include <algorithm>
#include <cmath>

namespace complicator
{
namespace
{
    double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 64; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < sum * 1.0e-12) break;
        }
        return sum;
    }
}

void Decimator::prepare (double baseRate, int factor)
{
    fac = std::max (1, factor);
    const double osRate = baseRate * fac;

    // Keep everything up to 20 kHz (or 45% of the project rate, if lower). Remove everything that would fold back
    // below that point: that starts at (project rate - pass edge).
    const double passEdge = std::min (20000.0, 0.45 * baseRate);
    const double stopEdge = baseRate - passEdge;
    const double cutoff = 0.5 * (passEdge + stopEdge) / osRate;             // as a fraction of the oversampled rate
    const double width = std::max (1.0e-3, (stopEdge - passEdge) / osRate);
    const double atten = 100.0;                                             // dB
    const double beta = 0.1102 * (atten - 8.7);

    int n = (int) std::ceil ((atten - 7.95) / (2.285 * 2.0 * 3.14159265358979323846 * width)) + 1;
    n = std::clamp (n, 15, 1023);
    if ((n & 1) == 0) ++n;

    taps.assign ((size_t) n, 0.0f);
    const double mid = 0.5 * (n - 1);
    const double i0b = besselI0 (beta);
    double sum = 0.0;
    std::vector<double> t ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double x = i - mid;
        const double sinc = (x == 0.0) ? 2.0 * cutoff
                                       : std::sin (2.0 * 3.14159265358979323846 * cutoff * x) / (3.14159265358979323846 * x);
        const double r = x / mid;
        const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
        t[(size_t) i] = sinc * w;
        sum += t[(size_t) i];
    }
    for (int i = 0; i < n; ++i)
        taps[(size_t) i] = (float) (t[(size_t) i] / sum);

    buf.assign ((size_t) n * 2, 0.0f);
    pos = 0;
}

void Decimator::reset()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    pos = 0;
}

float Decimator::process (const float* in)
{
    const int n = (int) taps.size();
    for (int i = 0; i < fac; ++i)
    {
        pos = (pos == 0) ? n - 1 : pos - 1;
        buf[(size_t) pos] = buf[(size_t) (pos + n)] = in[i];
    }
    const float* b = buf.data() + pos;
    const float* h = taps.data();
    float acc = 0.0f;
    for (int k = 0; k < n; ++k)
        acc += h[k] * b[k];
    return acc;
}

} // namespace complicator
