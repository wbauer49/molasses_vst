/*
  ==============================================================================
    PluginEditor.cpp — Molasses VST
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MolassesVstAudioProcessorEditor::MolassesVstAudioProcessorEditor (MolassesVstAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // --- Sliders ---
    thresholdSlider.slider.setRange (-1.0, 1.0, 0.001);
    multiplierSlider.slider.setRange (1.0, 16.0, 1.0);
    resetSamplesSlider.slider.setRange (1.0, 100000.0, 1.0);

    addAndMakeVisible (thresholdSlider);
    addAndMakeVisible (multiplierSlider);
    addAndMakeVisible (resetSamplesSlider);
    addAndMakeVisible (sampleGraph);

    // --- APVTS attachments (must be created after controls are added) ---
    thresholdAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts, "threshold", thresholdSlider.slider);

    multiplierAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts, "multiplier", multiplierSlider.slider);

    resetSamplesAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts, "resetSamples", resetSamplesSlider.slider);

    setSize (620, 720);
}

MolassesVstAudioProcessorEditor::~MolassesVstAudioProcessorEditor() {}

//==============================================================================
void MolassesVstAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Dark background
    g.fillAll (juce::Colour (0xFF'1E1E2E));

    // Title
    g.setColour (juce::Colours::white);
    g.setFont   (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
    g.drawText  ("molasses2", getLocalBounds().removeFromTop (50),
                 juce::Justification::centred, false);

    // Subtle divider under title
    g.setColour (juce::Colour (0xFF'3A3A5C));
    g.drawHorizontalLine (48, 20.0f, (float) getWidth() - 20.0f);
}

void MolassesVstAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);

    // Reserve top strip for the title painted in paint()
    area.removeFromTop (36);

    // Keep the controls tall but leave room for the graph below
    auto sliderRow = area.removeFromTop (220);
    const int sliderW = sliderRow.getWidth() / 3;
    thresholdSlider.setBounds    (sliderRow.removeFromLeft (sliderW).reduced (8));
    multiplierSlider.setBounds   (sliderRow.removeFromLeft (sliderW).reduced (8));
    resetSamplesSlider.setBounds (sliderRow.reduced (8));

    auto graphArea = area;
    sampleGraph.setBounds (graphArea.reduced (8));
}
