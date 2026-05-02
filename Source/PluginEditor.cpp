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
    // --- Knobs ---
    addAndMakeVisible (thresholdKnob);
    addAndMakeVisible (mixKnob);

    // --- Freeze button ---
    freezeButton.setClickingTogglesState (true);
    freezeButton.setColour (juce::TextButton::buttonOnColourId,
                            juce::Colour (0xFF'E45C2B));   // warm orange when active
    addAndMakeVisible (freezeButton);

    // --- APVTS attachments (must be created after controls are added) ---
    thresholdAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts, "threshold", thresholdKnob.slider);

    mixAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts, "mix", mixKnob.slider);

    freezeAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.apvts, "freeze", freezeButton);

    setSize (380, 240);
}

ChunkerVstAudioProcessorEditor::~ChunkerVstAudioProcessorEditor() {}

//==============================================================================
void ChunkerVstAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Dark background
    g.fillAll (juce::Colour (0xFF'1E1E2E));

    // Title
    g.setColour (juce::Colours::white);
    g.setFont   (juce::Font (22.0f, juce::Font::bold));
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

    // Row 1: two knobs side by side
    auto knobRow = area.removeFromTop (130);
    const int knobW = knobRow.getWidth() / 2;
    thresholdKnob.setBounds (knobRow.removeFromLeft (knobW).reduced (8));
    mixKnob.setBounds       (knobRow.reduced (8));

    // Row 2: Freeze button centred
    area.removeFromTop (8);
    const int btnW = 110, btnH = 36;
    freezeButton.setBounds (
        area.getCentreX() - btnW / 2,
        area.getY(),
        btnW, btnH);
}