#include "PluginProcessor.h"
#include "PluginEditor.h"

ComplicatorAudioProcessor::ComplicatorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          // Sidechain input: declared from the first build so saved projects never change.
                          // It is used from milestone 2 on (Patch row source "Sidechain"). Off unless chosen in Logic.
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", CxParams::createLayout())
{
    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pa = { p (CxParams::aCoarse), p (CxParams::aFine), p (CxParams::aPwm), p (CxParams::aFm), p (CxParams::aFmCurve),
           p (CxParams::aFold), p (CxParams::aFoldCv), p (CxParams::aSym) };
    pb = { p (CxParams::bCoarse), p (CxParams::bFine), p (CxParams::bPwm), p (CxParams::bFm), p (CxParams::bFmCurve),
           p (CxParams::bFold), p (CxParams::bFoldCv), p (CxParams::bSym) };
    aLfo = p (CxParams::aLfo);  bSub = p (CxParams::bSub);
    keys = p (CxParams::keys);  snap = p (CxParams::pitchSnap);  scale = p (CxParams::pitchScale);
    key = p (CxParams::pitchKey);  sync = p (CxParams::sync);
    multDepth = p (CxParams::multDepth);  multMode = p (CxParams::multMode);
    index = p (CxParams::index);  indexCv = p (CxParams::indexCv);
    const char* routeIds[7] = { CxParams::routeFmA, CxParams::routeFmB, CxParams::routeFoldA, CxParams::routeFoldB,
                                CxParams::routeSymA, CxParams::routeSymB, CxParams::routePwm };
    for (int i = 0; i < 7; ++i) routes[i] = p (routeIds[i]);
    for (int i = 0; i < CxParams::numPatch; ++i) patch[i] = p (CxParams::patchIds[i]);
    for (int i = 0; i < CxParams::numMix; ++i) mix[i] = p (CxParams::mixIds[i]);
    envA = p (CxParams::envAttack);  envD = p (CxParams::envDecay);
    envS = p (CxParams::envSustain); envR = p (CxParams::envRelease);
    ampMode = p (CxParams::ampMode); volume = p (CxParams::volume); quality = p (CxParams::quality);
}

complicator::Params ComplicatorAudioProcessor::readParams() const
{
    auto f = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    auto i = [] (const std::atomic<float>* a) { return juce::roundToInt (a->load (std::memory_order_relaxed)); };
    auto osc = [&] (const OscPtrs& o)
    {
        complicator::OscParams r;
        r.coarse = f (o.coarse); r.fine = f (o.fine); r.pwm = f (o.pwm); r.fm = f (o.fm); r.fmCurve = i (o.fmCurve);
        r.fold = f (o.fold); r.foldCv = f (o.foldCv); r.sym = f (o.sym);
        return r;
    };

    complicator::Params P;
    P.a = osc (pa);
    P.b = osc (pb);
    P.aLfo = f (aLfo) > 0.5f;
    P.bSub = i (bSub);
    P.keys = i (keys); P.snap = i (snap); P.scale = i (scale); P.key = i (key); P.sync = i (sync);
    P.multDepth = f (multDepth); P.multMode = i (multMode); P.index = f (index); P.indexCv = f (indexCv);
    P.routeFmA = f (routes[0]) > 0.5f; P.routeFmB = f (routes[1]) > 0.5f;
    P.routeFoldA = f (routes[2]) > 0.5f; P.routeFoldB = f (routes[3]) > 0.5f;
    P.routeSymA = f (routes[4]) > 0.5f; P.routeSymB = f (routes[5]) > 0.5f;
    P.routePwm = i (routes[6]);
    for (int k = 0; k < CxParams::numPatch; ++k) P.patch[k] = i (patch[k]);
    for (int k = 0; k < CxParams::numMix; ++k) P.mix[k] = f (mix[k]);
    P.envAttack = f (envA); P.envDecay = f (envD); P.envSustain = f (envS); P.envRelease = f (envR);
    P.ampMode = i (ampMode); P.volume = f (volume); P.quality = i (quality);
    return P;
}

namespace
{
    // The four controls a preset leaves alone: the player's own pitch and quality set-up.
    bool isPlayersSetting (const juce::String& id)
    {
        return id == CxParams::pitchSnap || id == CxParams::pitchScale || id == CxParams::pitchKey || id == CxParams::quality;
    }
}

void ComplicatorAudioProcessor::applyParams (const complicator::Params& P)
{
    namespace C = CxParams;       // (the member names keys, index, sync ... would hide the ids)
    auto set = [this] (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    auto flag = [] (bool b) { return b ? 1.0f : 0.0f; };

    auto osc = [&] (bool isA, const complicator::OscParams& o)
    {
        set (isA ? C::aCoarse : C::bCoarse, o.coarse);   set (isA ? C::aFine : C::bFine, o.fine);
        set (isA ? C::aPwm : C::bPwm, o.pwm);            set (isA ? C::aFm : C::bFm, o.fm);
        set (isA ? C::aFmCurve : C::bFmCurve, (float) o.fmCurve);
        set (isA ? C::aFold : C::bFold, o.fold);         set (isA ? C::aFoldCv : C::bFoldCv, o.foldCv);
        set (isA ? C::aSym : C::bSym, o.sym);
    };
    osc (true, P.a);
    osc (false, P.b);
    set (C::aLfo, flag (P.aLfo));  set (C::bSub, (float) P.bSub);
    set (C::keys, (float) P.keys); set (C::sync, (float) P.sync);
    set (C::multDepth, P.multDepth); set (C::multMode, (float) P.multMode);
    set (C::index, P.index);         set (C::indexCv, P.indexCv);
    set (C::routeFmA, flag (P.routeFmA));     set (C::routeFmB, flag (P.routeFmB));
    set (C::routeFoldA, flag (P.routeFoldA)); set (C::routeFoldB, flag (P.routeFoldB));
    set (C::routeSymA, flag (P.routeSymA));   set (C::routeSymB, flag (P.routeSymB));
    set (C::routePwm, (float) P.routePwm);
    for (int k = 0; k < C::numPatch; ++k) set (C::patchIds[k], (float) P.patch[k]);
    for (int k = 0; k < C::numMix; ++k)   set (C::mixIds[k], P.mix[k]);
    set (C::envAttack, P.envAttack);  set (C::envDecay, P.envDecay);
    set (C::envSustain, P.envSustain); set (C::envRelease, P.envRelease);
    set (C::ampMode, (float) P.ampMode);
    set (C::volume, P.volume);
}

void ComplicatorAudioProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, getNumPrograms() - 1, index);
    if (index == 0)
    {
        for (auto* base : getParameters())
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (base))
                if (! isPlayersSetting (p->paramID))
                    p->setValueNotifyingHost (p->getDefaultValue());
    }
    else
        applyParams (complicator::presets::factory (index - 1));
    programIndex = index;
    presetShown = index;
}

const juce::String ComplicatorAudioProcessor::getProgramName (int index)
{
    if (index == 0) return "Init";
    return complicator::presets::name (index - 1);
}

void ComplicatorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (juce::roundToInt (engine.latencySamples()));
    sidechainMono.assign ((size_t) juce::jmax (samplesPerBlock, 16384), 0.0f);
}

bool ComplicatorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void ComplicatorAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                          engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())                    engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())                 engine.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
    else if (m.isControllerOfType (1))         engine.modWheel ((float) m.getControllerValue() / 127.0f);
    else if (m.isChannelPressure())            engine.aftertouch ((float) m.getChannelPressureValue() / 127.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
}

void ComplicatorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    engine.setParams (readParams());

    auto out = getBusBuffer (buffer, false, 0);
    const int numCh = out.getNumChannels();
    const int n = buffer.getNumSamples();
    if (numCh == 0) { midi.clear(); return; }

    // Sidechain (Patch row source "Sidechain"): the input and output buffers share memory, so the sidechain is copied
    // out first. If Logic has not connected it (or the block is unusually long), the engine simply gets silence.
    const float* sidechain = nullptr;
    if (auto* scBus = getBus (true, 0); scBus != nullptr && scBus->isEnabled() && n <= (int) sidechainMono.size())
    {
        auto in = getBusBuffer (buffer, true, 0);
        if (in.getNumChannels() > 0)
        {
            const float* l = in.getReadPointer (0);
            const float* r = in.getReadPointer (in.getNumChannels() > 1 ? 1 : 0);
            for (int i = 0; i < n; ++i)
                sidechainMono[(size_t) i] = 0.5f * (l[i] + r[i]);
            sidechain = sidechainMono.data();
        }
    }

    float* chans[2] = { out.getWritePointer (0), out.getWritePointer (numCh > 1 ? 1 : 0) };
    const int useCh = numCh > 1 ? 2 : 1;

    // Render up to each MIDI event, then apply it, so notes start exactly where they are in the bar.
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos)
        {
            float* seg[2] = { chans[0] + pos, chans[1] + pos };
            engine.process (seg, useCh, at - pos, sidechain != nullptr ? sidechain + pos : nullptr);
            pos = at;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < n)
    {
        float* seg[2] = { chans[0] + pos, chans[1] + pos };
        engine.process (seg, useCh, n - pos, sidechain != nullptr ? sidechain + pos : nullptr);
    }
    for (int c = 2; c < numCh; ++c)
        out.clear (c, 0, n);
    midi.clear();
}

juce::AudioProcessorEditor* ComplicatorAudioProcessor::createEditor()
{
    return new ComplicatorAudioProcessorEditor (*this);
}

void ComplicatorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (presetShown.load() >= 0)
        state.setProperty ("preset", presetShown.load(), nullptr);       // which Preset menu entry was chosen last
    if (auto xml = state.createXml())
    {
        xml->setAttribute ("pluginVersion", JucePlugin_VersionString);
        copyXmlToBinary (*xml, destData);
    }
}

void ComplicatorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            // Projects saved before presets existed have no "preset" entry: the menu then shows "Custom".
            const int shown = tree.hasProperty ("preset") ? juce::jlimit (0, getNumPrograms() - 1, (int) tree["preset"]) : -1;
            presetShown = shown;
            programIndex = juce::jmax (0, shown);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ComplicatorAudioProcessor();
}
