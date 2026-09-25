/*
  ==============================================================================
    PluginProcessor.h — Molasses VST
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <mutex>
#include <vector>

/**
    Sample duplication processor that repeats any sample whose absolute
    amplitude exceeds the threshold, producing a simple sample-length doubling
    effect when the threshold is hit.
*/
class SampleHoldProcessor
{
public:
    SampleHoldProcessor(int numChannels = 2);

    void setThresholdParameter   (std::atomic<float>* param) { thresholdParam   = param; }
    void setMultiplierParameter  (std::atomic<float>* param) { multiplierParam  = param; }
    void setResetSamplesParameter(std::atomic<float>* param) { resetSamplesParam = param; }

    void processBlock (juce::AudioBuffer<float>& buffer);
    void requestClearStorage() noexcept { clearRequested.store(true); }
    void clearDisplayData()
    {
        const std::lock_guard<std::mutex> lock (displayMutex);
        displaySamples.clear();
        displayProcessedSamples.clear();
        displayDirty.store (true);
    }

    int getDisplayCaptureLimit() const
    {
        const int resetSamples = std::max (1, int (std::round (resetSamplesParam ? resetSamplesParam->load() : 1000.0f)));
        return std::max (1, int (std::round (resetSamples * 0.10f)));
    }

    void appendDisplayData (const juce::AudioBuffer<float>& buffer)
    {
        const std::lock_guard<std::mutex> lock (displayMutex);
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0 || buffer.getNumChannels() <= 0)
            return;

        const int captureLimit = getDisplayCaptureLimit();
        if ((int) displaySamples.size() >= captureLimit)
            return;

        const auto* channelData = buffer.getReadPointer (0);
        const int samplesToAppend = std::min (numSamples, captureLimit - (int) displaySamples.size());
        displaySamples.insert (displaySamples.end(), channelData, channelData + samplesToAppend);
        displayDirty.store (true);
    }

    void appendProcessedDisplayData (const juce::AudioBuffer<float>& buffer)
    {
        const std::lock_guard<std::mutex> lock (displayMutex);
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0 || buffer.getNumChannels() <= 0)
            return;

        const int captureLimit = getDisplayCaptureLimit();
        if ((int) displayProcessedSamples.size() >= captureLimit)
            return;

        const auto* channelData = buffer.getReadPointer (0);
        const int samplesToAppend = std::min (numSamples, captureLimit - (int) displayProcessedSamples.size());
        displayProcessedSamples.insert (displayProcessedSamples.end(), channelData, channelData + samplesToAppend);
        displayDirty.store (true);
    }

    std::vector<float> getDisplayData() const
    {
        const std::lock_guard<std::mutex> lock (displayMutex);
        return displaySamples;
    }

    std::vector<float> getProcessedDisplayData() const
    {
        const std::lock_guard<std::mutex> lock (displayMutex);
        return displayProcessedSamples;
    }

    float getThresholdValue() const
    {
        return thresholdParam ? thresholdParam->load() : 0.0f;
    }

    bool hasDisplayChanged() const
    {
        return displayDirty.load();
    }

    void acknowledgeDisplayChange()
    {
        displayDirty.store (false);
    }

private:
    std::atomic<float>* thresholdParam    = nullptr;
    std::atomic<float>* multiplierParam   = nullptr;
    std::atomic<float>* resetSamplesParam = nullptr;

    std::vector<std::vector<float>> storage_vectors;
    int samplesRemaining = 0;
    std::atomic<bool> clearRequested{false};

    mutable std::mutex displayMutex;
    std::vector<float> displaySamples;
    std::vector<float> displayProcessedSamples;
    std::atomic<bool> displayDirty { false };
};

/**
    Main audio processor for the Molasses plugin.
*/
class MolassesVstAudioProcessor : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    MolassesVstAudioProcessor();
    ~MolassesVstAudioProcessor() override;

    // AudioProcessorValueTreeState::Listener
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi()  const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName  (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    std::vector<float> getLatestSampleDisplay() const
    {
        return sampleHoldProcessor.getDisplayData();
    }

    std::vector<float> getLatestProcessedSampleDisplay() const
    {
        return sampleHoldProcessor.getProcessedDisplayData();
    }

    float getCurrentThreshold() const
    {
        return sampleHoldProcessor.getThresholdValue();
    }

    bool hasDisplayChanged() const
    {
        return sampleHoldProcessor.hasDisplayChanged();
    }

    void acknowledgeDisplayChange()
    {
        sampleHoldProcessor.acknowledgeDisplayChange();
    }

    // APVTS — public so the editor can attach sliders/buttons directly
    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    SampleHoldProcessor sampleHoldProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MolassesVstAudioProcessor)
};