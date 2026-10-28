#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Parameters.h"
#include "TapeEngine.h"

class TapeMachineProcessor : public juce::AudioProcessor
{
public:
    TapeMachineProcessor();
    ~TapeMachineProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Meter values published for the editor (updated once per audio block)
    std::atomic<float> inRms { 0.0f }, outRms { 0.0f }, inPeak { 0.0f }, outPeak { 0.0f };

private:
    tape::Settings readSettings() const;

    tape::Engine engine;

    std::atomic<float>* pPower = nullptr;   std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pInput = nullptr;   std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pSpeed = nullptr;   std::atomic<float>* pType = nullptr;
    std::atomic<float>* pEq = nullptr;      std::atomic<float>* pBias = nullptr;
    std::atomic<float>* pSat = nullptr;     std::atomic<float>* pBump = nullptr;
    std::atomic<float>* pTone = nullptr;    std::atomic<float>* pLoss = nullptr;
    std::atomic<float>* pComp = nullptr;    std::atomic<float>* pWow = nullptr;
    std::atomic<float>* pFlutter = nullptr; std::atomic<float>* pDrift = nullptr;
    std::atomic<float>* pDegrade = nullptr; std::atomic<float>* pHiss = nullptr;
    std::atomic<float>* pHissOn = nullptr;  std::atomic<float>* pLink = nullptr;
    std::atomic<float>* pOutput = nullptr;  std::atomic<float>* pMix = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeMachineProcessor)
};
