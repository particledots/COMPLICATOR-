// Self-test for the COMPLICATOR engine. Run automatically by the build; the build fails if any check fails.

#include "complicator/Engine.h"
#include "complicator/Pitch.h"
#include "complicator/Scales.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <random>
#include <vector>

using namespace complicator;

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        ++checks;                                                          \
        if (! (cond)) {                                                    \
            std::printf ("FAIL line %d: %s   ", __LINE__, #cond);          \
            std::printf (__VA_ARGS__);                                     \
            std::printf ("\n");                                            \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

static const double kSr = 48000.0;
static const int kBlock = 256;

static Params quiet()
{
    Params p;
    for (auto& m : p.mix) m = 0.0f;
    p.ampMode = AmpDrone;
    p.volume = std::sqrt (0.5f);     // gain 1
    return p;
}

static std::vector<float> render (Engine& e, const Params& p, double seconds)
{
    std::vector<float> out ((size_t) (seconds * kSr));
    for (size_t pos = 0; pos < out.size(); pos += kBlock)
    {
        const int n = (int) std::min<size_t> (kBlock, out.size() - pos);
        float* ch[1] = { out.data() + pos };
        e.setParams (p);
        e.process (ch, 1, n);
    }
    return out;
}

static std::vector<float> renderNote (const Params& p, int note, double seconds, double skip = 0.25)
{
    Engine e;
    e.setParams (p);
    e.prepare (kSr, kBlock);
    e.noteOn (note, 1.0f);
    auto v = render (e, p, seconds + skip);
    return std::vector<float> (v.begin() + (long) (skip * kSr), v.end());
}

static double risingCrossingsHz (const std::vector<float>& v)
{
    int count = 0, first = -1, last = -1;
    for (size_t i = 1; i < v.size(); ++i)
        if (v[i - 1] < 0.0f && v[i] >= 0.0f)
        {
            if (first < 0) first = (int) i;
            last = (int) i;
            ++count;
        }
    if (count < 2) return 0.0;
    return (count - 1) * kSr / (double) (last - first);
}

static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * 3.14159265358979323846 / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                auto u = a[i + k], t = a[i + k + len / 2] * w;
                a[i + k] = u + t;
                a[i + k + len / 2] = u - t;
                w *= wl;
            }
        }
    }
}

// Power that is NOT near a harmonic of f0 (aliasing and noise), relative to the power near the harmonics, in dB.
// Only 20 Hz .. 20 kHz is looked at.
static double aliasDb (const std::vector<float>& v, double f0)
{
    const size_t n = 32768;
    std::vector<std::complex<double>> a (n);
    for (size_t i = 0; i < n; ++i)
    {
        // 4-term Blackman-Harris window: side lobes below -92 dB, so a strong harmonic does not smear into its neighbours.
        const double x = 2.0 * 3.14159265358979323846 * (double) i / (double) n;
        const double w = 0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2.0 * x) - 0.01168 * std::cos (3.0 * x);
        a[i] = v[i] * w;
    }
    fft (a);
    double harm = 0.0, rest = 0.0;
    const double binHz = kSr / (double) n;
    for (size_t k = 1; k < n / 2; ++k)
    {
        const double f = (double) k * binHz;
        if (f < 20.0 || f > 20000.0) continue;
        const double pw = std::norm (a[k]);
        const double h = f / f0;
        const double dist = std::fabs (h - std::round (h)) * f0 / binHz;   // distance to the nearest harmonic, in bins
        if (std::round (h) >= 1.0 && dist <= 6.0) harm += pw; else rest += pw;
    }
    return 10.0 * std::log10 ((rest + 1.0e-30) / (harm + 1.0e-30));
}

static double peak (const std::vector<float>& v)
{
    double m = 0.0;
    for (float s : v) m = std::max (m, (double) std::fabs (s));
    return m;
}

static double rms (const std::vector<float>& v)
{
    double s = 0.0;
    for (float x : v) s += (double) x * x;
    return std::sqrt (s / (double) std::max<size_t> (1, v.size()));
}

static double hz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

static void testPitchRules()
{
    using namespace pitch;
    CHECK (quantize (5.3f, SnapOff, 0, 0, 60) == 5.3f, "snap off keeps the value");
    CHECK (quantize (5.3f, SnapSemitones, 0, 0, 60) == 5.0f, "semitones");
    CHECK (quantize (-5.6f, SnapSemitones, 0, 0, 60) == -6.0f, "semitones negative");
    CHECK (quantize (9.0f, SnapSteps, 0, 0, 60) == 7.0f, "steps: 9 -> 7");
    CHECK (quantize (10.0f, SnapSteps, 0, 0, 60) == 12.0f, "steps: 10 -> 12");
    CHECK (quantize (47.0f, SnapSteps, 0, 0, 60) == 48.0f, "steps: 47 -> 48");
    CHECK (quantize (99.0f, SnapOff, 0, 0, 60) == 48.0f, "clamped to +48");
    // Major (menu 1) in C, playing C (60): C# is not allowed; tie between C and D goes to 0.
    CHECK (quantize (1.0f, SnapOff, 1, 0, 60) == 0.0f, "major C, +1 -> 0 (got %g)", quantize (1.0f, SnapOff, 1, 0, 60));
    CHECK (quantize (1.2f, SnapOff, 1, 0, 60) == 2.0f, "major C, +1.2 -> 2");
    CHECK (quantize (6.0f, SnapSemitones, 1, 0, 60) == 5.0f, "major C, F# -> F (tie toward 0)");
    // Playing C# (61, outside C major): offset 0 and octaves stay allowed; +1 (D) is allowed.
    CHECK (quantize (0.2f, SnapSemitones, 1, 0, 61) == 0.0f, "played note itself always allowed");
    CHECK (quantize (12.0f, SnapSemitones, 1, 0, 61) == 12.0f, "octave of the played note allowed");
    CHECK (quantize (1.0f, SnapSemitones, 1, 0, 61) == 1.0f, "C# + 1 = D, in C major");
    // Key D major (key 2), playing D (62): +4 = F# allowed, +3 = F not.
    CHECK (quantize (3.4f, SnapSemitones, 1, 2, 62) == 4.0f, "D major: +3.4 -> +4 (F#)");
    // Steps with a scale: fifths that are not in the scale are skipped. Minor pentatonic in C (menu 18), playing C.
    CHECK (quantize (7.0f, SnapSteps, 18, 0, 60) == 7.0f, "G is in C minor pentatonic");
    // Locrian (menu 7) in C has no G: steps +7 falls back to the nearest allowed step (0 or 12).
    const float loc = quantize (7.0f, SnapSteps, 7, 0, 60);
    CHECK (loc == 12.0f || loc == 0.0f, "locrian: +7 skipped (got %g)", loc);
    CHECK (scales::kNumScales == 28, "28 scales");
}

static void testFrequencies()
{
    Params p = quiet();
    p.mix[MixASaw] = 1.0f;
    auto v = renderNote (p, 69, 1.0);
    const double fa = risingCrossingsHz (v);
    CHECK (std::fabs (fa - 440.0) < 0.5, "Osc A at A4: %.3f Hz", fa);

    p = quiet();
    p.mix[MixBSquare] = 1.0f;
    v = renderNote (p, 69, 1.0);
    const double fb = risingCrossingsHz (v);
    CHECK (std::fabs (fb - 440.0) < 0.5, "Osc B at A4: %.3f Hz", fb);

    p.bSub = 1;
    v = renderNote (p, 69, 1.0);
    CHECK (std::fabs (risingCrossingsHz (v) - 220.0) < 0.5, "Sub -1: %.3f Hz", risingCrossingsHz (v));
    p.bSub = 2;
    v = renderNote (p, 69, 1.0);
    CHECK (std::fabs (risingCrossingsHz (v) - 110.0) < 0.5, "Sub -2: %.3f Hz", risingCrossingsHz (v));

    // Coarse +7 and Fine +0.5 on Osc A
    p = quiet();
    p.mix[MixASine] = 1.0f;
    p.a.coarse = 7.0f; p.a.fine = 0.5f;
    v = renderNote (p, 60, 1.0);
    CHECK (std::fabs (risingCrossingsHz (v) - hz (67.5)) < 0.5, "Coarse/Fine: %.3f Hz, expected %.3f", risingCrossingsHz (v), hz (67.5));

    // Keys = A only: Osc B stays on MIDI 48 whatever is played.
    p = quiet();
    p.keys = KeysA;
    p.mix[MixBSine] = 1.0f;
    v = renderNote (p, 72, 1.0);
    CHECK (std::fabs (risingCrossingsHz (v) - hz (48)) < 0.5, "Keys A: Osc B free at %.3f Hz", risingCrossingsHz (v));

    // LFO mode: 7 octaves down.
    p = quiet();
    p.aLfo = true;
    p.mix[MixASine] = 1.0f;
    v = renderNote (p, 69, 4.0, 1.0);
    const double lf = risingCrossingsHz (v);
    CHECK (std::fabs (lf - 440.0 / 128.0) < 0.05, "LFO: %.4f Hz", lf);

    // Pitch Snap Steps on Osc B: knob at +9 plays +7.
    p = quiet();
    p.snap = pitch::SnapSteps;
    p.b.coarse = 9.0f;
    p.mix[MixBSine] = 1.0f;
    v = renderNote (p, 60, 1.0);
    CHECK (std::fabs (risingCrossingsHz (v) - hz (67)) < 0.5, "Snap steps: %.3f Hz", risingCrossingsHz (v));
}

static void testAliasing()
{
    struct Case { const char* name; int slot; int note; double limitDb; };
    const Case cases[] = {
        { "A saw",        MixASaw,    100, -70.0 },
        { "A pulse",      MixAPwm,    100, -70.0 },
        { "A triangle",   MixATri,    100, -70.0 },
        { "B square",     MixBSquare, 100, -70.0 },
        { "B triangle",   MixBTri,    100, -70.0 },
        { "B staircase",  MixBPwm,    100, -70.0 },
        { "A saw high",   MixASaw,    112, -55.0 },
    };
    for (const auto& c : cases)
    {
        Params p = quiet();
        p.mix[c.slot] = 1.0f;
        p.a.pwm = 0.3f; p.b.pwm = 0.7f;
        auto v = renderNote (p, c.note, 0.75);
        const double db = aliasDb (v, hz (c.note));
        std::printf ("aliasing %-12s note %d: %.1f dB\n", c.name, c.note, db);
        CHECK (db < c.limitDb, "%s aliasing %.1f dB (limit %.1f)", c.name, db, c.limitDb);
    }

    // Hard sync: Osc A (higher) restarted by Osc B. The result repeats at Osc B's pitch, with little aliasing.
    Params p = quiet();
    p.sync = SyncHard;
    p.mix[MixASaw] = 1.0f;
    p.keys = KeysAB;
    p.a.coarse = 7.3f;
    auto v = renderNote (p, 81, 0.75);
    const double db = aliasDb (v, hz (81));
    std::printf ("aliasing hard sync saw (B at %.0f Hz): %.1f dB\n", hz (81), db);
    CHECK (db < -50.0, "hard sync aliasing %.1f dB", db);
    CHECK (std::fabs (risingCrossingsHz (v) - hz (81)) < 1.0 || db < -50.0, "hard sync period");
}

// Soft sync: a slightly flat Osc B is pulled into step with Osc A (or with a whole multiple of it).
static void testSoftSync()
{
    for (float detune : { -0.3f, 11.7f, 18.8f })
    {
        Params p = quiet();
        p.mix[MixBTri] = 1.0f;
        p.sync = SyncSoft;
        p.b.coarse = detune;
        auto v = renderNote (p, 57, 1.5, 1.0);            // Osc A at 220 Hz
        const int lag = (int) std::lround (kSr / 220.0 * 22.0);
        double err = 0.0, pw = 0.0;
        for (int i = 0; i < 20000; ++i) { const double d = v[(size_t) i] - v[(size_t) (i + lag)]; err += d * d; pw += (double) v[(size_t) i] * v[(size_t) i]; }
        const double db = 10.0 * std::log10 (err / pw + 1.0e-20);
        std::printf ("soft sync, Osc B %+.1f st: repeats with Osc A to %.1f dB\n", detune, db);
        CHECK (db < -60.0, "soft sync lock at %+.1f st: %.1f dB", detune, db);
    }
}

static void testEnvelopeAndDrone()
{
    Params p = quiet();
    p.mix[MixASine] = 1.0f;
    p.ampMode = AmpEnv;
    Engine e;
    e.setParams (p);
    e.prepare (kSr, kBlock);
    auto silent = render (e, p, 0.2);
    CHECK (peak (silent) < 1.0e-6, "Env mode, no note: silent (%g)", peak (silent));
    e.noteOn (60, 0.8f);
    auto on = render (e, p, 0.5);
    CHECK (rms (std::vector<float> (on.begin() + 12000, on.end())) > 0.05, "note sounds");
    e.noteOff (60);
    auto off = render (e, p, 1.0);
    CHECK (peak (std::vector<float> (off.begin() + 24000, off.end())) < 1.0e-3, "released to silence");

    p.ampMode = AmpDrone;
    Engine d;
    d.setParams (p);
    d.prepare (kSr, kBlock);
    auto drone = render (d, p, 0.5);
    CHECK (rms (std::vector<float> (drone.begin() + 12000, drone.end())) > 0.05, "Drone sounds without a note");

    // Legato: releasing the second of two held notes goes back to the first without silence.
    p.ampMode = AmpEnv;
    p.mix[MixASine] = 0.0f; p.mix[MixASaw] = 1.0f;
    Engine l;
    l.setParams (p);
    l.prepare (kSr, kBlock);
    l.noteOn (60, 1.0f);
    l.noteOn (67, 1.0f);
    render (l, p, 0.3);
    l.noteOff (67);
    auto back = render (l, p, 1.0);
    std::vector<float> tail (back.begin() + 12000, back.end());
    CHECK (std::fabs (risingCrossingsHz (tail) - hz (60)) < 1.0, "back to held note: %.2f Hz", risingCrossingsHz (tail));
}

static void testHeavyAutomation()
{
    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    Engine e;
    Params p = quiet();
    e.setParams (p);
    e.prepare (44100.0, 64);
    bool finite = true;
    double maxAbs = 0.0;
    std::vector<float> buf (64);
    for (int blk = 0; blk < 20000; ++blk)
    {
        if (blk % 3 == 0)
        {
            p.a.coarse = u (rng) * 96.0f - 48.0f;   p.b.coarse = u (rng) * 96.0f - 48.0f;
            p.a.fine = u (rng) * 2.0f - 1.0f;       p.b.fine = u (rng) * 2.0f - 1.0f;
            p.a.pwm = u (rng);                      p.b.pwm = u (rng);
            p.a.fm = u (rng);                       p.b.fm = u (rng);
            p.a.fmCurve = (int) (u (rng) * 2.99f);  p.b.fmCurve = (int) (u (rng) * 2.99f);
            p.aLfo = u (rng) < 0.2f;                p.bSub = (int) (u (rng) * 2.99f);
            p.sync = (int) (u (rng) * 2.99f);       p.keys = (int) (u (rng) * 2.99f);
            p.snap = (int) (u (rng) * 2.99f);       p.scale = (int) (u (rng) * 28.99f);
            p.key = (int) (u (rng) * 11.99f);       p.ampMode = (int) (u (rng) * 1.99f);
            for (auto& m : p.mix) m = u (rng);
            p.volume = u (rng);
            p.envAttack = u (rng); p.envDecay = u (rng); p.envSustain = u (rng); p.envRelease = u (rng);
        }
        if (blk % 7 == 0) e.noteOn ((int) (u (rng) * 127.0f), u (rng));
        if (blk % 11 == 0) e.noteOff ((int) (u (rng) * 127.0f));
        e.pitchBend (u (rng) * 4.0f - 2.0f);
        e.setParams (p);
        float* ch[1] = { buf.data() };
        e.process (ch, 1, 64);
        for (float s : buf)
        {
            if (! std::isfinite (s)) finite = false;
            maxAbs = std::max (maxAbs, (double) std::fabs (s));
        }
    }
    CHECK (finite, "all output finite under heavy automation");
    CHECK (maxAbs < 16.0, "output bounded under heavy automation (max %.2f)", maxAbs);
    std::printf ("heavy automation: max output %.2f\n", maxAbs);
}

static void testThroughZero()
{
    Params p = quiet();
    p.mix[MixASine] = 1.0f;
    p.a.fm = 1.0f;
    p.a.fmCurve = FmThroughZero;
    p.b.coarse = -12.0f;
    auto v = renderNote (p, 60, 0.5);
    CHECK (rms (v) > 0.05, "through-zero FM makes sound");
    bool finite = true;
    for (float s : v) finite = finite && std::isfinite (s);
    CHECK (finite, "through-zero output finite");
}

static void testCpu()
{
    Params p = quiet();
    for (auto& m : p.mix) m = 0.8f;
    p.a.fm = 0.6f; p.b.fm = 0.4f; p.sync = SyncHard; p.bSub = 1;
    Engine e;
    e.setParams (p);
    e.prepare (48000.0, 512);
    e.noteOn (48, 1.0f);
    std::vector<float> buf (512);
    const int blocks = (int) (10.0 * 48000.0 / 512.0);
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        float* ch[1] = { buf.data() };
        e.setParams (p);
        e.process (ch, 1, 512);
    }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("cpu: 10 s of audio in %.3f s (%.1f%% of one core)\n", secs, secs * 10.0);
}

int main()
{
    testPitchRules();
    testFrequencies();
    testAliasing();
    testSoftSync();
    testEnvelopeAndDrone();
    testHeavyAutomation();
    testThroughZero();
    testCpu();
    std::printf ("engine tests: %d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
