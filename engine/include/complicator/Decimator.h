// COMPLICATOR engine: brings the oversampled signal back down to the project's sample rate.
//
// A linear-phase low-pass filter (windowed sinc, Kaiser window) followed by keeping one sample in 'factor'.
// It is designed for the actual sample rate: everything that would fold back into the audible range
// (below 20 kHz) is removed by at least about 100 dB.

#pragma once
#include <vector>

namespace complicator
{
    class Decimator
    {
    public:
        // baseRate = the project's sample rate, factor = 2, 4 or 8.
        void prepare (double baseRate, int factor);
        void reset();

        // Feed exactly 'factor' oversampled samples; returns one output sample.
        float process (const float* in);

        int factor() const { return fac; }
        int numTaps() const { return (int) taps.size(); }
        // Delay of the filter, in output (project-rate) samples.
        double latency() const { return ((double) taps.size() - 1.0) * 0.5 / (double) fac; }

    private:
        int fac = 4;
        std::vector<float> taps;
        std::vector<float> buf;    // twice the filter length, so the newest samples are always one straight run
        int pos = 0;
    };
} // namespace complicator
