/*
  ==============================================================================
    PluginProcessor.h — Chunker VST
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>

//==============================================================================
/**
    Sample and hold processor that latches incoming audio values when they
    exceed a threshold, then outputs the held value until a new threshold
    crossing occurs.
*/
class SampleHoldProcessor
{
public:
    SampleHoldProcessor(int numChannels = 2);

    void setThresholdParameter (std::atomic<float>* param) { thresholdParam = param; }
    void setMixParameter       (std::atomic<float>* param) { mixParam        = param; }
    void setFreezeParameter    (std::atomic<float>* param) { freezeParam     = param; }

    void processBlock (juce::AudioBuffer<float>& buffer);

private:
    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* mixParam       = nullptr;
    std::atomic<float>* freezeParam    = nullptr;

    juce::Array<float> holdRegister;
};

//==============================================================================
/**
    Main audio processor for the Chunker plugin.
*/
class ChunkerVstAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    ChunkerVstAudioProcessor();
    ~ChunkerVstAudioProcessor() override;

    //==============================================================================
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi()  const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName  (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // APVTS — public so the editor can attach sliders/buttons directly
    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    //==============================================================================
    SampleHoldProcessor sampleHoldProcessor;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChunkerVstAudioProcessor)
};