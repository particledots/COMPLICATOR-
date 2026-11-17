#include "complicator/Engine.h"
#include "complicator/Pitch.h"

#include <algorithm>
#include <cmath>

namespace complicator
{
namespace
{
    // How far below the audio range LFO mode puts Osc A: 7 octaves.
    constexpr float kLfoDivide = 128.0f;
    // FM amount at full: +-5 octaves (Exp), or +-5 times the frequency (Lin / through-zero), for a full-size input.
    constexpr float kFmRange = 5.0f;
    // Each output into the mixer is scaled by this, so several outputs at full still leave headroom.
    constexpr float kMixScale = 0.5f;
    // The middle note for an oscillator that does not follow the keys (MIDI 48, about 130.8 Hz).
    constexpr int kFreeNote = 48;

    inline float noteToHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) * (1.0f / 12.0f)); }
    inline float envTime (float knob) { return 0.001f * std::pow (10000.0f, std::clamp (knob, 0.0f, 1.0f)); }   // 1 ms .. 10 s
    inline float smoothCoef (double seconds, double rate) { return (float) (1.0 - std::exp (-1.0 / (seconds * rate))); }
}

float Engine::Envelope::next (float attackStep, float decayCoef, float sustain, float releaseCoef)
{
    switch (stage)
    {
        case Attack:
            level += attackStep;
            if (level >= 1.0f) { level = 1.0f; stage = Decay; }
            break;
        case Decay:
        case Sustain:
            level = sustain + (level - sustain) * decayCoef;
            if (stage == Decay && std::fabs (level - sustain) < 1.0e-4f) stage = Sustain;
            break;
        case Release:
            level *= releaseCoef;
            if (level < 1.0e-5f) { level = 0.0f; stage = Idle; }
            break;
        case Idle:
        default:
            level = 0.0f;
            break;
    }
    return level;
}

void Engine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    factor = 4;                                    // fixed in milestone 1; the Quality menu (M3) will choose 2, 4 or 8
    osRate = sr * factor;
    decim.prepare (sr, factor);

    const float knob = smoothCoef (0.015, sr);
    for (int k = 0; k < 2; ++k)
    {
        sCoarse[k].coef = knob; sFine[k].coef = knob; sPwm[k].coef = knob; sFm[k].coef = knob;
        sCoarse[k].v = (k == 0 ? params.a : params.b).coarse;
        sFine[k].v   = (k == 0 ? params.a : params.b).fine;
        sPwm[k].v    = (k == 0 ? params.a : params.b).pwm;
        sFm[k].v     = (k == 0 ? params.a : params.b).fm;
    }
    for (int m = 0; m < kNumMix; ++m) { sMix[m].coef = knob; sMix[m].v = params.mix[m]; }
    sVolume.coef = knob; sVolume.v = params.volume;
    sBend.coef = smoothCoef (0.005, sr);
    sAmp.coef = smoothCoef (0.001, sr);
    dcCoef = (float) (1.0 - 2.0 * 3.14159265358979 * 5.0 / sr);   // removes DC (below about 5 Hz)
    reset();
}

void Engine::reset()
{
    oscA.reset();
    oscB.reset();
    decim.reset();
    env = Envelope();
    sAmp.v = 0.0f;
    lastSineA = lastSineB = 0.0f;
    dcX = dcY = 0.0f;
    for (auto& q : qcache) q = QCache();
}

void Engine::noteOn (int note, float vel)
{
    note = std::clamp (note, 0, 127);
    noteOff (note);                                // remove it if it is already held (keeps the gate as it is)
    if (stackSize < 128) noteStack[stackSize++] = note;
    playedNote = note;
    velocity = std::clamp (vel, 0.0f, 1.0f);
    env.gateOn();
}

void Engine::noteOff (int note)
{
    int w = 0;
    bool found = false;
    for (int i = 0; i < stackSize; ++i)
    {
        if (noteStack[i] == note) { found = true; continue; }
        noteStack[w++] = noteStack[i];
    }
    stackSize = w;
    if (! found) return;
    if (stackSize > 0) playedNote = noteStack[stackSize - 1];    // back to the previous held note (no new attack)
    else env.gateOff();
}

void Engine::allNotesOff()
{
    stackSize = 0;
    env.gateOff();
}

void Engine::pitchBend (float semitones) { bendTarget = semitones; }

float Engine::quantizedCoarse (int which, float coarse, int baseNote)
{
    if (params.snap == pitch::SnapOff && params.scale == 0)
        return std::clamp (coarse, -pitch::kCoarseRange, pitch::kCoarseRange);
    auto& q = qcache[which];
    if (std::fabs (q.coarse - coarse) > 1.0e-4f || q.base != baseNote || q.snap != params.snap
        || q.scale != params.scale || q.key != params.key)
    {
        q.coarse = coarse; q.base = baseNote; q.snap = params.snap; q.scale = params.scale; q.key = params.key;
        q.result = pitch::quantize (coarse, params.snap, params.scale, params.key, baseNote);
    }
    return q.result;
}

float Engine::fmFrequency (float f, float amount, int curve, float input) const
{
    const float depth = amount * amount * kFmRange;      // squared: fine control near zero
    if (depth <= 0.0f) return f;
    if (curve == FmExp) return f * std::exp2 (depth * input);
    const float g = f * (1.0f + depth * input);
    return (curve == FmThroughZero) ? g : std::max (0.0f, g);
}

void Engine::process (float* const* out, int numChannels, int numSamples)
{
    const Params& P = params;
    const bool followA = P.keys == KeysA || P.keys == KeysAB;
    const bool followB = P.keys == KeysB || P.keys == KeysAB;

    const float attackStep = 1.0f / (envTime (P.envAttack) * (float) sr);
    const float decayCoef = std::exp (-4.0f / (envTime (P.envDecay) * (float) sr));
    const float releaseCoef = std::exp (-4.0f / (envTime (P.envRelease) * (float) sr));
    const float sustain = std::clamp (P.envSustain, 0.0f, 1.0f);
    const int subMode = std::clamp (P.bSub, 0, 2);
    const float osr = (float) osRate;

    float os[kMaxFactor];
    float qA = 0.0f, qB = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float bend = sBend.next (bendTarget);
        const int baseA = followA ? playedNote : kFreeNote;
        const int baseB = followB ? playedNote : kFreeNote;
        qA = quantizedCoarse (0, sCoarse[0].next (P.a.coarse), baseA);
        qB = quantizedCoarse (1, sCoarse[1].next (P.b.coarse), baseB);
        const float noteA = (float) baseA + qA + sFine[0].next (P.a.fine) + (followA ? bend : 0.0f);
        const float noteB = (float) baseB + qB + sFine[1].next (P.b.fine) + (followB ? bend : 0.0f);
        float fA = noteToHz (noteA);
        if (P.aLfo) fA /= kLfoDivide;
        const float fB = noteToHz (noteB);

        // The PWM knobs' range always leaves an audible wave (the M2 CV inputs can push beyond).
        const float duty = 0.5f + (sPwm[0].next (P.a.pwm) - 0.5f) * 0.9f;
        const float stepWidth = 0.85f - 0.7f * sPwm[1].next (P.b.pwm);
        const float fmA = sFm[0].next (P.a.fm), fmB = sFm[1].next (P.b.fm);

        float mix[kNumMix];
        for (int m = 0; m < kNumMix; ++m) { const float l = sMix[m].next (P.mix[m]); mix[m] = l * l * kMixScale; }

        for (int j = 0; j < factor; ++j)
        {
            // FM inputs: as on the module, each oscillator's sine feeds the other's FM input.
            const float fiA = fmFrequency (fA, fmA, P.a.fmCurve, lastSineB);
            const float fiB = fmFrequency (fB, fmB, P.b.fmCurve, lastSineA);
            const double incA = std::clamp ((double) (fiA / osr), -0.25, 0.25);
            const double speedB = std::clamp (4.0 * (double) (fiB / osr), -1.0, 1.0);

            OscAOut a;
            OscBOut b;
            if (P.sync == SyncHard)                       // Osc B's square (or sub square) restarts Osc A
            {
                oscB.process (speedB, stepWidth, subMode, -1.0f, b);
                oscA.process (incA, duty, b.syncEdge, a);
            }
            else if (P.sync == SyncSoft)                  // Osc A's sawtooth turns Osc B round
            {
                const float wrap = oscA.process (incA, duty, -1.0f, a);
                oscB.process (speedB, stepWidth, subMode, wrap, b);
            }
            else
            {
                oscA.process (incA, duty, -1.0f, a);
                oscB.process (speedB, stepWidth, subMode, -1.0f, b);
            }
            lastSineA = a.sine;
            lastSineB = b.sine;

            os[j] = mix[MixASine] * a.sine + mix[MixATri] * a.tri + mix[MixASaw] * a.saw + mix[MixAPwm] * a.pulse
                  + mix[MixBSine] * b.sine + mix[MixBTri] * b.tri + mix[MixBSquare] * b.square + mix[MixBPwm] * b.stepped;
            // MixAFinal, MixMult and MixBFinal arrive with the wavefolders and the multiplier (milestone 2).
        }

        float y = decim.process (os);
        const float hp = y - dcX + dcCoef * dcY;          // DC filter (LFO mode can put very slow signals out)
        dcX = y; dcY = hp;

        const float e = env.next (attackStep, decayCoef, sustain, releaseCoef);
        const float amp = sAmp.next (P.ampMode == AmpDrone ? 1.0f : e);
        const float v = sVolume.next (P.volume);
        const float sample = hp * amp * (v * v * 2.0f);   // Volume: 0 = silent, 0.71 = 0 dB, 1 = +6 dB

        for (int c = 0; c < numChannels; ++c)
            out[c][i] = sample;
    }

    shownCoarseA.store (qA, std::memory_order_relaxed);
    shownCoarseB.store (qB, std::memory_order_relaxed);
}

} // namespace complicator
