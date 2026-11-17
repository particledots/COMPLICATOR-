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

void ComplicatorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (juce::roundToInt (engine.latencySamples()));
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
            engine.process (seg, useCh, at - pos);
            pos = at;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < n)
    {
        float* seg[2] = { chans[0] + pos, chans[1] + pos };
        engine.process (seg, useCh, n - pos);
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
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("pluginVersion", JucePlugin_VersionString);
        copyXmlToBinary (*xml, destData);
    }
}

void ComplicatorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ComplicatorAudioProcessor();
}
