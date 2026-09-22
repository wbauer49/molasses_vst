/*
  ==============================================================================
    PluginEditor.h — Molasses VST
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
        valueLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
        addAndMakeVisible (valueLabel);

        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (13.0f)));
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
class SampleWaveformDisplay : public juce::Component,
                             private juce::Timer
{
public:
    explicit SampleWaveformDisplay (std::function<std::vector<float>()> sampleSource)
        : sampleSource (std::move (sampleSource))
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto plotBounds = getLocalBounds().reduced (8);
        g.fillAll (juce::Colour (0xFF'1A1C2A));
        g.setColour (juce::Colour (0xFF'2B3555));
        g.drawRect (plotBounds, 1);

        const auto samples = sampleSource();
        if (samples.empty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.setFont (juce::Font (12.0f));
            g.drawText ("Waiting for samples", plotBounds, juce::Justification::centred, true);
            return;
        }

        auto minMax = std::minmax_element (samples.begin(), samples.end());
        const float minValue = *minMax.first;
        const float maxValue = *minMax.second;
        const float range = std::max (1.0e-5f, maxValue - minValue);

        const float left = (float) plotBounds.getX();
        const float top = (float) plotBounds.getY();
        const float right = (float) plotBounds.getRight();
        const float bottom = (float) plotBounds.getBottom();

        g.setColour (juce::Colour (0xFF'7E88C8));
        g.drawLine (left, top, left, bottom);
        g.drawLine (left, bottom, right, bottom);

        const float zeroY = juce::jmap (0.0f, minValue, maxValue, bottom, top);
        if (minValue <= 0.0f && maxValue >= 0.0f)
        {
            g.setColour (juce::Colour (0xFF'9AA7D1));
            g.drawHorizontalLine ((int) std::round (zeroY), (int) std::round (left), (int) std::round (right));
        }

        g.setColour (juce::Colour (0xFF'63D2FF));
        juce::Path waveform;
        for (size_t i = 0; i < samples.size(); ++i)
        {
            const float x = left + ((float) i / (float) std::max (samples.size() - 1, size_t (1))) * (float) plotBounds.getWidth();
            const float y = bottom - ((samples[i] - minValue) / range) * (float) plotBounds.getHeight();

            if (i == 0)
                waveform.startNewSubPath (x, y);
            else
                waveform.lineTo (x, y);
        }

        g.strokePath (waveform, juce::PathStrokeType (1.6f));

        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::Font (11.0f));
        g.drawText (juce::String (maxValue, 2), juce::Rectangle<float> (left - 4.0f, top - 2.0f, 30.0f, 12.0f), juce::Justification::right, true);
        g.drawText (juce::String (minValue, 2), juce::Rectangle<float> (left - 4.0f, bottom - 12.0f, 30.0f, 12.0f), juce::Justification::right, true);
        g.drawText ("sample", juce::Rectangle<float> (left + 4.0f, top - 18.0f, 80.0f, 12.0f), juce::Justification::left, true);
    }

    void timerCallback() override
    {
        repaint();
    }

private:
    std::function<std::vector<float>()> sampleSource;
};

class MolassesVstAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MolassesVstAudioProcessorEditor (MolassesVstAudioProcessor&);
    ~MolassesVstAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    MolassesVstAudioProcessor& audioProcessor;

    // Controls
    LabelledSlider thresholdSlider    { "Threshold" };
    LabelledSlider multiplierSlider   { "Multiplier" };
    LabelledSlider resetSamplesSlider { "Reset Samples" };

    SampleWaveformDisplay sampleGraph { [this] { return audioProcessor.getLatestSampleDisplay(); } };

    // APVTS attachments — keep these alive for the lifetime of the editor
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> multiplierAttachment;
    std::unique_ptr<SliderAttachment> resetSamplesAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MolassesVstAudioProcessorEditor)
};