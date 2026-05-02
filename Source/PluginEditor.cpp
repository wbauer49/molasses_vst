/*
  ==============================================================================
    PluginEditor.cpp — Chunker VST
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChunkerVstAudioProcessorEditor::ChunkerVstAudioProcessorEditor (ChunkerVstAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // --- Sliders ---
    thresholdSlider.slider.setRange (-1.0, 1.0, 0.001);
    multiplierSlider.slider.setRange (1.0, 16.0, 1.0);
    resetSamplesSlider.slider.setRange (1.0, 100000.0, 1.0);

    addAndMakeVisible (thresholdSlider);
    addAndMakeVisible (multiplierSlider);
    addAndMakeVisible (resetSamplesSlider);

    // --- APVTS attachments (must be created after controls are added) ---
    thresholdAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts, "threshold", thresholdSlider.slider);

    multiplierAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts, "multiplier", multiplierSlider.slider);

    resetSamplesAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts, "resetSamples", resetSamplesSlider.slider);

    setSize (620, 260);
}

ChunkerVstAudioProcessorEditor::~ChunkerVstAudioProcessorEditor() {}

//==============================================================================
void ChunkerVstAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Dark background
    g.fillAll (juce::Colour (0xFF'1E1E2E));

    // Title
    g.setColour (juce::Colours::white);
    g.setFont   (juce::Font (juce::FontOptions (22.0f).withTypefaceStyle ("Bold")));
    g.drawText  ("CHUNKER", getLocalBounds().removeFromTop (50),
                 juce::Justification::centred, false);

    // Subtle divider under title
    g.setColour (juce::Colour (0xFF'3A3A5C));
    g.drawHorizontalLine (48, 20.0f, (float) getWidth() - 20.0f);
}

void ChunkerVstAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);

    // Reserve top strip for the title painted in paint()
    area.removeFromTop (36);

    // Row 1: three sliders side by side
    auto sliderRow = area.removeFromTop (150);
    const int sliderW = sliderRow.getWidth() / 3;
    thresholdSlider.setBounds    (sliderRow.removeFromLeft (sliderW).reduced (8));
    multiplierSlider.setBounds   (sliderRow.removeFromLeft (sliderW).reduced (8));
    resetSamplesSlider.setBounds (sliderRow.reduced (8));

}
