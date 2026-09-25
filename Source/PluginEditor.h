/*
  ==============================================================================
    PluginEditor.h — Molasses VST
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

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
        slider.setColour (juce::Slider::backgroundColourId, juce::Colours::black);
        slider.setColour (juce::Slider::trackColourId, juce::Colours::white);
        slider.setColour (juce::Slider::thumbColourId, juce::Colours::grey);
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

class SampleWaveformDisplay : public juce::Component,
                             private juce::Timer
{
public:
    SampleWaveformDisplay (std::function<std::vector<float>()> beforeSource,
                           std::function<std::vector<float>()> afterSource,
                           std::function<float()> thresholdSource,
                           std::function<bool()> displayDirtySource = {},
                           std::function<void()> acknowledgeDirty = {})
        : beforeSource (std::move (beforeSource)),
          afterSource (std::move (afterSource)),
          thresholdSource (std::move (thresholdSource)),
          displayDirtySource (std::move (displayDirtySource)),
          acknowledgeDirty (std::move (acknowledgeDirty))
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto plotBounds = getLocalBounds().reduced (8);
        g.fillAll (juce::Colour (0xFF'1A1C2A));
        g.setColour (juce::Colour (0xFF'2B3555));
        g.drawRect (plotBounds, 1);

        const auto beforeSamples = beforeSource();
        const auto afterSamples = afterSource();
        const float thresholdValue = thresholdSource();

        std::vector<float> allSamples = beforeSamples;
        allSamples.insert (allSamples.end(), afterSamples.begin(), afterSamples.end());

        if (allSamples.empty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.setFont (juce::Font (12.0f));
            g.drawText ("Waiting for samples", plotBounds, juce::Justification::centred, true);
            return;
        }

        constexpr float minValue = -1.0f;
        constexpr float maxValue = 1.0f;
        constexpr float range = maxValue - minValue;

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

        const float thresholdY = juce::jmap (thresholdValue, minValue, maxValue, bottom, top);
        g.setColour (juce::Colour (0xFF'FFB870));
        g.drawHorizontalLine ((int) std::round (thresholdY), (int) std::round (left), (int) std::round (right));

        auto drawCurve = [&] (const std::vector<float>& samples, const juce::Colour& colour, float strokeWidth)
        {
            if (samples.empty())
                return;

            juce::Path curve;
            for (size_t i = 0; i < samples.size(); ++i)
            {
                const float x = left + ((float) i / (float) std::max (samples.size() - 1, size_t (1))) * (float) plotBounds.getWidth();
                const float y = bottom - ((samples[i] - minValue) / range) * (float) plotBounds.getHeight();

                if (i == 0)
                    curve.startNewSubPath (x, y);
                else
                    curve.lineTo (x, y);
            }

            g.setColour (colour);
            g.strokePath (curve, juce::PathStrokeType (strokeWidth));
        };

        drawCurve (beforeSamples, juce::Colour (0xFF'63D2FF), 1.5f);
        drawCurve (afterSamples, juce::Colour (0xFF'7AE7A3), 1.5f);

        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::Font (11.0f));
        g.drawText (juce::String (maxValue, 2), juce::Rectangle<float> (left - 4.0f, top - 2.0f, 30.0f, 12.0f), juce::Justification::right, true);
        g.drawText (juce::String (minValue, 2), juce::Rectangle<float> (left - 4.0f, bottom - 12.0f, 30.0f, 12.0f), juce::Justification::right, true);
        g.drawText ("before", juce::Rectangle<float> (left + 4.0f, top - 18.0f, 80.0f, 12.0f), juce::Justification::left, true);
        g.drawText ("after", juce::Rectangle<float> (left + 62.0f, top - 18.0f, 80.0f, 12.0f), juce::Justification::left, true);
        g.drawText ("threshold", juce::Rectangle<float> (left + 120.0f, top - 18.0f, 90.0f, 12.0f), juce::Justification::left, true);
    }

    void timerCallback() override
    {
        if (displayDirtySource && displayDirtySource())
        {
            repaint();
            if (acknowledgeDirty)
                acknowledgeDirty();
        }
    }

private:
    std::function<std::vector<float>()> beforeSource;
    std::function<std::vector<float>()> afterSource;
    std::function<float()> thresholdSource;
    std::function<bool()> displayDirtySource;
    std::function<void()> acknowledgeDirty;
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

    SampleWaveformDisplay sampleGraph {
        [this] { return audioProcessor.getLatestSampleDisplay(); },
        [this] { return audioProcessor.getLatestProcessedSampleDisplay(); },
        [this] { return audioProcessor.getCurrentThreshold(); },
        [this] { return audioProcessor.hasDisplayChanged(); },
        [this] { audioProcessor.acknowledgeDisplayChange(); }
    };

    // APVTS attachments — keep these alive for the lifetime of the editor
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> multiplierAttachment;
    std::unique_ptr<SliderAttachment> resetSamplesAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MolassesVstAudioProcessorEditor)
};