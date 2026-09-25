/*
  ==============================================================================
    PluginProcessor.cpp — Molasses VST
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

// Parameter layout — defined once, shared by constructor and any serialisation
juce::AudioProcessorValueTreeState::ParameterLayout
MolassesVstAudioProcessor::createParameterLayout()
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
        juce::NormalisableRange<float> (1.0f, 16.0f, 1.0f),
        2.0f));

    // Reset interval: number of samples until the duplicate buffer state is reset.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "resetSamples", "Reset Samples",
        juce::NormalisableRange<float> (
            100.0f, 41000.0f,
            [] (float start, float end, float normalisedValue) {
                return start * std::pow (end / start, normalisedValue);
            },
            [] (float start, float end, float actualValue) {
                return std::log (actualValue / start) / std::log (end / start);
            }),
        1000.0f));

    return { params.begin(), params.end() };
}

MolassesVstAudioProcessor::MolassesVstAudioProcessor()
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

    // Listen for parameter changes so we can reset storage when sliders update.
    apvts.addParameterListener ("threshold", this);
    apvts.addParameterListener ("multiplier", this);
    apvts.addParameterListener ("resetSamples", this);
}

MolassesVstAudioProcessor::~MolassesVstAudioProcessor()
{
    apvts.removeParameterListener ("threshold", this);
    apvts.removeParameterListener ("multiplier", this);
    apvts.removeParameterListener ("resetSamples", this);
}

const juce::String MolassesVstAudioProcessor::getName() const { return "molasses2"; }

bool MolassesVstAudioProcessor::acceptsMidi()  const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MolassesVstAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MolassesVstAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MolassesVstAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  MolassesVstAudioProcessor::getNumPrograms()                              { return 1; }
int  MolassesVstAudioProcessor::getCurrentProgram()                           { return 0; }
void MolassesVstAudioProcessor::setCurrentProgram (int)                       {}
const juce::String MolassesVstAudioProcessor::getProgramName (int)            { return {}; }
void MolassesVstAudioProcessor::changeProgramName (int, const juce::String&)  {}

void MolassesVstAudioProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    // Nothing extra needed for the sample-and-hold algorithm.
}

void MolassesVstAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MolassesVstAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void MolassesVstAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    sampleHoldProcessor.processBlock (buffer);
}

bool MolassesVstAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* MolassesVstAudioProcessor::createEditor()
{
    return new MolassesVstAudioProcessorEditor (*this);
}

void MolassesVstAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Serialise the APVTS tree to XML so DAWs can save/restore presets.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MolassesVstAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MolassesVstAudioProcessor();
}

SampleHoldProcessor::SampleHoldProcessor (int numChannels)
{
    storage_vectors.resize(numChannels);
    for (auto& vec : storage_vectors) vec.reserve(10000000);
    samplesRemaining = 1;
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
    const int smoothResetSamples = 0;

    // if (clearRequested.exchange(false))
    // {
    //     samplesRemaining = 0;
    //     for (auto& vec : storage_vectors) vec.clear();
    //     clearDisplayData();
    // }

    appendDisplayData(buffer);

    bool storageReset = false;
    for (int i = 0; i < numSamples; ++i)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto& storage = storage_vectors[channel];

            if(storage.size() <= samplesRemaining)
            {
                float sample = buffer.getSample(channel, i);
                
                storage.push_back(sample);
            
                if (sample > threshold)
                {
                    for (int dup = 1; dup < multiplier; ++dup)
                    {
                        if(storage.size() > samplesRemaining){
                            break;
                        }
                        storage.push_back(sample);
                    }
                }
            }
        }

        samplesRemaining--;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto& storage = storage_vectors[channel];
            if (storage.empty())
            {
                //shouldn't happen, but just in case, set to 0
                buffer.setSample(channel, i, 0.0f);
            }
            else
            {
                buffer.setSample(channel, i, storage.front());
                storage.erase(storage.begin());
            }

            if (samplesRemaining <= 0)
            {
                storage.clear();
            }
        }

        if (samplesRemaining <= 0)
        {
            samplesRemaining = resetSamples;
            storageReset = true;
        }
    }

    if (storageReset)
    {
        clearDisplayData();
    }
    else
    {
        appendProcessedDisplayData(buffer);
    }
}

// Parameter change listener — called on message thread
void MolassesVstAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    //sampleHoldProcessor.requestClearStorage();
}