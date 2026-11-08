#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class GeistPadProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    static constexpr int numPads = 16;
    static constexpr int midiBase = 36;
    static constexpr int maxSeconds = 10;
    enum Mode { Threshold = 0, Immediate = 1 };
    GeistPadProcessor();
    ~GeistPadProcessor() override = default;

    const juce::String getName() const override { return "GeistPad"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    std::atomic<bool> armed { false }, monitor { true }, nextPad { true };
    std::atomic<int> selected { 0 }, triggerMode { 0 }, holdMs { 220 };
    std::atomic<float> startDb { -24.0f }, stopDb { -42.0f }, meterDb { -100.0f };
    std::atomic<bool> recording { false }, manualToggle { false };
    std::atomic<int> audition { -1 };
    int getFrames(int i) const { return pads[size_t(juce::jlimit(0, 15, i))].frames.load(); }
    juce::String getStatus() const { return status; }
    juce::File getExportFolder() const { return exportFolder; }

private:
    struct Pad {
        std::vector<float> left, right;
        std::atomic<int> frames {0};
        std::atomic<bool> exportPending {false};
        int playPosition = 0;
        bool playing = false;
    };
    std::array<Pad, numPads> pads;
    std::vector<float> preL, preR;
    int preWrite = 0, preCount = 0;
    int capacity = 0, captured = 0, silence = 0, activePad = -1;
    double rate = 44100.0;
    float envelope = 0.0f;
    juce::File exportFolder;
    juce::String status = "Ready";
    void beginCapture(int pad);
    void finishCapture();
    void playPad(int pad);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeistPadProcessor)
};
