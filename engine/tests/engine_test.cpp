// Self-test for the COMPLICATOR engine. Run automatically by the build; the build fails if any check fails.

#include "complicator/Engine.h"
#include "complicator/Nonlinear.h"
#include "complicator/Pitch.h"
#include "complicator/Presets.h"
#include "complicator/Scales.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
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


// ================================================================================================ milestone 2

struct Opt
{
    int note = 60;
    float vel = 1.0f, wheel = 0.0f, touch = 0.0f;
    double skip = 0.25;
    const std::vector<float>* sidechain = nullptr;   // must cover skip + seconds
    bool noNote = false;
};

static std::vector<float> renderX (const Params& p, double seconds, const Opt& o = Opt())
{
    Engine e;
    e.setParams (p);
    e.modWheel (o.wheel);
    e.aftertouch (o.touch);
    e.prepare (kSr, kBlock);
    if (! o.noNote) e.noteOn (o.note, o.vel);
    const size_t total = (size_t) ((seconds + o.skip) * kSr);
    std::vector<float> out (total);
    for (size_t pos = 0; pos < total; pos += kBlock)
    {
        const int n = (int) std::min<size_t> (kBlock, total - pos);
        float* ch[1] = { out.data() + pos };
        e.setParams (p);
        e.process (ch, 1, n, o.sidechain ? o.sidechain->data() + pos : nullptr);
    }
    return std::vector<float> (out.begin() + (long) (o.skip * kSr), out.end());
}

static std::vector<float> sineWave (double freq, double amp, double seconds)
{
    std::vector<float> v ((size_t) (seconds * kSr));
    for (size_t i = 0; i < v.size(); ++i) v[i] = (float) (amp * std::sin (2.0 * 3.14159265358979323846 * freq * (double) i / kSr));
    return v;
}

// Amplitude (peak, of a sine) at exactly frequency f: a Blackman-Harris windowed DFT at that one frequency over 32768
// samples (no bin-straddling loss, unlike reading an FFT bin).
static double ampAt (const std::vector<float>& v, double f)
{
    const size_t n = 32768;
    const double w0 = 2.0 * 3.14159265358979323846 * f / kSr;
    double re = 0.0, im = 0.0, wsum = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double x = 2.0 * 3.14159265358979323846 * (double) i / (double) n;
        const double w = 0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2.0 * x) - 0.01168 * std::cos (3.0 * x);
        re += w * v[i] * std::cos (w0 * (double) i);
        im -= w * v[i] * std::sin (w0 * (double) i);
        wsum += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

// What is left (in dB, relative to the signal) after the best-fitting sine of frequency f is taken out.
static double sineFitResidualDb (const std::vector<float>& v, double f, size_t from, size_t to)
{
    const double w0 = 2.0 * 3.14159265358979323846 * f / kSr;
    double cc = 0, ss = 0, cs = 0, xc = 0, xs = 0, xx = 0;
    for (size_t i = from; i < to; ++i)
    {
        const double c = std::cos (w0 * (double) i), sn = std::sin (w0 * (double) i), x = v[i];
        cc += c * c; ss += sn * sn; cs += c * sn; xc += x * c; xs += x * sn; xx += x * x;
    }
    const double det = cc * ss - cs * cs;
    const double a = (xc * ss - xs * cs) / det, b = (xs * cc - xc * cs) / det;
    const double res = xx - a * xc - b * xs;
    return 10.0 * std::log10 (std::max (res, 1.0e-30) / xx);
}

static double dbOf (double ratio) { return 20.0 * std::log10 (ratio + 1.0e-12); }

// Share of the signal's power (20 Hz .. 20 kHz) that lies within +-6 bins of f0 itself: 1 = a pure sine.
static double fundamentalShare (const std::vector<float>& v, double f0)
{
    const size_t n = 32768;
    std::vector<std::complex<double>> a (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double x = 2.0 * 3.14159265358979323846 * (double) i / (double) n;
        const double w = 0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2.0 * x) - 0.01168 * std::cos (3.0 * x);
        a[i] = v[i] * w;
    }
    fft (a);
    const double bin = kSr / (double) n;
    double fund = 0.0, all = 0.0;
    for (size_t k = 1; k < n / 2; ++k)
    {
        const double f = (double) k * bin;
        if (f < 20.0 || f > 20000.0) continue;
        const double pw = std::norm (a[k]);
        all += pw;
        if (std::fabs (f - f0) <= 6.0 * bin) fund += pw;
    }
    return fund / (all + 1.0e-30);
}

static double maxStep (const std::vector<float>& v, size_t from, size_t to)
{
    double m = 0.0;
    for (size_t i = std::max<size_t> (from, 1); i < to && i < v.size(); ++i)
        m = std::max (m, (double) std::fabs (v[i] - v[i - 1]));
    return m;
}

static Params onlyMix (int slot, float level = 1.0f)
{
    Params p = quiet();
    p.mix[slot] = level;
    return p;
}

static void testNonlinearMaths()
{
    using namespace nl;
    CHECK (std::fabs (TriFold::f (0.5) - 0.5) < 1e-12 && std::fabs (TriFold::f (-0.9) + 0.9) < 1e-12, "fold: linear inside +-1");
    CHECK (std::fabs (TriFold::f (1.5) - 0.5) < 1e-12 && std::fabs (TriFold::f (2.0)) < 1e-12 && std::fabs (TriFold::f (3.0) + 1.0) < 1e-12,
           "fold: mirrored at +1 and +3");
    CHECK (std::fabs (TriFold::f (-1.5) + 0.5) < 1e-12 && std::fabs (TriFold::f (5.0) - 1.0) < 1e-12, "fold: negative side, period 4");
    // F' = f, and F is continuous, for every stage.
    auto derivOk = [] (auto fn, double lo, double hi, const char* name)
    {
        double worst = 0.0, jump = 0.0;
        for (double x = lo; x < hi; x += 0.0137)
        {
            const double h = 1.0e-6;
            worst = std::max (worst, std::fabs ((decltype (fn)::F (x + h) - decltype (fn)::F (x - h)) / (2.0 * h) - decltype (fn)::f (x)));
            jump = std::max (jump, std::fabs (decltype (fn)::F (x + 1.0e-9) - decltype (fn)::F (x - 1.0e-9)));
        }
        CHECK (worst < 2.0e-4, "%s: F' = f (worst %.2e)", name, worst);
        CHECK (jump < 1.0e-7, "%s: F continuous", name);
    };
    derivOk (TriFold(), -9.0, 9.0, "fold");
    derivOk (SoftClip(), -3.0, 3.0, "softclip");
    derivOk (HalfRect(), -3.0, 3.0, "halfrect");
    derivOk (AbsRect(), -3.0, 3.0, "absrect");
    CHECK (SoftClip::f (0.99) == 0.99 && std::fabs (SoftClip::f (1.5) - 1.25) < 1e-12 && SoftClip::f (3.0) == 1.25 && SoftClip::f (-3.0) == -1.25,
           "softclip: linear to 1.0, flat (1.25) from 1.5");
    // the slope of the soft clip is continuous (no corner): compare slopes either side of the knee and the top
    for (double x : { 1.0, 1.5 })
    {
        const double sl = (SoftClip::f (x - 1e-7) - SoftClip::f (x - 2e-7)) / 1e-7, sr2 = (SoftClip::f (x + 2e-7) - SoftClip::f (x + 1e-7)) / 1e-7;
        CHECK (std::fabs (sl - sr2) < 1e-3, "softclip: no corner at %.1f", x);
    }
    // ADAA of a slow signal equals the function itself.
    Adaa1<TriFold> ad;
    ad.reset (0.3);
    double err = 0.0;
    for (int i = 1; i < 200; ++i) { const double x = 0.3 + 0.0005 * i; err = std::max (err, std::fabs (ad.next (x) - TriFold::f (x - 0.00025))); }
    CHECK (err < 1e-5, "ADAA follows the function for slow input (%.2e)", err);
}

static void testFolder()
{
    const double f60 = hz (60);
    Params p = onlyMix (MixAFinal);
    p.a.fold = 0.5f;
    auto centre = renderNote (p, 60, 1.0);
    auto sine = renderNote (onlyMix (MixASine), 60, 1.0);
    const double ratio = rms (centre) / rms (sine);
    double thd = 0.0;
    for (int h = 2; h <= 9; ++h) thd += std::pow (ampAt (centre, f60 * h), 2.0);
    thd = dbOf (std::sqrt (thd) / ampAt (centre, f60));
    std::printf ("folder centre: level %.4f of the sine, harmonic distortion %.1f dB\n", ratio, thd);
    CHECK (std::fabs (dbOf (ratio)) < 0.2, "fader centre = plain sine (level %.3f)", ratio);
    CHECK (thd < -70.0, "fader centre: no distortion (%.1f dB)", thd);

    p.a.fold = 0.0f;
    auto left = renderNote (p, 60, 0.5);
    CHECK (peak (left) < 1.0e-4, "fader left = silence (peak %g)", peak (left));
    p.a.fold = 0.25f;
    auto quarter = renderNote (p, 60, 1.0);
    CHECK (std::fabs (rms (quarter) / rms (sine) - 0.25) < 0.01, "fader at 25%%: gain 0.25 (got %.3f)", rms (quarter) / rms (sine));

    p.a.fold = 1.0f;
    auto right = renderNote (p, 60, 1.0);
    int rich = 0;
    const double top = [&] { double m = 0.0; for (int h = 1; h <= 40; ++h) m = std::max (m, ampAt (right, f60 * h)); return m; }();
    for (int h = 1; h <= 40; ++h) if (dbOf (ampAt (right, f60 * h) / top) > -30.0) ++rich;
    std::printf ("folder right: %d harmonics within 30 dB of the strongest, peak %.3f\n", rich, peak (right));
    CHECK (rich >= 8, "fader right: rich (%d harmonics)", rich);
    CHECK (peak (right) <= 0.51, "fold output stays within +-1 (%.3f of 0.5)", peak (right));

    // Symmetry: a bias makes even harmonics; none at zero.
    p.a.fold = 0.5f; p.a.sym = 0.0f;
    const double h2zero = dbOf (ampAt (renderNote (p, 60, 1.0), f60 * 2) / ampAt (centre, f60));
    p.a.sym = 0.5f;
    auto biased = renderNote (p, 60, 1.0);
    const double h2bias = dbOf (ampAt (biased, f60 * 2) / ampAt (biased, f60));
    std::printf ("symmetry: 2nd harmonic %.1f dB at 0, %.1f dB at +50%%\n", h2zero, h2bias);
    CHECK (h2zero < -80.0, "no symmetry, no 2nd harmonic (%.1f dB)", h2zero);
    CHECK (h2bias > -25.0, "symmetry +50%% adds 2nd harmonic (%.1f dB)", h2bias);
    p.a.sym = -0.5f;
    auto biasedNeg = renderNote (p, 60, 1.0);
    CHECK (std::fabs (dbOf (ampAt (biasedNeg, f60 * 2)) - dbOf (ampAt (biased, f60 * 2))) < 1.0, "symmetry is mirrored for negative values");

    // Osc B has its own folder.
    Params pb = onlyMix (MixBFinal);
    pb.b.fold = 1.0f;
    auto rb = renderNote (pb, 60, 1.0);
    CHECK (rms (rb) > 0.05 && peak (rb) <= 0.51, "Osc B folder works (rms %.3f)", rms (rb));
    pb.b.fold = 0.5f;
    CHECK (std::fabs (dbOf (rms (renderNote (pb, 60, 1.0)) / rms (renderNote (onlyMix (MixBSine), 60, 1.0)))) < 0.2, "Osc B fader centre = sine");

    // Aliasing of the heavily folded sine, against a plain (naive) fold at the same oversampling.
    struct Case { int note; double limit; };
    for (const Case c : { Case { 60, -75.0 }, Case { 72, -65.0 }, Case { 84, -45.0 } })
    {
        Params q = onlyMix (MixAFinal);
        q.a.fold = 1.0f;
        const double f0 = hz (c.note);
        const double adaaDb = aliasDb (renderNote (q, c.note, 0.75), f0);

        Decimator dec;
        dec.prepare (kSr, 4);
        std::vector<float> naive;
        double ph = 0.0;
        const double inc = 2.0 * 3.14159265358979323846 * f0 / (kSr * 4.0);
        float blk[4];
        for (int i = 0; i < (int) (kSr * 1.0); ++i)
        {
            for (int j = 0; j < 4; ++j) { blk[j] = (float) (0.5 * nl::TriFold::f (10.0 * std::sin (ph))); ph += inc; }
            naive.push_back (dec.process (blk));
        }
        naive.erase (naive.begin(), naive.begin() + 12000);
        const double naiveDb = aliasDb (naive, f0);
        std::printf ("folder aliasing, fader right, note %d: %.1f dB with ADAA, %.1f dB naive (limit %.0f)\n", c.note, adaaDb, naiveDb, c.limit);
        CHECK (adaaDb < c.limit, "fold aliasing at note %d: %.1f dB (limit %.0f)", c.note, adaaDb, c.limit);
        CHECK (adaaDb < naiveDb - 6.0, "ADAA beats the naive fold by more than 6 dB at note %d (%.1f vs %.1f)", c.note, adaaDb, naiveDb);
    }

    // Fold CV: Velocity into Fold A CV pushes a centred fader to the right. The Wavefold CV amount scales it, negative inverts.
    Params q = onlyMix (MixAFinal);
    q.a.fold = 0.5f; q.a.foldCv = 1.0f;
    q.patch[InFoldA] = SrcVelocity;
    auto cvOn = renderNote (q, 60, 1.0);
    q.a.foldCv = 0.0f;
    auto cvZero = renderNote (q, 60, 1.0);
    q.a.foldCv = -1.0f;
    auto cvNeg = renderNote (q, 60, 1.0);          // 0.5 - 1.0 -> clamps to 0: silence
    CHECK (fundamentalShare (cvOn, f60) < 0.9 && peak (cvOn) > 0.4, "Fold CV (velocity) folds");
    CHECK (fundamentalShare (cvZero, f60) > 0.999, "CV amount 0: sine");
    CHECK (peak (cvNeg) < 1.0e-3, "negative CV amount pulls the fold to silence (%g)", peak (cvNeg));

    // The folder as an effect: the sidechain into Symmetry A, fader left: only the external signal reaches Final A.
    auto sc = sineWave (1000.0, 0.5, 1.5);
    Opt o; o.sidechain = &sc;
    Params e = onlyMix (MixAFinal);
    e.a.fold = 0.0f; e.a.sym = 1.0f;
    e.patch[InSymA] = SrcSidechain;
    auto ext = renderX (e, 1.0, o);
    std::printf ("folder as effect (sidechain 1 kHz into Symmetry A): 1 kHz level %.3f, own 261 Hz level %.5f\n",
                 ampAt (ext, 1000.0), ampAt (ext, f60));
    CHECK (ampAt (ext, 1000.0) > 0.1, "external signal appears at Final A");
    CHECK (ampAt (ext, f60) < 1.0e-3, "Osc A's own sine is gone with the fader left");
}

static void testMultiplier()
{
    // Osc A (the modulator, 440 Hz) and Osc B (the carrier, 261.63 Hz): the two sines are normalled in.
    Params p = onlyMix (MixMult);
    p.keys = KeysA;
    p.b.coarse = 12.0f;
    p.multDepth = 0.72f;
    const double fa = hz (69), fb = hz (60);
    Opt o; o.note = 69;

    p.multMode = 0;
    auto ring = renderX (p, 1.0, o);
    const double sum = ampAt (ring, fa + fb), diff = ampAt (ring, fa - fb);
    const double leakA = ampAt (ring, fa), leakB = ampAt (ring, fb);
    std::printf ("ring: sum %.3f diff %.3f, leak A %.5f leak B %.5f\n", sum, diff, leakA, leakB);
    CHECK (std::fabs (sum - 0.25) < 0.01 && std::fabs (diff - 0.25) < 0.01, "ring: only sum and difference (%.3f, %.3f)", sum, diff);
    CHECK (dbOf (leakA / sum) < -50.0 && dbOf (leakB / sum) < -50.0, "ring: inputs suppressed");

    p.multMode = 1;
    auto half = renderX (p, 1.0, o);
    const double halfCarrier = ampAt (half, fb), halfSum = ampAt (half, fa + fb);
    std::printf ("half-wave: carrier %.3f, sidebands %.3f\n", halfCarrier, halfSum);
    CHECK (halfCarrier > 0.8 * halfSum && halfSum > 0.1, "half-wave: carrier comes through, with sidebands at fA+-fB");

    p.multMode = 2;
    auto full = renderX (p, 1.0, o);
    const double fullCarrier = ampAt (full, fb), fullOdd = ampAt (full, fa + fb);
    std::printf ("full-wave: carrier %.3f, fA+fB %.5f, fA*2+fB %.3f\n", fullCarrier, fullOdd, ampAt (full, 2 * fa + fb));
    CHECK (fullCarrier > 0.25, "full-wave: strong carrier");
    CHECK (dbOf (fullOdd / fullCarrier) < -50.0, "full-wave: no fA+-fB (only even harmonics of A)");
    CHECK (ampAt (full, 2 * fa + fb) > 0.05, "full-wave: sidebands at 2fA+-fB");

    // Depth: the modulator is the sidechain sine, the carrier is Velocity (a constant 1).
    auto sc = sineWave (1000.0, 1.0, 1.5);
    Opt os; os.sidechain = &sc;
    Params d = onlyMix (MixMult);
    d.patch[InCarrier] = SrcVelocity;
    d.patch[InModulator] = SrcSidechain;
    auto thdOf = [&] (const std::vector<float>& v)
    {
        double t = 0.0;
        for (int h = 2; h <= 9; ++h) t += std::pow (ampAt (v, 1000.0 * h), 2.0);
        return dbOf (std::sqrt (t) / ampAt (v, 1000.0));
    };
    d.multDepth = 0.0f;
    CHECK (peak (renderX (d, 0.7, os)) < 1.0e-4, "depth 0: silent");
    d.multDepth = 0.36f;
    auto half36 = renderX (d, 0.7, os);
    d.multDepth = 0.72f;
    auto unity = renderX (d, 0.7, os);
    d.multDepth = 1.0f;
    auto hot = renderX (d, 0.7, os);
    const double pk36 = peak (half36) / 0.5, pk72 = peak (unity) / 0.5, pk100 = peak (hot) / 0.5;
    std::printf ("depth: peak %.3f at 36%%, %.3f at 72%%, %.3f at 100%%; distortion %.1f / %.1f / %.1f dB\n",
                 pk36, pk72, pk100, thdOf (half36), thdOf (unity), thdOf (hot));
    CHECK (std::fabs (pk36 - 0.5) < 0.02, "depth 36%%: half gain (%.3f)", pk36);
    CHECK (std::fabs (pk72 - 1.0) < 0.01, "depth 72%%: unity (%.3f)", pk72);
    CHECK (pk100 > pk72 + 0.1 && pk100 <= 1.27, "above unity: clips softly, ceiling 1.25 (%.3f)", pk100);
    CHECK (thdOf (half36) < -60.0 && thdOf (unity) < -60.0 && thdOf (hot) > -35.0, "clean up to unity, clipping only above");

    // The multiplier has no aliasing problem with its soft clip: heavily driven, a 1 kHz sine stays clean between harmonics.
    CHECK (aliasDb (hot, 1000.0) < -70.0, "soft clip aliasing %.1f dB", aliasDb (hot, 1000.0));

    // Carrier / Modulator patched to "Off" give silence.
    Params off = onlyMix (MixMult);
    off.patch[InCarrier] = SrcOff;
    CHECK (peak (renderNote (off, 69, 0.5)) < 1.0e-6, "carrier Off: silence");
}

static void testIndexRouting()
{
    const double fa = hz (60);
    // FM route: Osc A frequency-modulated by Osc B's sine, directly or through the Index.
    Params p = onlyMix (MixASine);
    p.keys = KeysA;
    p.b.coarse = 5.3f;
    p.a.fm = 1.0f; p.a.fmCurve = FmExp;
    p.routeFmA = false;
    auto direct = renderNote (p, 60, 1.0);
    p.routeFmA = true; p.index = 1.0f;
    auto viaFull = renderNote (p, 60, 1.0);
    p.index = 0.0f;
    auto viaZero = renderNote (p, 60, 1.0);
    p.index = 0.04f;
    auto viaHalf = renderNote (p, 60, 1.0);
    double dmax = 0.0;
    for (size_t i = 0; i < direct.size(); ++i) dmax = std::max (dmax, (double) std::fabs (direct[i] - viaFull[i]));
    std::printf ("index FM route: purity %.2f (direct), %.4f (index 0), %.2f (index 100%%), %.2f (4%%)\n",
                 fundamentalShare (direct, fa), fundamentalShare (viaZero, fa), fundamentalShare (viaFull, fa), fundamentalShare (viaHalf, fa));
    CHECK (fundamentalShare (direct, fa) < 0.5, "FM from B's sine modulates A");
    CHECK (fundamentalShare (viaZero, fa) > 0.9999, "FM route with Index 0: no modulation");
    CHECK (dmax < 1.0e-5, "FM route at Index 100%% equals the direct connection (%.2e)", dmax);
    CHECK (fundamentalShare (viaHalf, fa) > fundamentalShare (direct, fa) + 0.2, "Index 4%%: much less modulation");

    // Index CV: the Mod wheel into the Index input, the Index CV amount scaling it.
    Params c = p;
    c.index = 0.0f; c.indexCv = 1.0f; c.patch[InIndex] = SrcModWheel;
    Opt ow; ow.wheel = 0.5f;
    auto cvHalf = renderX (c, 1.0, ow);
    p.index = 0.5f;
    auto knobHalf = renderX (p, 1.0, ow);
    double dc = 0.0;
    for (size_t i = 0; i < cvHalf.size(); ++i) dc = std::max (dc, (double) std::fabs (cvHalf[i] - knobHalf[i]));
    CHECK (dc < 1.0e-4, "Index CV (wheel 0.5 x amount 1) = Index knob at 50%% (%.2e)", dc);
    c.indexCv = -1.0f; c.index = 0.5f;                              // 0.5 - 0.5 = 0
    auto cvNeg = renderX (c, 1.0, ow);
    CHECK (fundamentalShare (cvNeg, fa) > 0.9999, "negative Index CV amount pulls the Index down");

    // Fold route: A's folder gets B's sine (x Index x Wavefold CV amount) added to its fader.
    Params f = onlyMix (MixAFinal);
    f.keys = KeysA; f.b.coarse = 5.3f; f.a.fold = 0.5f; f.a.foldCv = 1.0f; f.index = 1.0f;
    f.routeFoldA = false;
    auto foldOff = renderNote (f, 60, 1.0);
    f.routeFoldA = true;
    auto foldOn = renderNote (f, 60, 1.0);
    std::printf ("fold route: purity %.4f off, %.2f on\n", fundamentalShare (foldOff, fa), fundamentalShare (foldOn, fa));
    CHECK (fundamentalShare (foldOff, fa) > 0.999, "fold route off: plain sine");
    CHECK (fundamentalShare (foldOn, fa) < 0.7, "fold route on: B's sine moves A's fold");

    // Symmetry route: B's sine replaces the DC reference.
    Params s = onlyMix (MixAFinal);
    s.keys = KeysA; s.b.coarse = 5.3f; s.a.fold = 0.5f; s.a.sym = 1.0f; s.index = 1.0f;
    const double fb = hz (48 + 5.3);
    auto symDc = renderNote (s, 60, 1.0);
    s.routeSymA = true;
    auto symB = renderNote (s, 60, 1.0);
    std::printf ("symmetry route: B's own pitch %.5f off, %.3f on\n", ampAt (symDc, fb), ampAt (symB, fb));
    CHECK (ampAt (symDc, fb) < 1.0e-4, "symmetry route off: nothing at Osc B's pitch");
    CHECK (ampAt (symB, fb) > 0.05, "symmetry route on: B's sine now goes through A's folder");
    s.index = 0.0f;
    auto symZero = renderNote (s, 60, 1.0);
    CHECK (fundamentalShare (symZero, fa) > 0.9999, "symmetry route at Index 0: no bias at all (the route replaces the reference)");

    // PWM routes. Off: A's pulse is perfectly periodic. Both: A's pulse width follows B's sine. A to B only: B's
    // staircase follows A's sine, A's pulse is still plain.
    Params w = quiet();
    w.keys = KeysA; w.b.coarse = 5.3f; w.index = 1.0f;
    w.mix[MixAPwm] = 1.0f; w.mix[MixBPwm] = 0.0f;
    auto pulseOf = [&] (int route) { Params q = w; q.routePwm = route; return renderNote (q, 60, 1.0); };
    const double pOff = fundamentalShare (pulseOf (0), fa), pBoth = fundamentalShare (pulseOf (1), fa), pAB = fundamentalShare (pulseOf (2), fa);
    // purity is a poor yardstick for a pulse (its harmonics are strong); look at how much of the spectrum is NOT harmonic of A
    auto offAl = aliasDb (pulseOf (0), fa), bothAl = aliasDb (pulseOf (1), fa), abAl = aliasDb (pulseOf (2), fa);
    (void) pOff; (void) pBoth; (void) pAB;
    std::printf ("PWM route, A pulse: non-harmonic energy %.1f dB (off), %.1f dB (both), %.1f dB (A to B only)\n", offAl, bothAl, abAl);
    CHECK (offAl < -70.0, "PWM route off: A's pulse is periodic");
    CHECK (bothAl > offAl + 25.0, "PWM route Both: B's sine moves A's pulse width");
    CHECK (abAl < -70.0, "PWM route A to B only: A's pulse untouched");

    Params wb = quiet();
    wb.keys = KeysA; wb.b.coarse = 5.3f; wb.index = 1.0f; wb.mix[MixBPwm] = 1.0f;
    auto stairOf = [&] (int route) { Params q = wb; q.routePwm = route; return aliasDb (renderNote (q, 60, 1.0), hz (48 + 5.3)); };
    const double sOff = stairOf (0), sBoth = stairOf (1), sAB = stairOf (2);
    std::printf ("PWM route, B staircase: non-harmonic energy %.1f dB (off), %.1f dB (both), %.1f dB (A to B only)\n", sOff, sBoth, sAB);
    CHECK (sOff < -70.0, "staircase off: periodic");
    CHECK (sBoth > sOff + 20.0 && sAB > sOff + 20.0, "A's sine moves B's steps (Both, and A to B only)");
}

static void testPatchSources()
{
    // Carrier patched to each source, Modulator = Osc A's sine (440 Hz); the multiplier output is that sine scaled by
    // the source. Drone amp: the amp envelope does not hide anything.
    Params base = onlyMix (MixMult);
    base.multDepth = 0.72f;
    base.keys = KeysA;
    const double fa = hz (69);
    auto level = [&] (Params p, int source, const Opt& o) { p.patch[InCarrier] = source; return ampAt (renderX (p, 0.7, o), fa) / 0.5; };

    Opt o; o.note = 69;
    CHECK (std::fabs (level (base, SrcVelocity, o) - 1.0) < 0.02, "Velocity 1 -> 1");
    Opt ov = o; ov.vel = 0.5f;
    CHECK (std::fabs (level (base, SrcVelocity, ov) - 0.5) < 0.02, "Velocity 0.5 -> 0.5 (%.3f)", level (base, SrcVelocity, ov));
    Opt ow = o; ow.wheel = 0.25f;
    CHECK (std::fabs (level (base, SrcModWheel, ow) - 0.25) < 0.02, "Mod wheel 0.25 -> 0.25 (%.3f)", level (base, SrcModWheel, ow));
    CHECK (level (base, SrcModWheel, o) < 1.0e-3, "Mod wheel 0 -> silence");
    Opt ot = o; ot.touch = 0.75f;
    CHECK (std::fabs (level (base, SrcAftertouch, ot) - 0.75) < 0.02, "Aftertouch 0.75 -> 0.75 (%.3f)", level (base, SrcAftertouch, ot));
    CHECK (level (base, SrcOff, o) < 1.0e-3, "Off -> silence");

    // Envelope: ADSR with a short attack and 60% sustain; no note -> nothing.
    Params env = base;
    env.envAttack = 0.0f; env.envDecay = 0.0f; env.envSustain = 0.6f;
    CHECK (std::fabs (level (env, SrcEnvelope, o) - 0.6) < 0.03, "Envelope at sustain 0.6 -> 0.6 (%.3f)", level (env, SrcEnvelope, o));
    Opt none = o; none.noNote = true;
    CHECK (level (env, SrcEnvelope, none) < 1.0e-3, "Envelope idle -> silence");

    // Sidechain through the interpolation: a 1 kHz sine of 0.5 into the carrier, Velocity (1) as the modulator.
    auto sc = sineWave (1000.0, 0.5, 1.5);
    Opt osc; osc.sidechain = &sc;
    Params sp = onlyMix (MixMult);
    sp.multDepth = 0.72f;
    sp.patch[InCarrier] = SrcSidechain; sp.patch[InModulator] = SrcVelocity;
    auto out = renderX (sp, 0.7, osc);
    const double resid = sineFitResidualDb (out, 1000.0, 4000, 24000);
    std::printf ("sidechain through the Patch row: level %.4f of 0.25, leftover after the 1 kHz sine is taken out %.1f dB\n",
                 ampAt (out, 1000.0) / 0.25, resid);
    CHECK (resid < -70.0, "sidechain arrives as a clean sine (leftover %.1f dB)", resid);
    CHECK (std::fabs (ampAt (out, 1000.0) / 0.25 - 1.0) < 0.01, "sidechain level kept (%.4f)", ampAt (out, 1000.0) / 0.25);

    // Every source into every continuous input: nothing blows up, and Off / Normal differ where they should.
    for (int src = 1; src < kNumSources; ++src)
        for (int in = 0; in < 11; ++in)
        {
            Params q = quiet();
            for (auto& m : q.mix) m = 0.6f;
            q.a.fm = 0.7f; q.b.fm = 0.7f; q.a.fold = 0.9f; q.b.fold = 0.9f; q.a.sym = 0.5f; q.b.sym = -0.5f; q.index = 0.8f;
            q.patch[in] = src;
            Opt oo; oo.sidechain = &sc; oo.wheel = 0.4f; oo.touch = 0.6f;
            auto v = renderX (q, 0.15, oo);
            bool finite = true;
            for (float x : v) finite = finite && std::isfinite (x);
            if (! finite || peak (v) > 12.0) { CHECK (false, "source %d into input %d: finite and bounded (peak %.2f)", src, in, peak (v)); }
        }
    CHECK (true, "all 17 sources into all 11 inputs: finite and bounded");
}

static void testPatchSync()
{
    // Hard sync from the Patch row: Osc A restarted by Osc B's square, with the Sync switch Off.
    Params p = quiet();
    p.mix[MixASaw] = 1.0f;
    p.a.coarse = 7.3f;
    p.patch[InHardSync] = SrcBSquare;
    auto v = renderNote (p, 81, 0.75);
    const double db = aliasDb (v, hz (81));
    std::printf ("patched hard sync (B square -> A): non-harmonic energy %.1f dB\n", db);
    CHECK (db < -70.0, "patched hard sync repeats at Osc B's pitch (%.1f dB)", db);
    // ... which is exactly what the Sync switch does:
    Params sw = p;
    sw.patch[InHardSync] = SrcNormal; sw.sync = SyncHard;
    auto viaSwitch = renderNote (sw, 81, 0.75);
    double same = 0.0;
    for (size_t i = 0; i < v.size(); ++i) same = std::max (same, (double) std::fabs (v[i] - viaSwitch[i]));
    CHECK (same < 1.0e-6, "B Square into Hard sync = the Sync switch's Hard (%.2e)", same);
    p.patch[InHardSync] = SrcOff;
    auto free = renderNote (p, 81, 0.75);
    CHECK (aliasDb (free, hz (81)) > -30.0, "hard sync input Off: no sync");

    // Any other source is found by a threshold: Osc B's sine rising through 2.5 V restarts Osc A.
    p.patch[InHardSync] = SrcBSine;
    auto th = renderNote (p, 81, 0.75);
    std::printf ("hard sync by threshold (B sine -> A): non-harmonic energy %.1f dB\n", aliasDb (th, hz (81)));
    CHECK (aliasDb (th, hz (81)) < -40.0, "hard sync by threshold repeats at Osc B's pitch (%.1f dB)", aliasDb (th, hz (81)));

    // Soft sync from the Patch row: Osc B locks to Osc A's sawtooth (Sync switch Off).
    for (float detune : { -0.3f, 11.7f })
    {
        Params q = quiet();
        q.mix[MixBTri] = 1.0f;
        q.b.coarse = detune;
        q.patch[InSoftSync] = SrcASaw;
        auto w = renderNote (q, 57, 1.5, 1.0);
        const int lag = (int) std::lround (kSr / 220.0 * 22.0);
        double err = 0.0, pw = 0.0;
        for (int i = 0; i < 20000; ++i) { const double d = w[(size_t) i] - w[(size_t) (i + lag)]; err += d * d; pw += (double) w[(size_t) i] * w[(size_t) i]; }
        const double sdb = 10.0 * std::log10 (err / pw + 1.0e-20);
        std::printf ("patched soft sync, Osc B %+.1f st: repeats with Osc A to %.1f dB\n", detune, sdb);
        CHECK (sdb < -50.0, "patched soft sync locks at %+.1f st (%.1f dB)", detune, sdb);
    }

    // Soft sync by threshold (any other source): Osc A's triangle rising through 2 V.
    {
        Params q = quiet();
        q.mix[MixBTri] = 1.0f;
        q.b.coarse = -0.3f;
        q.patch[InSoftSync] = SrcATri;
        auto w = renderNote (q, 57, 1.5, 1.0);
        const int lag = (int) std::lround (kSr / 220.0 * 22.0);
        double err = 0.0, pw = 0.0;
        for (int i = 0; i < 20000; ++i) { const double d = w[(size_t) i] - w[(size_t) (i + lag)]; err += d * d; pw += (double) w[(size_t) i] * w[(size_t) i]; }
        const double sdb = 10.0 * std::log10 (err / pw + 1.0e-20);
        std::printf ("soft sync by threshold (A tri -> B): repeats with Osc A to %.1f dB\n", sdb);
        CHECK (sdb < -40.0, "soft sync by threshold locks (%.1f dB)", sdb);
    }

    // A patched sync input replaces the Sync switch's connection.
    Params r = quiet();
    r.mix[MixASaw] = 1.0f; r.a.coarse = 7.3f;
    r.sync = SyncHard;
    r.patch[InHardSync] = SrcOff;
    CHECK (aliasDb (renderNote (r, 81, 0.75), hz (81)) > -30.0, "patching Off breaks the Sync switch's hard sync");
    r.patch[InHardSync] = SrcNormal;
    CHECK (aliasDb (renderNote (r, 81, 0.75), hz (81)) < -50.0, "Normal = the Sync switch's hard sync");
}

static void testFeedback()
{
    // Loops through the Patch row run on the previous sample. They must stay finite, bounded, repeatable, and sound.
    struct Loop { const char* name; int input; int source; };
    const Loop loops[] = {
        { "A Final -> FM A",        InFmA,   SrcAFinal },
        { "B Final -> FM A",        InFmA,   SrcBFinal },
        { "Multiply -> FM B",       InFmB,   SrcMultiply },
        { "A Final -> Symmetry A",  InSymA,  SrcAFinal },
        { "Multiply -> Fold B CV",  InFoldB, SrcMultiply },
        { "B Final -> Modulator",   InModulator, SrcBFinal },
        { "Multiply -> Carrier",    InCarrier,   SrcMultiply },
        { "Multiply -> PWM A CV",   InPwmA,  SrcMultiply },
        { "A Final -> Index CV",    InIndex, SrcAFinal },
    };
    for (const auto& l : loops)
    {
        Params p = quiet();
        for (auto& m : p.mix) m = 0.7f;
        p.a.fm = 0.8f; p.b.fm = 0.8f; p.a.fold = 0.85f; p.b.fold = 0.85f; p.a.foldCv = 1.0f; p.b.foldCv = 1.0f;
        p.a.sym = 0.6f; p.index = 0.7f; p.indexCv = 1.0f; p.multDepth = 1.0f;
        p.patch[l.input] = l.source;
        auto v1 = renderNote (p, 57, 0.4);
        auto v2 = renderNote (p, 57, 0.4);
        bool finite = true, same = v1.size() == v2.size();
        for (size_t i = 0; same && i < v1.size(); ++i) { finite = finite && std::isfinite (v1[i]); same = v1[i] == v2[i]; }
        CHECK (finite && peak (v1) < 12.0, "loop %s: bounded (peak %.2f)", l.name, peak (v1));
        CHECK (same, "loop %s: repeatable", l.name);
        CHECK (rms (v1) > 0.01, "loop %s: makes sound", l.name);
    }
    // A self-FM loop is a different sound from no loop.
    Params q = onlyMix (MixASine);
    q.keys = KeysA; q.a.fm = 0.6f; q.a.fmCurve = FmExp; q.a.fold = 0.7f;
    auto open = renderNote (q, 60, 0.75);
    q.patch[InFmA] = SrcAFinal;
    auto closed = renderNote (q, 60, 0.75);
    double dd = 0.0;
    for (size_t i = 0; i < open.size(); ++i) dd += std::pow ((double) open[i] - closed[i], 2.0);
    const double rel = std::sqrt (dd / (double) open.size()) / rms (open);
    std::printf ("self-FM loop (A Final -> FM A): difference from the open patch %.2f of its level\n", rel);
    CHECK (rel > 0.3, "A Final -> FM A changes the sound (%.2f)", rel);
}

// A change of Patch menu, Index route, multiplier mode or fader position in the middle of a note must not click.
static void testSwitching()
{
    struct Change { const char* name; std::function<void (Params&)> apply; Params start; };
    auto base = [] { Params p = quiet(); p.keys = KeysA; p.a.coarse = 0.0f; p.b.coarse = 7.0f; p.index = 0.8f; return p; };

    std::vector<Change> changes;
    { Params p = base(); p.mix[MixMult] = 1.0f; changes.push_back ({ "Carrier: Normal -> A Sine", [] (Params& q) { q.patch[InCarrier] = SrcASine; }, p }); }
    { Params p = base(); p.mix[MixMult] = 1.0f; changes.push_back ({ "Modulator: Normal -> Off", [] (Params& q) { q.patch[InModulator] = SrcOff; }, p }); }
    { Params p = base(); p.mix[MixMult] = 1.0f; changes.push_back ({ "Multiply mode: Ring -> Full-wave", [] (Params& q) { q.multMode = 2; }, p }); }
    { Params p = base(); p.mix[MixMult] = 1.0f; p.multMode = 1; changes.push_back ({ "Multiply mode: Half-wave -> Ring", [] (Params& q) { q.multMode = 0; }, p }); }
    { Params p = base(); p.mix[MixAFinal] = 1.0f; p.a.foldCv = 1.0f; changes.push_back ({ "Index route: Fold A on", [] (Params& q) { q.routeFoldA = true; }, p }); }
    { Params p = base(); p.mix[MixAFinal] = 1.0f; p.a.sym = 0.8f; changes.push_back ({ "Index route: Symmetry A on", [] (Params& q) { q.routeSymA = true; }, p }); }
    { Params p = base(); p.mix[MixASine] = 1.0f; p.a.fm = 0.6f; changes.push_back ({ "Index route: FM A on", [] (Params& q) { q.routeFmA = true; }, p }); }
    { Params p = base(); p.mix[MixAPwm] = 1.0f; changes.push_back ({ "PWM route: Both", [] (Params& q) { q.routePwm = 1; }, p }); }
    { Params p = base(); p.mix[MixAFinal] = 1.0f; changes.push_back ({ "Wavefold fader 0.1 -> 0.9", [] (Params& q) { q.a.fold = 0.9f; }, p }); }
    { Params p = base(); p.mix[MixAFinal] = 1.0f; changes.push_back ({ "Symmetry 0 -> 1", [] (Params& q) { q.a.sym = 1.0f; }, p }); }
    { Params p = base(); p.mix[MixMult] = 1.0f; changes.push_back ({ "Depth 0.2 -> 1", [] (Params& q) { q.multDepth = 1.0f; }, p }); }
    changes[8].start.a.fold = 0.1f;
    changes[10].start.multDepth = 0.2f;

    for (auto& c : changes)
    {
        Engine e;
        Params p = c.start;
        e.setParams (p);
        e.prepare (kSr, kBlock);
        e.noteOn (57, 1.0f);
        std::vector<float> out;
        auto run = [&] (double sec) { auto v = render (e, p, sec); out.insert (out.end(), v.begin(), v.end()); };
        run (0.5);
        const size_t at = out.size();
        c.apply (p);
        run (0.6);
        const size_t w = (size_t) (0.03 * kSr);
        const double transition = maxStep (out, at, at + w);
        const double steady = std::max (maxStep (out, at - (size_t) (0.3 * kSr), at), maxStep (out, at + (size_t) (0.25 * kSr), out.size()));
        const double ratio = transition / (steady + 1.0e-9);
        std::printf ("switching: %-36s biggest step %.4f during the change, %.4f in the steady sounds (x%.2f)\n", c.name, transition, steady, ratio);
        CHECK (ratio < 1.5, "no click when: %s (x%.2f)", c.name, ratio);
    }
}

static void testHeavyAutomationM2()
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        std::mt19937 rng (4321 + (unsigned) rate);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        Engine e;
        Params p = quiet();
        e.setParams (p);
        e.prepare (rate, 64);
        bool finite = true;
        double maxAbs = 0.0;
        std::vector<float> buf (64), sc (64);
        for (int blk = 0; blk < 12000; ++blk)
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
                p.a.fold = u (rng);     p.b.fold = u (rng);
                p.a.foldCv = u (rng) * 2.0f - 1.0f;  p.b.foldCv = u (rng) * 2.0f - 1.0f;
                p.a.sym = u (rng) * 2.0f - 1.0f;     p.b.sym = u (rng) * 2.0f - 1.0f;
                p.multDepth = u (rng);   p.multMode = (int) (u (rng) * 2.99f);
                p.index = u (rng);       p.indexCv = u (rng) * 2.0f - 1.0f;
                p.routeFmA = u (rng) < 0.5f; p.routeFmB = u (rng) < 0.5f; p.routeFoldA = u (rng) < 0.5f;
                p.routeFoldB = u (rng) < 0.5f; p.routeSymA = u (rng) < 0.5f; p.routeSymB = u (rng) < 0.5f;
                p.routePwm = (int) (u (rng) * 2.99f);
                for (int k = 0; k < kNumInputs; ++k) p.patch[k] = (int) (u (rng) * 17.99f);
            }
            if (blk % 5 == 0)                                   // single-block jumps of the fold, depth and index
            { p.a.fold = u (rng); p.b.fold = u (rng); p.multDepth = u (rng); p.index = u (rng); }
            if (blk % 7 == 0) e.noteOn ((int) (u (rng) * 127.0f), u (rng));
            if (blk % 11 == 0) e.noteOff ((int) (u (rng) * 127.0f));
            if (blk % 13 == 0) e.modWheel (u (rng));
            if (blk % 17 == 0) e.aftertouch (u (rng));
            e.pitchBend (u (rng) * 4.0f - 2.0f);
            for (float& x : sc) x = (u (rng) - 0.5f) * (blk % 19 == 0 ? 8.0f : 1.0f);    // loud noise now and then
            e.setParams (p);
            float* ch[1] = { buf.data() };
            e.process (ch, 1, 64, blk % 23 == 0 ? nullptr : sc.data());
            for (float s : buf)
            {
                if (! std::isfinite (s)) finite = false;
                maxAbs = std::max (maxAbs, (double) std::fabs (s));
            }
        }
        CHECK (finite, "heavy automation (all controls, %.0f Hz): finite", rate);
        CHECK (maxAbs < 16.0, "heavy automation (all controls, %.0f Hz): bounded (max %.2f)", rate, maxAbs);
        std::printf ("heavy automation, everything, %.0f Hz: max output %.2f\n", rate, maxAbs);
    }
}

static double cpuPercent (const Params& p, bool scopeOn, int seconds = 10)
{
    Engine e;
    e.setParams (p);
    e.prepare (48000.0, 512);
    e.setScopeActive (scopeOn);
    e.noteOn (48, 1.0f);
    std::vector<float> buf (512), sc (512, 0.1f);
    const int blocks = (int) ((double) seconds * 48000.0 / 512.0);
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        float* ch[1] = { buf.data() };
        e.setParams (p);
        e.process (ch, 1, 512, sc.data());
    }
    return std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() * 100.0 / (double) seconds;
}

static void testCpuM2()
{
    Params plain = quiet();
    plain.mix[MixASaw] = 0.8f; plain.mix[MixBSquare] = 0.8f;
    plain.a.fm = 0.5f; plain.b.fm = 0.5f;

    Params folds = plain;
    folds.mix[MixAFinal] = 0.8f; folds.mix[MixBFinal] = 0.8f; folds.a.fold = 0.9f; folds.b.fold = 0.9f;

    Params full = quiet();
    for (auto& m : full.mix) m = 0.8f;
    full.a.fm = 0.6f; full.b.fm = 0.4f; full.sync = SyncHard; full.bSub = 1;
    full.a.fold = 0.9f; full.b.fold = 0.9f; full.a.sym = 0.4f; full.index = 0.7f; full.multDepth = 1.0f; full.multMode = 1;
    full.routeFmA = full.routeFoldA = full.routeSymB = true; full.routePwm = 1;

    Params loops = full;
    loops.patch[InFmA] = SrcAFinal; loops.patch[InFoldB] = SrcMultiply; loops.patch[InModulator] = SrcBFinal;
    loops.patch[InSoftSync] = SrcASaw; loops.patch[InHardSync] = SrcBSquare; loops.patch[InSymA] = SrcSidechain;

    const double c1 = cpuPercent (plain, false), c2 = cpuPercent (folds, false), c3 = cpuPercent (full, true), c4 = cpuPercent (loops, true);
    const double c5 = cpuPercent (plain, true);
    std::printf ("cpu (%% of one core, 48 kHz, 4x): oscillators only %.1f, plus both folders %.1f, everything %.1f, everything with loops %.1f, panel open (scopes on) with only oscillators mixed %.1f\n",
                 c1, c2, c3, c4, c5);
    CHECK (c2 < 12.0, "cpu: oscillators + both folders %.1f%%", c2);
    CHECK (c3 < 15.0, "cpu: everything on %.1f%%", c3);
    CHECK (c4 < 15.0, "cpu: everything on, with loops %.1f%%", c4);
    CHECK (c1 < c3, "cpu: unused stages are skipped");
}

// ================================================================================================ milestone 3

// Render at any sample rate (the tests above use kSr). 'quality' is Params::quality; the note is held the whole time.
static std::vector<float> renderAt (Params p, double rate, int note, double seconds, double skip,
                                    const std::vector<float>* sidechain = nullptr)
{
    Engine e;
    e.setParams (p);
    e.prepare (rate, kBlock);
    e.noteOn (note, 1.0f);
    const size_t total = (size_t) ((seconds + skip) * rate);
    std::vector<float> out (total);
    for (size_t pos = 0; pos < total; pos += kBlock)
    {
        const int n = (int) std::min<size_t> (kBlock, total - pos);
        float* ch[1] = { out.data() + pos };
        e.setParams (p);
        e.process (ch, 1, n, sidechain != nullptr ? sidechain->data() + pos : nullptr);
    }
    return std::vector<float> (out.begin() + (long) (skip * rate), out.end());
}

static double aliasDbQ (Params p, int quality, int slot, int note)
{
    p.quality = quality;
    p.mix[slot] = 1.0f;
    p.a.pwm = 0.3f; p.b.pwm = 0.7f;
    return aliasDb (renderAt (p, kSr, note, 0.75, 0.25), hz (note));
}

static void testQualityMenu()
{
    CHECK (Engine::factorFor (0) == 2 && Engine::factorFor (1) == 4 && Engine::factorFor (2) == 8, "quality to factor");
    CHECK (Engine::factorFor (-1) == 4 && Engine::factorFor (7) == 4, "odd quality values read as 4x");
    CHECK (Params().quality == 1, "4x is the default");

    // The engine starts at the quality it is given, and reports it.
    for (int q = 0; q < 3; ++q)
    {
        Params p = quiet();
        p.quality = q;
        Engine e;
        e.setParams (p);
        e.prepare (kSr, kBlock);
        CHECK (e.oversamplingFactor() == (q == 0 ? 2 : q == 1 ? 4 : 8), "prepare at quality %d: factor %d", q, e.oversamplingFactor());
    }

    // Latency is the same (within a sample) at every quality, so changing it does not move the sound in time.
    {
        double lat[3];
        for (int q = 0; q < 3; ++q)
        {
            Params p = quiet(); p.quality = q;
            Engine e; e.setParams (p); e.prepare (kSr, kBlock);
            lat[q] = e.latencySamples();
        }
        std::printf ("latency (samples at 48 kHz): 2x %.2f, 4x %.2f, 8x %.2f\n", lat[0], lat[1], lat[2]);
        CHECK (std::fabs (lat[0] - lat[1]) < 1.0 && std::fabs (lat[2] - lat[1]) < 1.0, "latency differs between qualities");
    }

    // Same pitch, same loudness, less aliasing with each step.
    {
        double al[3][5];
        const int slots[5] = { MixASaw, MixAPwm, MixBSquare, MixBTri, MixAFinal };
        const int notes[5] = { 100, 100, 100, 100, 96 };
        for (int q = 0; q < 3; ++q)
            for (int k = 0; k < 5; ++k)
            {
                Params p = quiet();
                if (slots[k] == MixAFinal) p.a.fold = 1.0f;
                al[q][k] = aliasDbQ (p, q, slots[k], notes[k]);
            }
        std::printf ("aliasing by quality (A saw / A pulse / B square / B tri note 100, folded sine note 96), dB:\n"
                     "   2x: %.1f %.1f %.1f %.1f %.1f\n   4x: %.1f %.1f %.1f %.1f %.1f\n   8x: %.1f %.1f %.1f %.1f %.1f\n",
                     al[0][0], al[0][1], al[0][2], al[0][3], al[0][4], al[1][0], al[1][1], al[1][2], al[1][3], al[1][4],
                     al[2][0], al[2][1], al[2][2], al[2][3], al[2][4]);
        for (int k = 0; k < 5; ++k)
        {
            const bool fold = k == 4;
            CHECK (al[0][k] < (fold ? -30.0 : -45.0), "2x aliasing %.1f dB (output %d)", al[0][k], k);
            CHECK (al[1][k] < (fold ? -50.0 : -65.0), "4x aliasing %.1f dB (output %d)", al[1][k], k);
            CHECK (al[2][k] < (fold ? -65.0 : -80.0), "8x aliasing %.1f dB (output %d)", al[2][k], k);
            if (k != 3)    // the triangle is already near the floor of this measurement at 2x, so it is exempt from the step check
                CHECK (al[1][k] < al[0][k] - 8.0 && al[2][k] < al[1][k] - 8.0, "each quality step is at least 8 dB cleaner (output %d)", k);
        }

        double level[3], freq[3];
        for (int q = 0; q < 3; ++q)
        {
            Params p = quiet(); p.quality = q;
            p.mix[MixASine] = 0.8f; p.mix[MixBTri] = 0.6f; p.mix[MixBSquare] = 0.5f;
            p.a.coarse = 7.0f;
            auto v = renderAt (p, kSr, 57, 1.0, 0.25);
            level[q] = rms (v);
            Params s = quiet(); s.quality = q; s.mix[MixASine] = 1.0f;
            freq[q] = risingCrossingsHz (renderAt (s, kSr, 60, 1.0, 0.25));
        }
        for (int q : { 0, 2 })
        {
            CHECK (std::fabs (20.0 * std::log10 (level[q] / level[1])) < 0.5, "level at quality %d differs from 4x by %.2f dB", q, 20.0 * std::log10 (level[q] / level[1]));
            CHECK (std::fabs (freq[q] - hz (60)) < 0.5, "pitch at quality %d: %.2f Hz", q, freq[q]);
        }
    }

    // Changing the quality while playing: a short fade out and in, no click, and the sound comes back at the same level.
    for (int variant = 0; variant < 2; ++variant)
    {
        Params p = quiet();
        if (variant == 0) { p.mix[MixASaw] = 0.8f; p.mix[MixBTri] = 0.6f; p.mix[MixAFinal] = 0.7f; p.a.fold = 0.8f; }
        else              { p = presets::factory (2); p.ampMode = AmpDrone; }
        Engine ref, e;
        ref.setParams (p); ref.prepare (kSr, kBlock); ref.noteOn (57, 1.0f);
        e.setParams (p);   e.prepare (kSr, kBlock);   e.noteOn (57, 1.0f);
        const int sequence[] = { 1, 0, 2, 1, 2, 0, 0, 2, 1 };
        std::vector<float> a, b;
        auto step = [&] (int blocks, int q)
        {
            Params pq = p; pq.quality = q;
            for (int i = 0; i < blocks; ++i)
            {
                float x[kBlock], y[kBlock]; float* cx[1] = { x }; float* cy[1] = { y };
                ref.setParams (p); ref.process (cx, 1, kBlock);
                e.setParams (pq);  e.process (cy, 1, kBlock);
                a.insert (a.end(), x, x + kBlock); b.insert (b.end(), y, y + kBlock);
            }
        };
        step (40, 1);
        int last = 1;
        for (int q : sequence) { step (30, q); last = q; }
        step (60, 1);
        CHECK (e.oversamplingFactor() == 4, "back at 4x (%d)", e.oversamplingFactor());
        (void) last;
        double biggest = 0.0, steady = 0.0, mx = 0.0;
        for (size_t i = 2000; i < b.size(); ++i) { biggest = std::max (biggest, (double) std::fabs (b[i] - b[i - 1])); steady = std::max (steady, (double) std::fabs (a[i] - a[i - 1])); mx = std::max (mx, (double) std::fabs (b[i])); }
        std::printf ("quality changes while playing (variant %d): biggest step %.4f, steady %.4f\n", variant, biggest, steady);
        CHECK (std::isfinite (mx) && mx < 4.0, "quality changes: bounded (%g)", mx);
        CHECK (biggest < 1.5 * steady + 0.005, "quality changes click: step %.4f vs steady %.4f", biggest, steady);
        const size_t n = a.size();
        std::vector<float> ta (a.end() - 4000, a.end()), tb (b.end() - 4000, b.end());
        CHECK (std::fabs (20.0 * std::log10 (rms (tb) / rms (ta))) < 0.5, "level after quality changes %.2f dB off", 20.0 * std::log10 (rms (tb) / rms (ta)));
        (void) n;
        // while a change is in progress the sound is only ever quieter, never louder, than the steady sound by much
        double fadeMax = 0.0;
        for (size_t i = 3000; i < b.size() - 4000; ++i) fadeMax = std::max (fadeMax, (double) std::fabs (b[i]));
        CHECK (fadeMax < 1.6 * peak (a), "quality changes: peak %.3f vs steady peak %.3f", fadeMax, peak (a));
    }

    // Heavy random automation, all controls, quality included (the heavy-automation tests above run at 4x).
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        std::mt19937 g (31u + (unsigned) rate);
        auto U = [&] (float a, float b) { return std::uniform_real_distribution<float> (a, b) (g); };
        auto I = [&] (int a, int b) { return std::uniform_int_distribution<int> (a, b) (g); };
        Engine e;
        Params p = quiet();
        e.setParams (p);
        e.prepare (rate, kBlock);
        e.noteOn (48, 1.0f);
        bool finite = true;
        double maxAbs = 0.0;
        for (int b = 0; b < 700; ++b)
        {
            if (b % 3 == 0)
            {
                for (auto* o : { &p.a, &p.b }) { o->coarse = U (-30, 30); o->fine = U (-1, 1); o->pwm = U (0, 1); o->fm = U (0, 1); o->fmCurve = I (0, 2); o->fold = U (0, 1); o->foldCv = U (-1, 1); o->sym = U (-1, 1); }
                p.sync = I (0, 2); p.bSub = I (0, 2); p.aLfo = I (0, 6) == 0; p.keys = I (0, 2);
                p.multDepth = U (0, 1); p.multMode = I (0, 2); p.index = U (0, 1); p.indexCv = U (-1, 1);
                p.routeFmA = I (0, 1); p.routeFoldB = I (0, 1); p.routeSymA = I (0, 1); p.routePwm = I (0, 2);
                for (int k = 0; k < kNumInputs; ++k) p.patch[k] = I (0, 2) == 0 ? I (0, kNumSources - 1) : 0;
                for (auto& m : p.mix) m = U (0, 1);
            }
            if (b % 5 == 0) p.quality = I (0, 2);             // changes faster than the fade completes, too
            if (b % 41 == 0) e.noteOn (36 + I (0, 48), 0.8f);
            float buf[kBlock]; float* ch[1] = { buf };
            e.setParams (p);
            e.process (ch, 1, kBlock);
            for (float x : buf) { if (! std::isfinite (x)) finite = false; maxAbs = std::max (maxAbs, (double) std::fabs (x)); }
        }
        CHECK (finite, "heavy automation incl. quality (%.0f Hz): finite", rate);
        CHECK (maxAbs < 16.0, "heavy automation incl. quality (%.0f Hz): bounded (max %.2f)", rate, maxAbs);
        std::printf ("heavy automation incl. Quality, %.0f Hz: max output %.2f\n", rate, maxAbs);
    }
}

// Soft sync at simple ratios. A is at 220 Hz; the engine runs at 44 kHz so that one period of A is exactly 200 samples.
// Osc B, tuned a little flat of a ratio, must settle into the pattern that ratio really has: it repeats after
// 'denominator' periods of A (2 for a fifth: 3 cycles of B in 2 of A), not after twice that.
static void testSoftSyncRatios()
{
    struct Ratio { const char* name; double r; int period; };
    const Ratio ratios[] = { { "fifth 3:2", 1.5, 2 }, { "fourth 4:3", 4.0 / 3.0, 3 }, { "major third 5:4", 1.25, 4 },
                             { "sixth 5:3", 5.0 / 3.0, 3 }, { "octave 2:1", 2.0, 1 }, { "unison", 1.0, 1 },
                             { "octave down 1:2", 0.5, 2 } };
    for (const auto& r : ratios)
    {
        for (int note : { 57, 45 })           // A at 220 Hz and 110 Hz
        {
            const int per = note == 57 ? 200 : 400;
            for (double cents : { -60.0, -25.0, -10.0, -2.0 })
            {
                Params p = quiet();
                p.mix[MixBTri] = 1.0f;
                p.sync = SyncSoft;
                p.b.coarse = (float) (12.0 * std::log2 (r.r) + cents / 100.0);
                auto v = renderAt (p, 44000.0, note, 1.0, cents > -5.0 ? 10.0 : 3.0);   // a tiny detune takes longer to settle
                auto dbAtLag = [&] (int periods)
                {
                    const int lag = per * periods;
                    double err = 0.0, pw = 0.0;
                    for (int i = 0; i < 20000; ++i) { const double d = v[(size_t) i] - v[(size_t) (i + lag)]; err += d * d; pw += (double) v[(size_t) i] * v[(size_t) i]; }
                    return 10.0 * std::log10 (err / pw + 1.0e-20);
                };
                const double natural = dbAtLag (r.period);
                CHECK (natural < -60.0, "soft sync %s at %.0f Hz, %+.0f cents: repeats after %d periods only to %.1f dB", r.name, hz (note), cents, r.period, natural);
                if (cents == -10.0 && note == 57) std::printf ("soft sync %-16s %+4.0f cents: repeats after %d period(s) of A to %.1f dB\n", r.name, cents, r.period, natural);
            }
        }
    }

    // A sharp B is not pulled in: it keeps beating (the module does not lock a sharp oscillator either).
    {
        Params p = quiet();
        p.mix[MixBTri] = 1.0f; p.sync = SyncSoft;
        p.b.coarse = (float) (12.0 * std::log2 (1.5) + 0.40);
        auto v = renderAt (p, 44000.0, 57, 1.0, 3.0);
        double err = 0.0, pw = 0.0;
        for (int i = 0; i < 20000; ++i) { const double d = v[(size_t) i] - v[(size_t) (i + 400)]; err += d * d; pw += (double) v[(size_t) i] * v[(size_t) i]; }
        const double db = 10.0 * std::log10 (err / pw + 1.0e-20);
        CHECK (db > -20.0, "a sharp fifth should keep beating (%.1f dB)", db);
    }

    // The jump the pull makes is small once locked, and the output stays band-limited and bounded.
    {
        Params p = quiet();
        p.mix[MixBTri] = 1.0f; p.mix[MixBSine] = 0.5f; p.sync = SyncSoft;
        p.b.coarse = (float) (12.0 * std::log2 (1.5) - 0.10);
        auto v = renderAt (p, kSr, 69, 1.0, 3.0);
        double ms = 0.0;
        for (size_t i = 1; i < v.size(); ++i) ms = std::max (ms, (double) std::fabs (v[i] - v[i - 1]));
        std::printf ("soft sync fifth, A 440 Hz: biggest step between samples %.3f, peak %.3f\n", ms, peak (v));
        CHECK (peak (v) < 1.2, "soft sync fifth: bounded (%.3f)", peak (v));
        CHECK (ms < 0.6, "soft sync fifth: biggest step %.3f", ms);
        const double al = aliasDb (v, hz (69) / 2.0);
        std::printf ("soft sync fifth, A 440 Hz: non-harmonic energy %.1f dB\n", al);
        CHECK (al < -50.0, "soft sync fifth: aliasing %.1f dB", al);
    }

    // The same pull works through the Patch row (any source into Soft sync) and at every quality.
    for (int q = 0; q < 3; ++q)
    {
        Params p = quiet();
        p.quality = q;
        p.mix[MixBTri] = 1.0f;
        p.patch[InSoftSync] = SrcASaw;
        p.b.coarse = (float) (12.0 * std::log2 (1.5) - 0.20);
        auto v = renderAt (p, 44000.0, 57, 1.0, 3.0);
        double err = 0.0, pw = 0.0;
        for (int i = 0; i < 20000; ++i) { const double d = v[(size_t) i] - v[(size_t) (i + 400)]; err += d * d; pw += (double) v[(size_t) i] * v[(size_t) i]; }
        const double db = 10.0 * std::log10 (err / pw + 1.0e-20);
        CHECK (db < -60.0, "patched soft sync fifth at quality %d: %.1f dB", q, db);
    }
}

// The fundamental of a signal by autocorrelation (for presets: which pitch do you hear?).
static double fundamentalHz (const std::vector<float>& v, double rate, double lo, double hi)
{
    const size_t n = std::min<size_t> (v.size(), 16384);
    const int minLag = (int) (rate / hi), maxLag = (int) (rate / lo);
    double best = 0.0; int bestLag = minLag;
    std::vector<double> ac ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double s = 0.0;
        for (size_t i = 0; i + (size_t) lag < n; ++i) s += (double) v[i] * v[i + (size_t) lag];
        ac[(size_t) lag] = s;
        best = std::max (best, s);
    }
    for (int lag = minLag; lag <= maxLag; ++lag)          // the first lag that is nearly as good as the best
        if (ac[(size_t) lag] > 0.9 * best && ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1]) { bestLag = lag; break; }
    return rate / (double) bestLag;
}

static void testPresets()
{
    CHECK (presets::kNumFactory == 9, "factory preset count");
    // distinct names, distinct sounds
    for (int i = 0; i < presets::kNumFactory; ++i)
    {
        CHECK (presets::name (i)[0] != 0 && presets::source (i)[0] != 0, "preset %d has a name and a source", i);
        for (int j = i + 1; j < presets::kNumFactory; ++j)
        {
            CHECK (std::string (presets::name (i)) != presets::name (j), "preset names %d and %d are the same", i, j);
            const Params a = presets::factory (i), b = presets::factory (j);
            bool same = a.keys == b.keys && a.sync == b.sync && a.bSub == b.bSub && a.multMode == b.multMode && a.aLfo == b.aLfo
                     && a.a.fold == b.a.fold && a.b.fold == b.b.fold && a.a.coarse == b.a.coarse && a.b.coarse == b.b.coarse;
            for (int k = 0; k < kNumMix; ++k) same = same && a.mix[k] == b.mix[k];
            for (int k = 0; k < kNumInputs; ++k) same = same && a.patch[k] == b.patch[k];
            CHECK (! same, "presets %d and %d are the same patch", i, j);
        }
    }

    // The reference level: the Init patch's sawtooth.
    const double initLevel = rms (renderAt (presets::init(), kSr, 60, 1.0, 0.6));

    for (int i = 0; i < presets::kNumFactory; ++i)
    {
        const Params p = presets::factory (i);
        const bool sidechainPreset = p.patch[InSymA] == SrcSidechain;

        // every setting inside its range
        for (const OscParams* o : { &p.a, &p.b })
            CHECK (std::fabs (o->coarse) <= 48.0f && std::fabs (o->fine) <= 1.0f && o->pwm >= 0 && o->pwm <= 1 && o->fm >= 0 && o->fm <= 1
                   && o->fold >= 0 && o->fold <= 1 && std::fabs (o->foldCv) <= 1 && std::fabs (o->sym) <= 1, "preset %d: a setting is out of range", i);
        for (int k = 0; k < kNumMix; ++k) CHECK (p.mix[k] >= 0.0f && p.mix[k] <= 1.0f, "preset %d: mixer level %d", i, k);
        CHECK (p.volume > 0.0f && p.volume <= 1.0f, "preset %d: volume", i);
        CHECK (p.quality == 1, "preset %d: leaves Quality at the default value of Params", i);

        // The Final levels: if a wavefolder is listened to, it is either in the mix or patched into something that is.
        bool anyMixed = false;
        for (int k = 0; k < kNumMix; ++k) anyMixed = anyMixed || p.mix[k] > 0.0f;
        CHECK (anyMixed, "preset %d: nothing is mixed", i);
        if (p.patch[InModulator] == SrcAFinal || p.patch[InCarrier] == SrcAFinal || p.patch[InCarrier] == SrcBFinal)
            CHECK (p.mix[MixMult] > 0.0f, "preset %d: uses a Final in the multiplier but the Multiply level is 0", i);

        // Sound: finite, bounded, audible, about the same loudness, at every kind of note and sample rate.
        for (double rate : { 44100.0, 48000.0, 96000.0 })
            for (int note : { 36, 60, 84 })
            {
                std::vector<float> sc;
                if (sidechainPreset)
                {
                    sc.resize ((size_t) (2.2 * rate));
                    for (size_t k = 0; k < sc.size(); ++k) sc[k] = 0.3f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) k / rate);
                }
                auto v = renderAt (p, rate, note, 1.2, 0.6, sidechainPreset ? &sc : nullptr);
                bool finite = true;
                for (float x : v) finite = finite && std::isfinite (x);
                const double level = rms (v), pk = peak (v);
                const double dB = 20.0 * std::log10 (level / initLevel);
                CHECK (finite, "preset %d (%s) at %.0f Hz, note %d: not finite", i, presets::name (i), rate, note);
                CHECK (pk < 1.0, "preset %d (%s) at %.0f Hz, note %d: peak %.2f", i, presets::name (i), rate, note, pk);
                CHECK (level > 0.01, "preset %d (%s) at %.0f Hz, note %d: silent (rms %.4f)", i, presets::name (i), rate, note, level);
                CHECK (dB > 1.0 && dB < 9.0, "preset %d (%s) at %.0f Hz, note %d: %.1f dB against the Init patch (expected about +4)", i, presets::name (i), rate, note, dB);
                if (rate == kSr && note == 60) std::printf ("preset %-26s rms %.1f dB (Init %.1f dB), peak %.2f\n", presets::name (i), 20.0 * std::log10 (level), 20.0 * std::log10 (initLevel), pk);
            }

        // The sidechain preset is silent without a sidechain signal (it is a wavefolder for outside audio).
        if (sidechainPreset)
            CHECK (rms (renderAt (p, kSr, 60, 1.0, 0.6)) < 1.0e-3, "preset %d without a sidechain signal is silent", i);

        // Same settings, same sound.
        auto v1 = renderAt (p, kSr, 60, 0.3, 0.3), v2 = renderAt (p, kSr, 60, 0.3, 0.3);
        CHECK (v1 == v2 || sidechainPreset, "preset %d is not repeatable", i);

        // Every preset also works at the other qualities.
        for (int q : { 0, 2 })
        {
            Params pq = p; pq.quality = q;
            std::vector<float> sc;
            if (sidechainPreset) { sc.assign ((size_t) (1.8 * kSr), 0.0f); for (size_t k = 0; k < sc.size(); ++k) sc[k] = 0.3f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) k / kSr); }
            auto v = renderAt (pq, kSr, 60, 1.0, 0.6, sidechainPreset ? &sc : nullptr);
            auto v4 = renderAt (p, kSr, 60, 1.0, 0.6, sidechainPreset ? &sc : nullptr);
            CHECK (std::fabs (20.0 * std::log10 (rms (v) / rms (v4))) < 1.5, "preset %d at quality %d: %.2f dB from 4x", i, q, 20.0 * std::log10 (rms (v) / rms (v4)));
        }
    }

    // A few things the presets are meant to do, as the manual describes them.
    {   // Formant Voice: the pitch you hear is the note you play (Osc B two octaves up, its sub square two octaves down).
        const Params p = presets::factory (3);
        for (int note : { 48, 55, 60 })
        {
            auto v = renderAt (p, kSr, note, 0.8, 0.5);
            const double f = fundamentalHz (v, kSr, 40.0, 1200.0);
            std::printf ("Formant Voice, note %d (%.1f Hz): fundamental %.1f Hz\n", note, hz (note), f);
            CHECK (std::fabs (f / hz (note) - 1.0) < 0.02, "Formant Voice note %d: %.1f Hz instead of %.1f", note, f, hz (note));
        }
    }
    {   // Spooky Theremin: Osc B's sine at the note's pitch, with a slow wobble.
        const Params p = presets::factory (4);
        auto v = renderAt (p, kSr, 60, 1.5, 0.8);
        const double f = fundamentalHz (v, kSr, 100.0, 1000.0);
        CHECK (std::fabs (f / hz (60) - 1.0) < 0.03, "Spooky Theremin pitch %.1f Hz", f);
    }
    {   // Complete Voice: silent without a note (the multiplier is the VCA and the envelope is closed); loud with one.
        const Params p = presets::factory (1);
        Engine e; e.setParams (p); e.prepare (kSr, kBlock);
        std::vector<float> idle (kBlock * 20);
        for (int b = 0; b < 20; ++b) { float* ch[1] = { idle.data() + b * kBlock }; e.setParams (p); e.process (ch, 1, kBlock); }
        CHECK (peak (idle) < 1.0e-4, "Complete Voice makes sound without a note (%.5f)", peak (idle));
    }
    {   // Wavefolder (sidechain in): at the Symmetry knob's full scale, a quiet signal passes unfolded and a loud one folds.
        const Params p = presets::factory (8);
        auto render = [&] (double amp)
        {
            std::vector<float> sc ((size_t) (1.5 * kSr));
            for (size_t k = 0; k < sc.size(); ++k) sc[k] = (float) (amp * std::sin (2.0 * 3.14159265358979 * 200.0 * (double) k / kSr));
            Params q = p; q.volume = std::sqrt (0.5f);              // gain 1, so the preset's level is what is measured
            return renderAt (q, kSr, 60, 1.0, 0.4, &sc);
        };
        auto quietIn = render (0.1), loudIn = render (0.6);
        const double qs = fundamentalShare (quietIn, 200.0), ls = fundamentalShare (loudIn, 200.0);
        std::printf ("Wavefolder preset: share of the 200 Hz sine in the output: quiet input %.3f, loud input %.3f\n", qs, ls);
        CHECK (qs > 0.9, "quiet input should pass nearly unchanged (%.3f)", qs);
        CHECK (ls < 0.8, "loud input should be folded (%.3f)", ls);
    }
}

static void testCpuM3()
{
    Params plain = quiet();
    plain.mix[MixASaw] = 0.8f; plain.mix[MixBSquare] = 0.8f; plain.a.fm = 0.5f; plain.b.fm = 0.5f;

    Params full = quiet();
    for (auto& m : full.mix) m = 0.8f;
    full.a.fm = 0.6f; full.b.fm = 0.4f; full.sync = SyncHard; full.bSub = 1;
    full.a.fold = 0.9f; full.b.fold = 0.9f; full.a.sym = 0.4f; full.index = 0.7f; full.multDepth = 1.0f; full.multMode = 1;
    full.routeFmA = full.routeFoldA = full.routeSymB = true; full.routePwm = 1;

    Params loops = full;
    loops.patch[InFmA] = SrcAFinal; loops.patch[InFoldB] = SrcMultiply; loops.patch[InModulator] = SrcBFinal;
    loops.patch[InSoftSync] = SrcASaw; loops.patch[InHardSync] = SrcBSquare; loops.patch[InSymA] = SrcSidechain;

    double c[3][3];
    for (int q = 0; q < 3; ++q)
    {
        Params a = plain, b = full, d = loops;
        a.quality = b.quality = d.quality = q;
        c[q][0] = cpuPercent (a, false, 6);
        c[q][1] = cpuPercent (b, true, 6);
        c[q][2] = cpuPercent (d, true, 6);
    }
    std::printf ("cpu by quality (%% of one core, 48 kHz): oscillators only / everything (panel open) / everything with loops\n"
                 "   2x: %.1f / %.1f / %.1f\n   4x: %.1f / %.1f / %.1f\n   8x: %.1f / %.1f / %.1f\n",
                 c[0][0], c[0][1], c[0][2], c[1][0], c[1][1], c[1][2], c[2][0], c[2][1], c[2][2]);
    for (int k = 0; k < 3; ++k)
    {
        CHECK (c[0][k] < 10.0, "2x cpu %.1f%%", c[0][k]);
        CHECK (c[1][k] < 15.0, "4x cpu %.1f%%", c[1][k]);
        CHECK (c[2][k] < 30.0, "8x cpu %.1f%%", c[2][k]);
        CHECK (c[2][k] > c[1][k] && c[1][k] > c[0][k], "each quality step costs more (%.1f %.1f %.1f)", c[0][k], c[1][k], c[2][k]);
    }
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
    testNonlinearMaths();
    testFolder();
    testMultiplier();
    testIndexRouting();
    testPatchSources();
    testPatchSync();
    testFeedback();
    testSwitching();
    testHeavyAutomationM2();
    testCpuM2();
    testQualityMenu();
    testSoftSyncRatios();
    testPresets();
    testCpuM3();
    std::printf ("engine tests: %d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
