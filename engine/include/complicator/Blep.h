// COMPLICATOR engine: band-limited corrections for one waveform output.
//
// The oscillators first compute a "naive" (sharp-edged) waveform. Every jump (step) and every sharp corner (change
// of slope) is then smoothed over the two samples around it with a small polynomial (2-point polyBLEP and polyBLAMP).
// That removes most of the harsh aliasing; the 4x oversampling and the decimation filter remove the rest.
//
// To be able to correct the sample *before* a jump, each output runs one (oversampled) sample late.
// All outputs run equally late, so they stay in step with each other.
//
// 'd' is how long ago the event happened, as a fraction of one sample: 0 <= d < 1.
// Header only.

#pragma once

namespace complicator
{
    struct BlepOut
    {
        float hold = 0.0f;       // the previous sample, already holding its own corrections
        float pendPrev = 0.0f;   // corrections still to add to the previous sample
        float pendCur = 0.0f;    // corrections for the sample being computed now

        void reset (float v = 0.0f) { hold = v; pendPrev = pendCur = 0.0f; }

        // A jump of height h (new value minus old value).
        void step (float h, float d)
        {
            const float e = 1.0f - d;
            pendPrev += 0.5f * h * d * d;
            pendCur  -= 0.5f * h * e * e;
        }

        // A corner: the slope (change per sample) changed by ds.
        void ramp (float ds, float d)
        {
            const float e = 1.0f - d;
            pendPrev += ds * d * d * d * (1.0f / 6.0f);
            pendCur  += ds * e * e * e * (1.0f / 6.0f);
        }

        // Give the naive value of the current sample; returns the corrected previous sample.
        float push (float naive)
        {
            const float out = hold + pendPrev;
            hold = naive + pendCur;
            pendPrev = pendCur = 0.0f;
            return out;
        }
    };
} // namespace complicator
