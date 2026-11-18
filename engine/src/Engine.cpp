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

    // Milestone 2. Signals inside are +-1 (= +-5 V on the module); envelope, velocity, wheel and aftertouch are 0..1.
    constexpr float kSymScale = 3.0f;         // Symmetry knob at full = +-3 x the signal at the Symmetry input
    constexpr float kSymRef = 1.0f / 3.0f;    // the DC reference normalled to each Symmetry input (so the knob = +-1 bias)
    constexpr float kPwmCvRange = 0.5f;       // a full-size signal at a PWM CV input moves the PWM knob by +-0.5
    constexpr float kDepthUnity = 0.72f;      // multiplier Depth at which the multiplier has unity gain (about 2 o'clock)
    constexpr float kHardThreshold = 0.5f;    // 2.5 V
    constexpr float kSoftThreshold = 0.4f;    // 2 V
    constexpr float kEdgeHysteresis = 0.1f;
    constexpr double kFadeSeconds = 0.006;    // cross-fade when a Patch menu or an Index route changes
    constexpr float kBoundSidechain = 4.0f;

    // Input "codes": 0..17 = a source (PatchSource), + kScaledBit = scaled by the Index; 60..63 = special signals.
    constexpr int kScaledBit = 64, kCurB = 60, kCurA = 61, kZero = 62, kDc = 63;

    // Wavefold fader (0..1) to the gain in front of the folder: 0 = silence, 0.5 = 1 (a sine fits exactly), 1 = 10.
    inline float foldGain (float f)
    {
        f = std::clamp (f, 0.0f, 1.0f);
        if (f < 0.5f) { const float t = 2.0f * f; return t * t; }
        const float u = 2.0f * f - 1.0f;
        return 1.0f + 9.0f * u * u;
    }

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

Engine::Engine()
{
    for (auto& buf : scopeBuf)
        for (auto& v : buf)
            v.store (0.0f, std::memory_order_relaxed);
}

void Engine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    for (int k = 0; k < 3; ++k) decim[k].prepare (sr, 1 << (k + 1));    // 2x, 4x, 8x
    factor = factorFor (params.quality);
    decimIdx = factor == 2 ? 0 : (factor == 4 ? 1 : 2);
    osRate = sr * factor;
    qGain = 1.0f;
    qStep = (float) (1.0 / (0.004 * sr));                // 4 ms fade when the quality changes

    const float knob = smoothCoef (0.015, sr);
    for (int k = 0; k < 2; ++k)
    {
        const OscParams& o = k == 0 ? params.a : params.b;
        sCoarse[k].coef = knob; sFine[k].coef = knob; sPwm[k].coef = knob; sFm[k].coef = knob;
        sFold[k].coef = knob; sFoldCv[k].coef = knob; sSym[k].coef = knob;
        sCoarse[k].v = o.coarse;
        sFine[k].v   = o.fine;
        sPwm[k].v    = o.pwm;
        sFm[k].v     = o.fm;
        sFold[k].v   = o.fold;
        sFoldCv[k].v = o.foldCv;
        sSym[k].v    = o.sym;
    }
    for (int m = 0; m < kNumMix; ++m) { sMix[m].coef = knob; sMix[m].v = params.mix[m]; }
    sVolume.coef = knob; sVolume.v = params.volume;
    sDepth.coef = knob; sDepth.v = params.multDepth;
    sIndex.coef = knob; sIndex.v = params.index;
    sIndexCv.coef = knob; sIndexCv.v = params.indexCv;
    const int mode = std::clamp (params.multMode, 0, 2);
    for (int m = 0; m < 3; ++m) { sMode[m].coef = smoothCoef (0.008, sr); sMode[m].v = m == mode ? 1.0f : 0.0f; }
    sVel.coef = smoothCoef (0.002, sr);
    sWheel.coef = smoothCoef (0.01, sr);
    sTouch.coef = smoothCoef (0.01, sr);
    sBend.coef = smoothCoef (0.005, sr);
    sAmp.coef = smoothCoef (0.001, sr);
    dcCoef = (float) (1.0 - 2.0 * 3.14159265358979 * 5.0 / sr);   // removes DC (below about 5 Hz)
    fadeStep = (float) (1.0 / (kFadeSeconds * osRate));

    for (int k = 0; k < kContinuousInputs; ++k)
    {
        inCode[k] = inPrev[k] = codeFor (k, params.patch[k]);
        inFade[k] = 1.0f;
    }
    sVel.v = velocity; sWheel.v = wheel; sTouch.v = touch;
    reset();
}

void Engine::clearFeedback()
{
    S.fill (0.0f);
    curASine = curBSine = 0.0f;
    idxGain = 0.0f;
    hardEdge = EdgeDetector();
    softEdge = EdgeDetector();
    foldPrimed[0] = foldPrimed[1] = clipPrimed = halfPrimed = absPrimed = false;
    for (float& h : scHist) h = 0.0f;
}

void Engine::setFactor (int f)
{
    factor = f;
    decimIdx = f == 2 ? 0 : (f == 4 ? 1 : 2);
    osRate = sr * factor;
    fadeStep = (float) (1.0 / (kFadeSeconds * osRate));
    decim[decimIdx].reset();
    clearFeedback();
}

void Engine::reset()
{
    oscA.reset();
    oscB.reset();
    for (auto& d : decim) d.reset();
    env = Envelope();
    envLast = 0.0f;
    sAmp.v = 0.0f;
    clearFeedback();
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

float Engine::EdgeDetector::next (float x, float threshold)
{
    float ago = -1.0f;
    if (armed)
    {
        if (prev < threshold && x >= threshold)
        {
            ago = 1.0f - (threshold - prev) / (x - prev);
            ago = std::clamp (ago, 0.0f, 0.99999f);
            armed = false;
        }
    }
    else if (x < threshold - kEdgeHysteresis)
        armed = true;
    prev = x;
    return ago;
}

// What an input receives when its Patch menu says "Normal": the connection the module has inside, with the Index
// routes switched on or off. Menu entries other than Normal are simply that source.
int Engine::codeFor (int input, int selection) const
{
    selection = std::clamp (selection, 0, kNumSources - 1);
    if (selection != SrcNormal) return selection;           // "Off" (1) reads as silence
    const Params& P = params;
    switch (input)
    {
        case InFmA:       return SrcBSine | (P.routeFmA ? kScaledBit : 0);      // Osc B's sine, straight in, or through the Index
        case InFmB:       return SrcASine | (P.routeFmB ? kScaledBit : 0);
        case InPwmA:      return P.routePwm == 1 ? (SrcBSine | kScaledBit) : kZero;                // Both
        case InPwmB:      return P.routePwm >= 1 ? (SrcASine | kScaledBit) : kZero;                // Both, or A to B only
        case InFoldA:     return P.routeFoldA ? (SrcBSine | kScaledBit) : kZero;
        case InFoldB:     return P.routeFoldB ? (SrcASine | kScaledBit) : kZero;
        case InSymA:      return P.routeSymA ? (SrcBSine | kScaledBit) : kDc;   // the route replaces the DC reference
        case InSymB:      return P.routeSymB ? (SrcASine | kScaledBit) : kDc;
        case InCarrier:   return kCurB;                                           // Osc B's sine
        case InModulator: return kCurA;                                           // Osc A's sine
        default:          return kZero;                                           // Index CV has no normal
    }
}

float Engine::sourceValue (int code) const
{
    const int s = code & 63;
    float v;
    if (s < kNumSources)    v = S[(size_t) s];
    else if (s == kCurB)    v = curBSine;
    else if (s == kCurA)    v = curASine;
    else if (s == kDc)      v = kSymRef;
    else                    v = 0.0f;
    return (code & kScaledBit) ? v * idxGain : v;
}

// One input's signal for this oversampled sample. Call exactly once per sample (it advances a cross-fade).
float Engine::readInput (int k)
{
    float f = inFade[k];
    if (f >= 1.0f) return sourceValue (inCode[k]);
    f = std::min (1.0f, f + fadeStep);
    inFade[k] = f;
    const float a = sourceValue (inPrev[k]), b = sourceValue (inCode[k]);
    return a + (b - a) * f;
}

void Engine::scopeCopy (int which, float* dst, int n) const
{
    n = std::clamp (n, 0, kScopeSize);
    const int w = scopeWrite.load (std::memory_order_acquire);
    const auto& buf = scopeBuf[std::clamp (which, 0, 1)];
    for (int i = 0; i < n; ++i)
        dst[i] = buf[(size_t) ((w - n + i) & (kScopeSize - 1))].load (std::memory_order_relaxed);
}

void Engine::process (float* const* out, int numChannels, int numSamples, const float* sidechain)
{
    const Params& P = params;
    const bool followA = P.keys == KeysA || P.keys == KeysAB;
    const bool followB = P.keys == KeysB || P.keys == KeysAB;

    const float attackStep = 1.0f / (envTime (P.envAttack) * (float) sr);
    const float decayCoef = std::exp (-4.0f / (envTime (P.envDecay) * (float) sr));
    const float releaseCoef = std::exp (-4.0f / (envTime (P.envRelease) * (float) sr));
    const float sustain = std::clamp (P.envSustain, 0.0f, 1.0f);
    const int subMode = std::clamp (P.bSub, 0, 2);
    const int mode = std::clamp (P.multMode, 0, 2);
    const int wantedFactor = factorFor (P.quality);
    float osr = (float) osRate;
    float invFactor = 1.0f / (float) factor;

    // Patch row: a changed menu or Index route starts a short cross-fade.
    bool used[kNumSources] = {};
    for (int k = 0; k < kNumInputs; ++k)
        used[std::clamp (P.patch[k], 0, kNumSources - 1)] = true;
    for (int k = 0; k < kContinuousInputs; ++k)
    {
        const int c = codeFor (k, P.patch[k]);
        if (c != inCode[k]) { inPrev[k] = inCode[k]; inCode[k] = c; inFade[k] = 0.0f; }
    }
    // Sync inputs. What each one listens to: "Normal" follows the Sync switch, anything else is the chosen source.
    // Osc B's square into Hard sync and Osc A's sawtooth into Soft sync are exactly what the Sync switch does, so they
    // run the same exact path (the edge is known to a fraction of a sample, in the same sample). Any other source is
    // found by a threshold (2.5 V hard, 2 V soft) on the previous oversampled sample's values.
    // (If both would need the other oscillator's same-sample output, Hard keeps the exact path and Soft the threshold.)
    const int hardSel = std::clamp (P.patch[InHardSync], 0, kNumSources - 1);
    const int softSel = std::clamp (P.patch[InSoftSync], 0, kNumSources - 1);
    const int hardWant = hardSel != SrcNormal ? hardSel : (P.sync == SyncHard ? (int) SrcBSquare : -1);
    const int softWant = softSel != SrcNormal ? softSel : (P.sync == SyncSoft ? (int) SrcASaw : -1);
    const bool hardDirect = hardWant == SrcBSquare;
    const bool softDirect = ! hardDirect && softWant == SrcASaw;
    const int hardCode = hardDirect ? -1 : hardWant;
    const int softCode = softDirect ? -1 : softWant;
    const bool scope = scopeActive.load (std::memory_order_relaxed);
    const bool sidechainUsed = used[SrcSidechain];

    float os[kMaxFactor];
    float qA = 0.0f, qB = 0.0f;
    float scopeA = 0.0f, scopeB = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // ---- a changed Quality: fade out, switch the oversampling while silent, fade in again
        if (wantedFactor != factor)
        {
            qGain -= qStep;
            if (qGain <= 0.0f)
            {
                qGain = 0.0f;
                setFactor (wantedFactor);
                osr = (float) osRate;
                invFactor = 1.0f / (float) factor;
            }
        }
        else if (qGain < 1.0f)
            qGain = std::min (1.0f, qGain + qStep);

        // ---- controls, once per project-rate sample (every continuous control is smoothed)
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

        // The PWM knobs' range always leaves an audible wave; the PWM CV inputs can push beyond it.
        const float pwmKnobA = sPwm[0].next (P.a.pwm), pwmKnobB = sPwm[1].next (P.b.pwm);
        const float fmA = sFm[0].next (P.a.fm), fmB = sFm[1].next (P.b.fm);

        const float foldKnob[2] = { sFold[0].next (P.a.fold), sFold[1].next (P.b.fold) };
        const float foldCvAmt[2] = { sFoldCv[0].next (P.a.foldCv), sFoldCv[1].next (P.b.foldCv) };
        const float symAmt[2] = { sSym[0].next (P.a.sym), sSym[1].next (P.b.sym) };
        const float multGain = sDepth.next (P.multDepth) / kDepthUnity;
        const float wRing = sMode[0].next (mode == 0 ? 1.0f : 0.0f);
        const float wHalf = sMode[1].next (mode == 1 ? 1.0f : 0.0f);
        const float wFull = sMode[2].next (mode == 2 ? 1.0f : 0.0f);
        const float indexKnob = sIndex.next (P.index), indexCvAmt = sIndexCv.next (P.indexCv);
        const float velV = sVel.next (velocity), wheelV = sWheel.next (wheel), touchV = sTouch.next (touch);

        // Sidechain: keep the last four project-rate samples (cubic interpolation, one sample of delay).
        float scNow = sidechain != nullptr ? sidechain[i] : 0.0f;
        if (! (std::fabs (scNow) < 1.0e6f)) scNow = 0.0f;
        scHist[0] = scHist[1]; scHist[1] = scHist[2]; scHist[2] = scHist[3]; scHist[3] = scNow;
        const float sc0 = scHist[0], sc1 = scHist[1], sc2 = scHist[2], sc3 = scHist[3];
        const float scA = -0.5f * sc0 + 1.5f * sc1 - 1.5f * sc2 + 0.5f * sc3;
        const float scB = sc0 - 2.5f * sc1 + 2.0f * sc2 - 0.5f * sc3;
        const float scC = -0.5f * sc0 + 0.5f * sc2;

        float mix[kNumMix];
        for (int m = 0; m < kNumMix; ++m) { const float l = sMix[m].next (P.mix[m]); mix[m] = l * l * kMixScale; }

        // The envelope runs once per project-rate sample; it is the amp envelope and a Patch source.
        const float e = env.next (attackStep, decayCoef, sustain, releaseCoef);
        const float envFrom = envLast;
        envLast = e;

        // Stages nobody listens to are not run (CPU). The scope listens to both Finals while the panel is open.
        const bool needFinal[2] = { mix[MixAFinal] > 1.0e-7f || used[SrcAFinal] || scope,
                                    mix[MixBFinal] > 1.0e-7f || used[SrcBFinal] || scope };
        const bool needMult = mix[MixMult] > 1.0e-7f || used[SrcMultiply];

        for (int j = 0; j < factor; ++j)
        {
            // 1. Sources that do not come from the oscillators
            const float jt = (float) (j + 1) * invFactor;
            S[SrcEnvelope] = envFrom + (e - envFrom) * jt;
            S[SrcVelocity] = velV;
            S[SrcModWheel] = wheelV;
            S[SrcAftertouch] = touchV;
            if (sidechainUsed)
            {
                const float t = (float) j * invFactor;
                S[SrcSidechain] = std::clamp (((scA * t + scB) * t + scC) * t + sc1, -kBoundSidechain, kBoundSidechain);
            }

            // 2. Index, PWM and FM inputs (the other inputs are read after the oscillators)
            idxGain = std::clamp (indexKnob + indexCvAmt * readInput (InIndex), 0.0f, 1.0f);
            const float kA = pwmKnobA + kPwmCvRange * readInput (InPwmA);
            const float kB = pwmKnobB + kPwmCvRange * readInput (InPwmB);
            const float duty = std::clamp (0.5f + (kA - 0.5f) * 0.9f, 0.0f, 1.0f);
            const float stepWidth = std::clamp (0.85f - 0.7f * kB, 0.0f, 0.98f);
            const float fiA = fmFrequency (fA, fmA, P.a.fmCurve, readInput (InFmA));
            const float fiB = fmFrequency (fB, fmB, P.b.fmCurve, readInput (InFmB));
            const double incA = std::clamp ((double) (fiA / osr), -0.25, 0.25);
            const double speedB = std::clamp (4.0 * (double) (fiB / osr), -1.0, 1.0);

            float hardAt = -1.0f, softAt = -1.0f;
            if (hardCode >= 0) hardAt = hardEdge.next (sourceValue (hardCode), kHardThreshold);
            if (softCode >= 0) softAt = softEdge.next (sourceValue (softCode), kSoftThreshold);

            // 3. Oscillators
            OscAOut a;
            OscBOut b;
            if (hardDirect)                               // Osc B's square (or sub square) restarts Osc A
            {
                oscB.process (speedB, stepWidth, subMode, softAt, b);
                oscA.process (incA, duty, b.syncEdge, a);
            }
            else                                          // Osc A's sawtooth turns Osc B round (Soft), or no internal sync
            {
                const float wrap = oscA.process (incA, duty, hardAt, a);
                oscB.process (speedB, stepWidth, subMode, softDirect ? wrap : softAt, b);
            }
            curASine = a.sine;
            curBSine = b.sine;

            // 4. Wavefolders and multiplier
            float finalOut[2] = { 0.0f, 0.0f };
            for (int w = 0; w < 2; ++w)
            {
                if (! needFinal[w]) { foldPrimed[w] = false; continue; }
                const float sine = w == 0 ? a.sine : b.sine;
                const float amount = std::clamp (foldKnob[w] + foldCvAmt[w] * readInput (w == 0 ? InFoldA : InFoldB), 0.0f, 1.0f);
                const float bias = symAmt[w] * kSymScale * readInput (w == 0 ? InSymA : InSymB);
                const double pre = std::clamp ((double) (foldGain (amount) * sine + bias), -64.0, 64.0);
                if (! foldPrimed[w]) { foldAdaa[w].reset (pre); foldPrimed[w] = true; }
                finalOut[w] = (float) foldAdaa[w].next (pre);
            }

            float mult = 0.0f;
            if (needMult)
            {
                const double carrier = readInput (InCarrier);
                const double modulator = readInput (InModulator);
                double m = (double) wRing * modulator;
                if (wHalf > 1.0e-5f)
                {
                    if (! halfPrimed) { halfAdaa.reset (modulator); halfPrimed = true; }
                    m += (double) wHalf * halfAdaa.next (modulator);
                }
                else halfPrimed = false;
                if (wFull > 1.0e-5f)
                {
                    if (! absPrimed) { absAdaa.reset (modulator); absPrimed = true; }
                    m += (double) wFull * absAdaa.next (modulator);
                }
                else absPrimed = false;
                const double product = std::clamp ((double) multGain * m * carrier, -16.0, 16.0);
                if (! clipPrimed) { clipAdaa.reset (product); clipPrimed = true; }
                mult = (float) clipAdaa.next (product);
            }
            else clipPrimed = false;

            // Everything above is the "previous sample" for the next pass through the Patch row.
            S[SrcASine] = a.sine;   S[SrcATri] = a.tri;      S[SrcASaw] = a.saw;       S[SrcAPwm] = a.pulse;
            S[SrcAFinal] = finalOut[0];
            S[SrcBSine] = b.sine;   S[SrcBTri] = b.tri;      S[SrcBSquare] = b.square; S[SrcBPwm] = b.stepped;
            S[SrcBFinal] = finalOut[1];
            S[SrcMultiply] = mult;

            // 5. Output mixer
            os[j] = mix[MixASine] * a.sine + mix[MixATri] * a.tri + mix[MixASaw] * a.saw + mix[MixAPwm] * a.pulse
                  + mix[MixAFinal] * finalOut[0] + mix[MixMult] * mult
                  + mix[MixBSine] * b.sine + mix[MixBTri] * b.tri + mix[MixBSquare] * b.square + mix[MixBPwm] * b.stepped
                  + mix[MixBFinal] * finalOut[1];
            if (j == 0) { scopeA = finalOut[0]; scopeB = finalOut[1]; }
        }

        float y = decim[decimIdx].process (os);
        if (! std::isfinite (y))                          // should never happen; if it does, start clean rather than howl
        {
            oscA.reset(); oscB.reset(); decim[decimIdx].reset(); clearFeedback();
            dcX = dcY = 0.0f;
            y = 0.0f;
        }
        const float hp = y - dcX + dcCoef * dcY;          // DC filter (LFO mode can put very slow signals out)
        dcX = y; dcY = hp;

        const float amp = sAmp.next (P.ampMode == AmpDrone ? 1.0f : e);
        const float v = sVolume.next (P.volume);
        const float sample = hp * amp * (v * v * 2.0f) * qGain;   // Volume: 0 = silent, 0.71 = 0 dB, 1 = +6 dB

        for (int c = 0; c < numChannels; ++c)
            out[c][i] = sample;

        if (scope)
        {
            const int w = scopeWrite.load (std::memory_order_relaxed);
            scopeBuf[0][(size_t) w].store (scopeA, std::memory_order_relaxed);
            scopeBuf[1][(size_t) w].store (scopeB, std::memory_order_relaxed);
            scopeWrite.store ((w + 1) & (kScopeSize - 1), std::memory_order_release);
        }
    }

    shownCoarseA.store (qA, std::memory_order_relaxed);
    shownCoarseB.store (qB, std::memory_order_relaxed);
}

} // namespace complicator
