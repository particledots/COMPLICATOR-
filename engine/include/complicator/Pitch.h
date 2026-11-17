// COMPLICATOR engine: Pitch Snap, Scale and Key.
//
// These act only on the Coarse knobs (the tuning offset of each oscillator, in semitones). Fine is never moved.
//
//   Snap Off        the offset is used as it is (smooth).
//   Snap Semitones  the offset lands on whole semitones.
//   Snap Steps      the offset lands on octaves and fifths: 0, +-7, +-12, +-19, +-24, +-31, +-36, +-43, +-48.
//
//   Scale (Off + 28) and Key (C..B): with a scale on, the offset only lands where
//   (the oscillator's own note + offset) is a note of that scale in that key. The oscillator's own note is the note
//   you play (or MIDI note 48 for an oscillator that does not follow the keys). That note itself and its octaves are
//   always allowed, so Coarse 0 always plays exactly the note you played, even one outside the key.
//   With a scale on and Snap Off, the offset still moves in whole notes (a scale has no "between" notes).
//
// Header only, no dependencies.

#pragma once
#include "Scales.h"
#include <cmath>

namespace complicator
{
namespace pitch
{
    enum Snap { SnapOff = 0, SnapSemitones = 1, SnapSteps = 2 };

    constexpr int kSteps[] = { -48, -43, -36, -31, -24, -19, -12, -7, 0, 7, 12, 19, 24, 31, 36, 43, 48 };
    constexpr float kCoarseRange = 48.0f;

    inline int wrap12 (int n) { n %= 12; return n < 0 ? n + 12 : n; }

    // True when 'offset' (whole semitones) is allowed above 'baseNote' for this scale mask and key.
    inline bool allowed (int offset, int baseNote, int mask, int key)
    {
        if (mask == 0 || wrap12 (offset) == 0) return true;            // no scale, or the note itself / its octaves
        return (mask >> wrap12 (baseNote + offset - key)) & 1;
    }

    // The offset (semitones) actually played for a Coarse knob value 'coarse' (-48..+48).
    inline float quantize (float coarse, int snap, int scaleMenu, int key, int baseNote)
    {
        if (coarse > kCoarseRange) coarse = kCoarseRange;
        if (coarse < -kCoarseRange) coarse = -kCoarseRange;
        const int mask = scales::maskFor (scaleMenu);
        if (snap == SnapOff && mask == 0) return coarse;

        int best = 0;
        float bestDist = 1.0e9f;
        auto consider = [&] (int o)
        {
            if (! allowed (o, baseNote, mask, key)) return;
            const float dist = std::fabs ((float) o - coarse);
            // Ties go to the offset nearer to 0, so the result never depends on the search order.
            if (dist < bestDist - 1.0e-6f || (std::fabs (dist - bestDist) <= 1.0e-6f && std::abs (o) < std::abs (best)))
            { best = o; bestDist = dist; }
        };
        if (snap == SnapSteps)
            for (int o : kSteps) consider (o);
        else
            for (int o = -48; o <= 48; ++o) consider (o);
        return (float) best;
    }
} // namespace pitch
} // namespace complicator
