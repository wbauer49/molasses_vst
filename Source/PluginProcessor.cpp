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

    // Threshold: 0.0 (latch everything) → 1.0 (latch nothing below full scale)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "threshold", "Threshold",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        0.5f));

    // Dry/wet mix: 0.0 = fully dry, 1.0 = fully processed
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "mix", "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        1.0f));

    // Freeze: when engaged, stop updating the hold register (hold forever)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "freeze", "Freeze",
        juce::NormalisableRange<float> (0.0f, 1.0f, 1.0f),   // 0 or 1 only
        0.0f));

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
    sampleHoldProcessor.setThresholdParameter (apvts.getRawParameterValue ("threshold"));
    sampleHoldProcessor.setMixParameter       (apvts.getRawParameterValue ("mix"));
    sampleHoldProcessor.setFreezeParameter    (apvts.getRawParameterValue ("freeze"));
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
    holdRegister.resize (numChannels);
    holdRegister.fill (0.0f);
}

void SampleHoldProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (!thresholdParam)
        return;

    const float threshold = thresholdParam->load();
    const float mix       = mixParam       ? mixParam->load()    : 1.0f;
    const bool  freeze    = freezeParam    ? (freezeParam->load() >= 0.5f) : false;

    // Ensure hold register is large enough if channel count changes at runtime
    if (buffer.getNumChannels() > holdRegister.size())
    {
        holdRegister.resize (buffer.getNumChannels());
        holdRegister.fill (0.0f);
    }

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* samples   = buffer.getWritePointer (channel);
        float  heldValue = holdRegister[channel];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float dry = samples[i];

            // Only update the latch if freeze is off
            if (!freeze && std::abs (dry) >= threshold)
                heldValue = dry;

            // Blend dry and processed signals
            samples[i] = dry + mix * (heldValue - dry);
        }

        holdRegister.set (channel, heldValue);
    }
}