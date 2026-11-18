// ComplicatorCheck: runs the plugin layer without a host.
//
//   1. The list of controls is exactly what 0.1.0 / 0.2.0 shipped (same ids, order, ranges, defaults, menu lists), so
//      saved projects and automation keep working.
//   2. Projects saved by 0.1.0 and 0.2.0 (no "preset" entry) load with every control unchanged.
//   3. The programs (Preset menu): Init + the factory presets set exactly what they say and never touch Pitch Snap,
//      Scale, Key or Quality; the chosen preset survives save and load.
//   4. Quality 2x / 4x / 8x: changing it while playing does not click; every setting is audible and the same loudness.
//   5. The panel opens, the Preset and Quality menus are filled, and the panel draws.
//   6. Heavy random automation, including the Quality menu, stays finite and bounded.
//   7. CPU of the whole plugin layer at each Quality (information).
//
// The build workflow runs it on every push and fails if any check fails.

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include <vector>

using namespace complicator;

static int checks = 0, failures = 0;

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

static std::vector<juce::RangedAudioParameter*> allParams (ComplicatorAudioProcessor& proc)
{
    std::vector<juce::RangedAudioParameter*> v;
    for (auto* base : proc.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (base))
            v.push_back (p);
    return v;
}

static float realValue (juce::RangedAudioParameter* p) { return p->convertFrom0to1 (p->getValue()); }
static void setReal (juce::RangedAudioParameter* p, float v) { p->setValueNotifyingHost (p->convertTo0to1 (v)); }
static juce::RangedAudioParameter* param (ComplicatorAudioProcessor& proc, const char* id)
{
    return dynamic_cast<juce::RangedAudioParameter*> (proc.getValueTreeState().getParameter (id));
}

// ------------------------------------------------------------------------------------------------ 1. the control list

static juce::StringArray dumpLayout (ComplicatorAudioProcessor& proc)
{
    juce::StringArray lines;
    int i = 0;
    for (auto* p : allParams (proc))
    {
        const auto& r = p->getNormalisableRange();
        juce::String line;
        line << i++ << " " << p->paramID << " v" << p->getVersionHint() << " | " << p->getName (100)
             << " | range " << juce::String (r.start, 4) << ".." << juce::String (r.end, 4) << " step " << juce::String (r.interval, 4)
             << " | default " << juce::String (p->convertFrom0to1 (p->getDefaultValue()), 4);
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p)) line << " | choices " << c->choices.joinIntoString (";");
        if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr) line << " | switch";
        lines.add (line);
    }
    return lines;
}

static void testControlList (ComplicatorAudioProcessor& proc, const juce::File& goldenDir, bool writeGolden)
{
    const auto now = dumpLayout (proc);
    const juce::File golden = goldenDir.getChildFile ("layout-0.2.0.txt");
    if (writeGolden)
    {
        golden.replaceWithText (now.joinIntoString ("\n") + "\n");
        std::printf ("wrote %s (%d controls)\n", golden.getFullPathName().toRawUTF8(), now.size());
        return;
    }
    juce::StringArray old;
    old.addLines (golden.loadFileAsString());
    old.removeEmptyStrings();
    CHECK (old.size() == 65, "reference list has %d controls", old.size());
    CHECK (now.size() >= old.size(), "control count %d is smaller than the shipped %d", now.size(), old.size());
    int same = 0;
    for (int i = 0; i < old.size() && i < now.size(); ++i)
    {
        if (old[i] == now[i]) ++same;
        else std::printf ("control %d differs:\n   shipped: %s\n   now:     %s\n", i, old[i].toRawUTF8(), now[i].toRawUTF8());
    }
    CHECK (same == old.size(), "%d of %d controls identical to 0.2.0 (ids, order, version, range, default, menus)", same, old.size());
    for (int i = old.size(); i < now.size(); ++i)
        CHECK (now[i].contains (" v2 "), "control added after 0.2.0 must be version 2: %s", now[i].toRawUTF8());
    std::printf ("control list: %d controls, first %d identical to 0.2.0 (0.1.0 had the same list)\n", now.size(), same);
}

// ------------------------------------------------------------------------------------------------ 2. old projects

static void compareAll (ComplicatorAudioProcessor& a, ComplicatorAudioProcessor& b, const char* what)
{
    auto pa = allParams (a), pb = allParams (b);
    CHECK (pa.size() == pb.size(), "%s: control count", what);
    int different = 0;
    for (size_t i = 0; i < pa.size() && i < pb.size(); ++i)
        if (std::fabs (realValue (pa[i]) - realValue (pb[i])) > 1.0e-4f)
        {
            ++different;
            std::printf ("  %s: %s is %g, expected %g\n", what, pa[i]->paramID.toRawUTF8(), realValue (pb[i]), realValue (pa[i]));
        }
    CHECK (different == 0, "%s: %d controls changed", what, different);
}

static void testOldProjects()
{
    for (const char* version : { "0.1.0", "0.2.0" })
    {
        ComplicatorAudioProcessor source;
        std::mt19937 g (version[2] == '1' ? 11u : 22u);
        juce::XmlElement xml ("Parameters");           // exactly the shape 0.1.0 and 0.2.0 wrote: no "preset" entry
        xml.setAttribute ("pluginVersion", version);
        for (auto* p : allParams (source))
        {
            setReal (p, p->convertFrom0to1 (std::uniform_real_distribution<float> (0.0f, 1.0f) (g)));
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", p->paramID);
            e->setAttribute ("value", (double) realValue (p));
        }
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (xml, block);

        ComplicatorAudioProcessor loaded;
        loaded.setStateInformation (block.getData(), (int) block.getSize());
        const juce::String what = juce::String ("project saved by ") + version;
        compareAll (source, loaded, what.toRawUTF8());
        CHECK (loaded.shownPreset() == -1, "%s: the Preset menu shows Custom (%d)", what.toRawUTF8(), loaded.shownPreset());

        // and the engine gets the same settings from both
        const auto ps = source.readParams(), pl = loaded.readParams();
        CHECK (ps.quality == pl.quality && ps.a.coarse == pl.a.coarse && ps.mix[3] == pl.mix[3] && ps.patch[5] == pl.patch[5],
               "%s: engine settings", what.toRawUTF8());
    }

    // A project with Quality left at its default (what every old project has) plays at 4x.
    ComplicatorAudioProcessor fresh;
    CHECK (fresh.readParams().quality == 1 && Engine::factorFor (fresh.readParams().quality) == 4, "default Quality is 4x");

    // A project saved by this version, with a preset chosen, comes back with the preset shown.
    ComplicatorAudioProcessor a;
    a.setCurrentProgram (4);
    setReal (param (a, "aCoarse"), 5.0f);                     // edited after choosing the preset
    juce::MemoryBlock block;
    a.getStateInformation (block);
    ComplicatorAudioProcessor b;
    b.setStateInformation (block.getData(), (int) block.getSize());
    compareAll (a, b, "project saved by this version");
    CHECK (b.shownPreset() == 4 && b.getCurrentProgram() == 4, "preset 4 remembered (%d)", b.shownPreset());

    // and an older plugin (which only looks at the tag name and the controls) would read it fine: same tag, same controls
    if (auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize()))
        CHECK (xml->hasTagName ("Parameters") && xml->getNumChildElements() == 65, "state layout: %d entries", xml->getNumChildElements());
}

// ------------------------------------------------------------------------------------------------ 3. presets

static bool sameSound (const Params& a, const Params& b, juce::String& why)
{
    bool ok = true;
    auto f = [&] (const char* n, float x, float y) { if (std::fabs (x - y) > 1.0e-4f) { ok = false; why << n << " (" << x << " vs " << y << ") "; } };
    auto i = [&] (const char* n, int x, int y) { if (x != y) { ok = false; why << n << " (" << x << " vs " << y << ") "; } };
    auto osc = [&] (const char* who, const OscParams& x, const OscParams& y)
    {
        const juce::String w (who);
        f ((w + "coarse").toRawUTF8(), x.coarse, y.coarse); f ((w + "fine").toRawUTF8(), x.fine, y.fine);
        f ((w + "pwm").toRawUTF8(), x.pwm, y.pwm);         f ((w + "fm").toRawUTF8(), x.fm, y.fm);
        i ((w + "fmCurve").toRawUTF8(), x.fmCurve, y.fmCurve);
        f ((w + "fold").toRawUTF8(), x.fold, y.fold);      f ((w + "foldCv").toRawUTF8(), x.foldCv, y.foldCv);
        f ((w + "sym").toRawUTF8(), x.sym, y.sym);
    };
    osc ("A.", a.a, b.a); osc ("B.", a.b, b.b);
    i ("aLfo", a.aLfo, b.aLfo); i ("bSub", a.bSub, b.bSub); i ("keys", a.keys, b.keys); i ("sync", a.sync, b.sync);
    f ("multDepth", a.multDepth, b.multDepth); i ("multMode", a.multMode, b.multMode);
    f ("index", a.index, b.index); f ("indexCv", a.indexCv, b.indexCv);
    i ("routeFmA", a.routeFmA, b.routeFmA); i ("routeFmB", a.routeFmB, b.routeFmB);
    i ("routeFoldA", a.routeFoldA, b.routeFoldA); i ("routeFoldB", a.routeFoldB, b.routeFoldB);
    i ("routeSymA", a.routeSymA, b.routeSymA); i ("routeSymB", a.routeSymB, b.routeSymB); i ("routePwm", a.routePwm, b.routePwm);
    for (int k = 0; k < kNumInputs; ++k) i ("patch", a.patch[k], b.patch[k]);
    for (int k = 0; k < kNumMix; ++k) f ("mix", a.mix[k], b.mix[k]);
    f ("attack", a.envAttack, b.envAttack); f ("decay", a.envDecay, b.envDecay);
    f ("sustain", a.envSustain, b.envSustain); f ("release", a.envRelease, b.envRelease);
    i ("ampMode", a.ampMode, b.ampMode); f ("volume", a.volume, b.volume);
    return ok;
}

static void testPrograms()
{
    ComplicatorAudioProcessor proc;
    CHECK (proc.getNumPrograms() == 1 + presets::kNumFactory, "program count %d", proc.getNumPrograms());
    CHECK (proc.getProgramName (0) == "Init", "program 0 is Init");
    juce::StringArray names;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        CHECK (proc.getProgramName (i).isNotEmpty(), "program %d has a name", i);
        CHECK (! names.contains (proc.getProgramName (i)), "program name %s is used twice", proc.getProgramName (i).toRawUTF8());
        names.add (proc.getProgramName (i));
    }
    CHECK (proc.getCurrentProgram() == 0 && proc.shownPreset() == 0, "a new instance is on Init");

    // the engine's idea of "Init" is what the controls default to
    {
        juce::String why;
        CHECK (sameSound (proc.readParams(), presets::init(), why), "presets::init() differs from the control defaults: %s", why.toRawUTF8());
    }

    // The player's own set-up: Pitch Snap, Scale, Key, Quality. Set to unusual values; no program may touch them.
    setReal (param (proc, "pitchSnap"), 2.0f);
    setReal (param (proc, "pitchScale"), 5.0f);
    setReal (param (proc, "pitchKey"), 7.0f);
    setReal (param (proc, "quality"), 2.0f);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        // start from a messy state so that a program has to set everything itself
        std::mt19937 g ((unsigned) (100 + i));
        for (auto* p : allParams (proc))
        {
            const juce::String id = p->paramID;
            if (id == "pitchSnap" || id == "pitchScale" || id == "pitchKey" || id == "quality") continue;
            p->setValueNotifyingHost (std::uniform_real_distribution<float> (0.0f, 1.0f) (g));
        }
        proc.setCurrentProgram (i);
        const auto P = proc.readParams();
        juce::String why;
        const Params want = i == 0 ? presets::init() : presets::factory (i - 1);
        CHECK (sameSound (P, want, why), "program %d (%s) does not set what it says: %s", i, proc.getProgramName (i).toRawUTF8(), why.toRawUTF8());
        CHECK (P.snap == 2 && P.scale == 5 && P.key == 7 && P.quality == 2, "program %d changed Snap/Scale/Key/Quality (%d %d %d %d)", i, P.snap, P.scale, P.key, P.quality);
        CHECK (proc.getCurrentProgram() == i && proc.shownPreset() == i, "program %d is current", i);

        // every control value is a legal one (inside its range, whole numbers for menus)
        for (auto* p : allParams (proc))
        {
            const float v = p->getValue();
            CHECK (v >= 0.0f && v <= 1.0f, "program %d: %s out of range", i, p->paramID.toRawUTF8());
        }
    }
    // out-of-range program numbers do not crash
    proc.setCurrentProgram (-3);
    CHECK (proc.getCurrentProgram() == 0, "program -3 is read as 0");
    proc.setCurrentProgram (999);
    CHECK (proc.getCurrentProgram() == proc.getNumPrograms() - 1, "program 999 is read as the last");
    std::printf ("programs: %d (Init + %d factory presets), none touches Snap / Scale / Key / Quality\n", proc.getNumPrograms(), presets::kNumFactory);
}

// ------------------------------------------------------------------------------------------------ audio helpers

struct Runner
{
    ComplicatorAudioProcessor proc;
    double rate;
    int block;
    juce::AudioBuffer<float> buffer { 2, 1 };
    juce::MidiBuffer midi;
    std::vector<float> out;                     // left channel of everything rendered

    explicit Runner (double sr = 48000.0, int blockSize = 256) : rate (sr), block (blockSize)
    {
        proc.setRateAndBufferSizeDetails (sr, blockSize);
        proc.prepareToPlay (sr, blockSize);
        buffer.setSize (2, blockSize);
    }
    void noteOn (int note, int sample = 0) { midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), sample); }
    void noteOff (int note, int sample = 0) { midi.addEvent (juce::MidiMessage::noteOff (1, note), sample); }
    void run (int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            proc.processBlock (buffer, midi);
            midi.clear();
            const float* l = buffer.getReadPointer (0);
            out.insert (out.end(), l, l + block);
        }
    }
    void clearOut() { out.clear(); }
};

static double rmsOf (const std::vector<float>& v, size_t from, size_t to)
{
    double a = 0.0;
    for (size_t i = from; i < to && i < v.size(); ++i) a += (double) v[i] * v[i];
    return std::sqrt (a / (double) std::max<size_t> (1, to - from));
}
static double maxStepOf (const std::vector<float>& v, size_t from, size_t to)
{
    double m = 0.0;
    for (size_t i = from + 1; i < to && i < v.size(); ++i) m = std::max (m, (double) std::fabs (v[i] - v[i - 1]));
    return m;
}
static bool finiteAndBounded (const std::vector<float>& v, double limit, double& maxAbs)
{
    maxAbs = 0.0;
    for (float x : v) { if (! std::isfinite (x)) return false; maxAbs = std::max (maxAbs, (double) std::fabs (x)); }
    return maxAbs < limit;
}

// ------------------------------------------------------------------------------------------------ 4. quality

static void testQuality()
{
    // Every quality plays, at the same loudness (within 1.5 dB).
    double level[3] = {};
    for (int q = 0; q < 3; ++q)
    {
        Runner r;
        setReal (param (r.proc, "quality"), (float) q);
        r.proc.setCurrentProgram (3);                    // Synced Growl: a busy patch
        r.noteOn (60);
        r.run (120);
        level[q] = rmsOf (r.out, 20000, r.out.size());
        double mx;
        CHECK (finiteAndBounded (r.out, 4.0, mx), "quality %d: finite and bounded (max %g)", q, mx);
        CHECK (level[q] > 0.003, "quality %d: sound (%g)", q, level[q]);
        CHECK (Engine::factorFor (r.proc.readParams().quality) == (q == 0 ? 2 : q == 1 ? 4 : 8), "quality %d factor", q);
    }
    for (int q : { 0, 2 })
    {
        const double dB = 20.0 * std::log10 (level[q] / level[1]);
        CHECK (std::fabs (dB) < 1.5, "quality %d is %+.2f dB from 4x", q, dB);
        std::printf ("quality %s level vs 4x: %+.2f dB\n", q == 0 ? "2x" : "8x", dB);
    }

    // Changing the quality while a note sounds: no click, no dropout, never silent for long.
    for (int program : { 1, 3, 7 })
    {
        Runner ref;
        ref.proc.setCurrentProgram (program);
        ref.noteOn (57);
        ref.run (300);

        Runner r;
        r.proc.setCurrentProgram (program);
        r.noteOn (57);
        const int sequence[] = { 1, 0, 2, 1, 2, 0, 1, 0, 2, 2, 0, 1 };
        r.run (20);
        for (int q : sequence)
        {
            setReal (param (r.proc, "quality"), (float) q);
            r.run (24);
        }
        double mx;
        CHECK (finiteAndBounded (r.out, 4.0, mx), "program %d: quality changes: finite and bounded (%g)", program, mx);
        const double stepRef = maxStepOf (ref.out, 2000, ref.out.size());
        const double stepSw = maxStepOf (r.out, 2000, r.out.size());
        // the fade is 4 ms; a fade of a loud signal over 192 samples is a small step per sample, so the biggest step
        // must stay in the range of the steady sound
        CHECK (stepSw < 1.5 * stepRef + 0.01, "program %d: quality changes: biggest step %.4f vs %.4f steady", program, stepSw, stepRef);
        // the sound comes back at the same level after the last change
        const double endLevel = rmsOf (r.out, r.out.size() - 8000, r.out.size());
        const double refLevel = rmsOf (ref.out, ref.out.size() - 8000, ref.out.size());
        CHECK (std::fabs (20.0 * std::log10 (endLevel / refLevel)) < 2.0, "program %d: level after quality changes %.1f dB off", program, 20.0 * std::log10 (endLevel / refLevel));
        std::printf ("quality changes while playing (program %d): biggest step %.4f (steady %.4f)\n", program, stepSw, stepRef);
    }
}

// ------------------------------------------------------------------------------------------------ 5. panel

static void testPanel()
{
    ComplicatorAudioProcessor proc;
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        CHECK (editor != nullptr, "the panel opens");
        if (editor != nullptr)
        {
            CHECK (editor->getWidth() > 600 && editor->getHeight() > 600, "panel size %d x %d", editor->getWidth(), editor->getHeight());
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
            CHECK (image.isValid() && image.getWidth() == editor->getWidth(), "the panel draws");
            // find the two header menus
            juce::ComboBox* preset = nullptr;
            juce::ComboBox* quality = nullptr;
            std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
            {
                if (auto* cb = dynamic_cast<juce::ComboBox*> (&c))
                {
                    if (cb->getName() == "Preset") preset = cb;
                    if (cb->getName() == "Quality") quality = cb;
                }
                for (auto* child : c.getChildren()) walk (*child);
            };
            walk (*editor);
            CHECK (preset != nullptr && quality != nullptr, "Preset and Quality menus exist");
            if (preset != nullptr)
            {
                CHECK (preset->getNumItems() == proc.getNumPrograms(), "Preset menu has %d entries", preset->getNumItems());
                CHECK (preset->isEnabled() && preset->getAlpha() > 0.99f, "Preset menu is not dimmed");
                CHECK (preset->getSelectedId() == 1, "Preset menu starts on Init (%d)", preset->getSelectedId());
                preset->setSelectedId (5, juce::sendNotificationSync);         // choose the fourth factory preset
                CHECK (proc.getCurrentProgram() == 4 && proc.shownPreset() == 4, "choosing in the menu selects program 4 (%d)", proc.getCurrentProgram());
                CHECK (std::fabs (realValue (param (proc, "mixMult")) - presets::factory (3).mix[MixMult]) < 1.0e-4f, "the menu choice set the controls");
            }
            if (quality != nullptr)
            {
                CHECK (quality->getNumItems() == 3 && quality->isEnabled() && quality->getAlpha() > 0.99f, "Quality menu: 3 entries, not dimmed");
                CHECK (quality->getItemText (0) == "2x" && quality->getItemText (1) == "4x" && quality->getItemText (2).startsWith ("8x"),
                       "Quality entries: %s %s %s", quality->getItemText (0).toRawUTF8(), quality->getItemText (1).toRawUTF8(), quality->getItemText (2).toRawUTF8());
                CHECK (quality->getItemText (2).containsIgnoreCase ("heavy"), "8x is marked heavy: %s", quality->getItemText (2).toRawUTF8());
                CHECK (quality->getSelectedId() == 2, "Quality starts on 4x (%d)", quality->getSelectedId());
            }
        }
    }
    // the choice names the host sees did not change
    auto* q = dynamic_cast<juce::AudioParameterChoice*> (proc.getValueTreeState().getParameter ("quality"));
    CHECK (q != nullptr && q->choices.joinIntoString (";") == "2x;4x;8x", "Quality menu list as the host sees it");
}

// ------------------------------------------------------------------------------------------------ 6. automation

static void testAutomation()
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        Runner r (rate, 512);
        std::mt19937 g (77u + (unsigned) rate);
        auto ps = allParams (r.proc);
        r.noteOn (48);
        for (int b = 0; b < 400; ++b)
        {
            for (auto* p : ps)
                if (std::uniform_int_distribution<int> (0, 5) (g) == 0)
                    p->setValueNotifyingHost (std::uniform_real_distribution<float> (0.0f, 1.0f) (g));
            if (b % 37 == 0) r.proc.setCurrentProgram (std::uniform_int_distribution<int> (0, 9) (g));
            if (b % 11 == 0) r.noteOn (36 + std::uniform_int_distribution<int> (0, 48) (g), 100);
            if (b % 29 == 0) r.noteOff (48, 200);
            r.run (1);
        }
        double mx;
        CHECK (finiteAndBounded (r.out, 20.0, mx), "automation at %.0f Hz (quality and presets included): finite, max %g", rate, mx);
        std::printf ("heavy automation through the plugin, everything incl. Quality and presets, %.0f Hz: max output %.2f\n", rate, mx);
    }
}

// ------------------------------------------------------------------------------------------------ 7. CPU

static double cpuOfPlugin (int quality, int program)
{
    Runner r (48000.0, 512);
    r.proc.setCurrentProgram (program);
    setReal (param (r.proc, "quality"), (float) quality);
    r.noteOn (48);
    r.run (4);
    const auto t0 = std::chrono::steady_clock::now();
    const int blocks = (int) (10.0 * 48000.0 / 512.0);
    for (int b = 0; b < blocks; ++b)
    {
        r.buffer.clear();
        r.proc.processBlock (r.buffer, r.midi);
    }
    return std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() * 10.0;     // % of one core
}

static void testCpu()
{
    // "Everything on": every output mixed, both folders, multiplier, loops, the panel's scopes are off (no panel open).
    ComplicatorAudioProcessor setup;
    for (int p : { 1, 2, 6 })
    {
        const double c2 = cpuOfPlugin (0, p), c4 = cpuOfPlugin (1, p), c8 = cpuOfPlugin (2, p);
        std::printf ("plugin cpu (%% of one core, 48 kHz, 512 block), program %d: 2x %.1f   4x %.1f   8x %.1f\n", p, c2, c4, c8);
        CHECK (c8 > c4 * 0.9 && c4 > c2 * 0.9, "program %d: the quality steps cost more each", p);
        CHECK (c4 < 25.0, "program %d: 4x costs %.1f%%", p, c4);
    }
}

// For looking at the panel without Logic: --snapshot <folder> writes PNGs of the panel in a few states.
static void writeSnapshots (const juce::File& dir)
{
    dir.createDirectory();
    auto shoot = [&] (ComplicatorAudioProcessor& proc, const char* name)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        juce::FileOutputStream out (dir.getChildFile (name));
        out.setPosition (0);
        out.truncate();
        png.writeImageToStream (image, out);
    };
    {
        ComplicatorAudioProcessor p;
        shoot (p, "panel-init.png");
    }
    {
        ComplicatorAudioProcessor p;
        p.setCurrentProgram (3);
        setReal (param (p, "quality"), 2.0f);
        shoot (p, "panel-growl-8x.png");
    }
    {
        ComplicatorAudioProcessor p;
        p.setCurrentProgram (9);
        shoot (p, "panel-wavefolder.png");
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    bool writeGolden = false;
    juce::File goldenDir (COMPLICATOR_GOLDEN_DIR);
    for (int i = 1; i < argc; ++i)
    {
        if (juce::String (argv[i]) == "--write-golden") writeGolden = true;
        if (juce::String (argv[i]) == "--snapshot" && i + 1 < argc) { writeSnapshots (juce::File (argv[i + 1])); return 0; }
    }

    {
        ComplicatorAudioProcessor proc;
        testControlList (proc, goldenDir, writeGolden);
    }
    if (writeGolden) return 0;

    testOldProjects();
    testPrograms();
    testQuality();
    testPanel();
    testAutomation();
    testCpu();

    std::printf ("plugin checks: %d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
