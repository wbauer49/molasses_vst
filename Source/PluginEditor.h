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
    A simple labelled rotary knob component used for Threshold and Mix.
*/
class LabelledKnob : public juce::Component
{
public:
    juce::Slider slider;
    juce::Label  label;

    explicit LabelledKnob (const juce::String& labelText)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
        addAndMakeVisible (slider);

        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (13.0f));
        addAndMakeVisible (label);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds  (area.removeFromBottom (20));
        slider.setBounds (area);
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
    LabelledKnob thresholdKnob { "Threshold" };
    LabelledKnob mixKnob       { "Mix"       };

    juce::TextButton freezeButton { "FREEZE" };

    // APVTS attachments — keep these alive for the lifetime of the editor
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<ButtonAttachment> freezeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChunkerVstAudioProcessorEditor)
};