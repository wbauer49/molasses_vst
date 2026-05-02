/*
  ==============================================================================
    PluginProcessor.cpp — Chunker VST
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Parameter layout — defined once, shared by constructor and any serialisation
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
ChunkerVstAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Threshold: -1.0 → 1.0, duplicates any sample whose absolute amplitude exceeds
    // the threshold magnitude.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "threshold", "Threshold",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f),
        0.5f));

    // Multiplier: 1 = no duplication, 2 = duplicate each triggered sample once, etc.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "multiplier", "Multiplier",
        juce::NormalisableRange<float> (1.0f, 8.0f, 1.0f),
        2.0f));

    // Reset interval: number of samples until the duplicate buffer state is reset.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "resetSamples", "Reset Samples",
        juce::NormalisableRange<float> (
            1.0f, 100000.0f,
            [] (float start, float end, float normalisedValue) {
                return start * std::pow (end / start, normalisedValue);
            },
            [] (float start, float end, float actualValue) {
                return std::log (actualValue / start) / std::log (end / start);
            }),
        1000.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
ChunkerVstAudioProcessor::ChunkerVstAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                      ),
#else
    :
#endif
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    // Hand raw atomic pointers from APVTS to the DSP class.
    // getRawParameterValue() returns a pointer to the internal std::atomic<float>
    // that is updated on the message thread but safely read on the audio thread.
    sampleHoldProcessor.setThresholdParameter   (apvts.getRawParameterValue ("threshold"));
    sampleHoldProcessor.setMultiplierParameter  (apvts.getRawParameterValue ("multiplier"));
    sampleHoldProcessor.setResetSamplesParameter (apvts.getRawParameterValue ("resetSamples"));
}

ChunkerVstAudioProcessor::~ChunkerVstAudioProcessor() {}

//==============================================================================
const juce::String ChunkerVstAudioProcessor::getName() const { return "CHUNKER"; }

bool ChunkerVstAudioProcessor::acceptsMidi()  const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ChunkerVstAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ChunkerVstAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ChunkerVstAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  ChunkerVstAudioProcessor::getNumPrograms()                              { return 1; }
int  ChunkerVstAudioProcessor::getCurrentProgram()                           { return 0; }
void ChunkerVstAudioProcessor::setCurrentProgram (int)                       {}
const juce::String ChunkerVstAudioProcessor::getProgramName (int)            { return {}; }
void ChunkerVstAudioProcessor::changeProgramName (int, const juce::String&)  {}

//==============================================================================
void ChunkerVstAudioProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    // Nothing extra needed for the sample-and-hold algorithm.
}

void ChunkerVstAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ChunkerVstAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ChunkerVstAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    sampleHoldProcessor.processBlock (buffer);
}

//==============================================================================
bool ChunkerVstAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ChunkerVstAudioProcessor::createEditor()
{
    return new ChunkerVstAudioProcessorEditor (*this);
}

//==============================================================================
void ChunkerVstAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Serialise the APVTS tree to XML so DAWs can save/restore presets.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ChunkerVstAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChunkerVstAudioProcessor();
}

//==============================================================================
// SampleHoldProcessor
//==============================================================================
SampleHoldProcessor::SampleHoldProcessor (int numChannels)
{
    storage_vectors.resize(numChannels);
    for (auto& vec : storage_vectors) vec.reserve(100000);
    thresholdCrossed.assign(numChannels, false);
}

void SampleHoldProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (!thresholdParam || !multiplierParam || !resetSamplesParam)
        return;

    const float threshold = thresholdParam->load();
    const int multiplier  = juce::jlimit (1, 16, int (std::round (multiplierParam->load())));
    const int resetSamples = std::max (1, int (std::round (resetSamplesParam->load())));
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    if (numChannels != (int) storage_vectors.size())
    {
        storage_vectors.resize(numChannels);
        for (auto& vec : storage_vectors) vec.reserve(100000);
        thresholdCrossed.assign(numChannels, false);
    }

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto& storage = storage_vectors[channel];
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = buffer.getSample(channel, i);
            storage.push_back(sample);

            if (sample > threshold && !thresholdCrossed[channel])
            {
                thresholdCrossed[channel] = true;
                for (int dup = 1; dup < multiplier; ++dup)
                {
                    storage.push_back(sample);
                }
            }
            else if (sample <= threshold)
            {
                thresholdCrossed[channel] = false;
            }

            if (storage.size() >= resetSamples)
            {
                storage.clear();
            }
        }

        // Output from storage
        for (int i = 0; i < numSamples; ++i)
        {
            if (!storage.empty())
            {
                buffer.setSample(channel, i, storage.front());
                storage.erase(storage.begin());
            }
            else
            {
                buffer.setSample(channel, i, 0.0f);
            }
        }
    }
}