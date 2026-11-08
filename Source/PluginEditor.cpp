#include "PluginEditor.h"

GeistPadEditor::GeistPadEditor(GeistPadProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(640, 460);
    auto configure = [this](juce::Slider& s, juce::Label& label,
                            const juce::String& title, double minimum,
                            double maximum, double step, double current)
    {
        addAndMakeVisible(label);
        label.setText(title, juce::dontSendNotification);
        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 24);
        s.setRange(minimum, maximum, step);
        s.setValue(current, juce::dontSendNotification);
    };
    configure(start, startLabel, "Start dBFS", -60, 0, 1, processor.startDb.load());
    configure(stop, stopLabel, "Stop dBFS", -80, -1, 1, processor.stopDb.load());
    configure(hold, holdLabel, "Silence hold ms", 30, 1500, 10, processor.holdMs.load());
    start.onValueChange = [this] { processor.startDb.store(float(start.getValue())); };
    stop.onValueChange = [this] { processor.stopDb.store(float(stop.getValue())); };
    hold.onValueChange = [this] { processor.holdMs.store(int(hold.getValue())); };
    addAndMakeVisible(arm);
    addAndMakeVisible(manual);
    addAndMakeVisible(monitor);
    addAndMakeVisible(next);
    addAndMakeVisible(mode);
    addAndMakeVisible(status);
    arm.onClick = [this] { processor.armed.store(!processor.armed.load()); };
    manual.onClick = [this] { processor.manualToggle.store(true); };
    monitor.setToggleState(processor.monitor.load(), juce::dontSendNotification);
    monitor.onClick = [this] { processor.monitor.store(monitor.getToggleState()); };
    next.setToggleState(processor.nextPad.load(), juce::dontSendNotification);
    next.onClick = [this] { processor.nextPad.store(next.getToggleState()); };
    mode.addItem("Threshold", 1);
    mode.addItem("Immediate", 2);
    mode.setSelectedId(processor.triggerMode.load() + 1, juce::dontSendNotification);
    mode.onChange = [this] { processor.triggerMode.store(mode.getSelectedId() - 1); };
    for (int i = 0; i < GeistPadProcessor::numPads; ++i) {
        auto& b = pads[size_t(i)];
        addAndMakeVisible(b);
        b.onClick = [this, i] {
            processor.selected.store(i);
            processor.audition.store(i);
        };
    }
    startTimerHz(8);
}

void GeistPadEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(20, 24, 29));
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(23.0f, juce::Font::bold)));
    g.drawText("GEISTPAD  /  TRIGGER SAMPLER", 18, 10, 590, 34, juce::Justification::centredLeft);
    g.setColour(juce::Colour::fromRGB(68, 80, 87));
    g.drawLine(18, 48, 618, 48);
}

void GeistPadEditor::resized()
{
    arm.setBounds(18, 61, 92, 30);
    manual.setBounds(120, 61, 132, 30);
    mode.setBounds(267, 61, 135, 30);
    monitor.setBounds(422, 61, 175, 30);
    next.setBounds(18, 104, 112, 30);
    startLabel.setBounds(145, 105, 110, 25);
    start.setBounds(260, 105, 340, 25);
    stopLabel.setBounds(145, 137, 110, 25);
    stop.setBounds(260, 137, 340, 25);
    holdLabel.setBounds(145, 169, 110, 25);
    hold.setBounds(260, 169, 340, 25);
    for (int i = 0; i < GeistPadProcessor::numPads; ++i) {
        const int row = i / 4, col = i % 4;
        pads[size_t(i)].setBounds(18 + col * 153, 215 + row * 48, 141, 40);
    }
    status.setBounds(18, 417, 600, 28);
}

void GeistPadEditor::timerCallback()
{
    arm.setButtonText(processor.armed.load() ? "ARMED" : "Arm");
    for (int i = 0; i < GeistPadProcessor::numPads; ++i) {
        auto& b = pads[size_t(i)];
        b.setButtonText(juce::String(i + 1) + "  |  " + juce::String(processor.getFrames(i) > 0 ? "Sample" : "Empty"));
        b.setColour(juce::TextButton::buttonColourId,
            i == processor.selected.load() ? juce::Colour::fromRGB(15, 143, 140)
                                           : juce::Colour::fromRGB(48, 57, 64));
    }
    status.setText((processor.recording.load() ? "RECORDING  |  " : "Ready  |  ") +
                   processor.getStatus() + "  |  Input " +
                   juce::String(processor.meterDb.load(), 1) + " dBFS",
                   juce::dontSendNotification);
}
juce::AudioProcessorEditor* GeistPadProcessor::createEditor() { return new GeistPadEditor(*this); }
