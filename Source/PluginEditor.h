#pragma once
#include "PluginProcessor.h"

class GeistPadEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GeistPadEditor(GeistPadProcessor&);
    ~GeistPadEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    GeistPadProcessor& processor;
    juce::TextButton arm {"Arm"}, manual {"Record / Stop"};
    juce::ToggleButton monitor {"Monitor input"}, next {"Next pad"};
    juce::ComboBox mode;
    juce::Slider start, stop, hold;
    juce::Label startLabel, stopLabel, holdLabel, status;
    std::array<juce::TextButton, GeistPadProcessor::numPads> pads;
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeistPadEditor)
};
