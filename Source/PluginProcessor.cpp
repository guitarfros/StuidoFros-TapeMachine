#include "PluginProcessor.h"
#include "PluginEditor.h"

TapeMachineProcessor::TapeMachineProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", Parameters::createLayout())
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    pPower = raw (ParamID::power);        pBypass = raw (ParamID::bypass);
    pInput = raw (ParamID::input);        pDrive = raw (ParamID::drive);
    pSpeed = raw (ParamID::speed);        pType = raw (ParamID::tapeType);
    pEq = raw (ParamID::eq);              pBias = raw (ParamID::bias);
    pSat = raw (ParamID::saturation);     pBump = raw (ParamID::headBump);
    pTone = raw (ParamID::tone);          pLoss = raw (ParamID::loss);
    pComp = raw (ParamID::compression);   pWow = raw (ParamID::wow);
    pFlutter = raw (ParamID::flutter);    pDrift = raw (ParamID::drift);
    pDegrade = raw (ParamID::degrade);    pHiss = raw (ParamID::hiss);
    pHissOn = raw (ParamID::hissOn);      pLink = raw (ParamID::link);
    pOutput = raw (ParamID::output);      pMix = raw (ParamID::mix);
}

bool TapeMachineProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void TapeMachineProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()));
    setLatencySamples (engine.getLatencySamples());
}

tape::Settings TapeMachineProcessor::readSettings() const
{
    tape::Settings s;
    s.power = pPower->load() > 0.5f;
    s.bypass = pBypass->load() > 0.5f;
    s.inputDb = pInput->load();
    s.outputDb = pOutput->load();
    s.drive = pDrive->load();
    s.speed = (int) pSpeed->load();
    s.type = (int) pType->load();
    s.eq = (int) pEq->load();
    s.bias = pBias->load();
    s.saturation = pSat->load();
    s.headBump = pBump->load();
    s.tone = pTone->load();
    s.loss = pLoss->load();
    s.compression = pComp->load();
    s.wow = pWow->load();
    s.flutter = pFlutter->load();
    s.drift = pDrift->load();
    s.degrade = pDegrade->load();
    s.hiss = pHiss->load();
    s.hissOn = pHissOn->load() > 0.5f;
    s.link = pLink->load() > 0.5f;
    s.mix = pMix->load();
    return s;
}

void TapeMachineProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const auto m = engine.process (buffer, readSettings());

    inRms.store (m.inRms);
    outRms.store (m.outRms);

    auto raisePeak = [] (std::atomic<float>& a, float v)
    {
        float prev = a.load();
        while (v > prev && ! a.compare_exchange_weak (prev, v)) {}
    };
    raisePeak (inPeak, m.inPeak);
    raisePeak (outPeak, m.outPeak);
}

juce::AudioProcessorEditor* TapeMachineProcessor::createEditor()
{
    return new TapeMachineEditor (*this);
}

void TapeMachineProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TapeMachineProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeMachineProcessor();
}
