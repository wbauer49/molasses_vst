/*
  ==============================================================================
    PluginEditor.h — Chunker VST
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
    A simple labelled vertical slider component used for Threshold.
*/
class LabelledSlider : public juce::Component
{
public:
    juce::Slider slider;
    juce::Label  valueLabel;
    juce::Label  label;

    explicit LabelledSlider (const juce::String& labelText)
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (slider);

        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setFont (juce::Font (13.0f));
        addAndMakeVisible (valueLabel);

        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (13.0f));
        addAndMakeVisible (label);

        updateDisplay();
        slider.onValueChange = [this] { updateDisplay(); };
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto textArea = area.removeFromBottom (44);
        valueLabel.setBounds (textArea.removeFromTop (22));
        label.setBounds (textArea);
        slider.setBounds (area);
    }

private:
    void updateDisplay()
    {
        const double value = slider.getValue();
        if (slider.getInterval() >= 1.0)
            valueLabel.setText (juce::String (int (std::round (value))), juce::dontSendNotification);
        else
            valueLabel.setText (juce::String (value, 2), juce::dontSendNotification);
    }
};

//==============================================================================
class ChunkerVstAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChunkerVstAudioProcessorEditor (ChunkerVstAudioProcessor&);
    ~ChunkerVstAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    ChunkerVstAudioProcessor& audioProcessor;

    // Controls
    LabelledSlider thresholdSlider    { "Threshold" };
    LabelledSlider multiplierSlider   { "Multiplier" };
    LabelledSlider resetSamplesSlider { "Reset Samples" };

    // APVTS attachments — keep these alive for the lifetime of the editor
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> multiplierAttachment;
    std::unique_ptr<SliderAttachment> resetSamplesAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChunkerVstAudioProcessorEditor)
};