#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

GeistPadProcessor::GeistPadProcessor()
 : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    exportFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                        .getChildFile("GeistPad Captures");
    startTimerHz(5);
}

bool GeistPadProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GeistPadProcessor::prepareToPlay(double sampleRate, int)
{
    rate = sampleRate;
    capacity = juce::jmax(1, int(sampleRate * maxSeconds));
    for (auto& p : pads) {
        p.left.resize(size_t(capacity), 0.f);
        p.right.resize(size_t(capacity), 0.f);
        p.frames.store(0);
        p.exportPending.store(false);
        p.playing = false;
    }
    const int preFrames = juce::jmax(1, int(sampleRate * 0.080));
    preL.assign(size_t(preFrames), 0.f);
    preR.assign(size_t(preFrames), 0.f);
    preWrite = preCount = captured = silence = 0;
    activePad = -1;
    envelope = 0.f;
    recording.store(false);
}

void GeistPadProcessor::beginCapture(int index)
{
    if (activePad >= 0 || index < 0 || index >= numPads || capacity <= 0) return;
    auto& p = pads[size_t(index)];
    if (p.exportPending.load()) return;
    p.playing = false;
    p.frames.store(0);
    captured = 0;
    silence = 0;
    for (int i = 0; i < preCount && captured < capacity; ++i) {
        const int j = (preWrite - preCount + i + int(preL.size())) % int(preL.size());
        p.left[size_t(captured)] = preL[size_t(j)];
        p.right[size_t(captured)] = preR[size_t(j)];
        ++captured;
    }
    activePad = index;
    recording.store(true);
}

void GeistPadProcessor::finishCapture()
{
    if (activePad < 0) return;
    auto& p = pads[size_t(activePad)];
    if (captured > 0) {
        p.frames.store(captured);
        p.exportPending.store(true);
        if (nextPad.load()) selected.store((activePad + 1) % numPads);
    }
    activePad = -1;
    recording.store(false);
    captured = 0;
}

void GeistPadProcessor::playPad(int index)
{
    if (index < 0 || index >= numPads || index == activePad) return;
    auto& p = pads[size_t(index)];
    if (p.frames.load() > 0) {
        p.playPosition = 0;
        p.playing = true;
    }
}

void GeistPadProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2 || preL.empty()) return;
    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getWritePointer(1);
    const auto startLevel = juce::Decibels::decibelsToGain(startDb.load());
    const auto stopLevel = juce::Decibels::decibelsToGain(stopDb.load());
    const auto holdSamples = juce::jmax(1, int(rate * holdMs.load() / 1000.0));
    const auto attack = float(1.0 - std::exp(-1.0 / (rate * 0.002)));
    const auto release = float(1.0 - std::exp(-1.0 / (rate * 0.018)));

    if (manualToggle.exchange(false)) {
        if (activePad >= 0) finishCapture();
        else beginCapture(selected.load());
    }
    const int clicked = audition.exchange(-1);
    if (clicked >= 0) playPad(clicked);

    juce::MidiBuffer::Iterator iterator(midi);
    juce::MidiMessage msg;
    int eventPosition = -1;
    bool hasEvent = iterator.getNextEvent(msg, eventPosition);
    float peak = 0.f;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        while (hasEvent && eventPosition <= i) {
            if (msg.isNoteOn()) playPad(msg.getNoteNumber() - midiBase);
            hasEvent = iterator.getNextEvent(msg, eventPosition);
        }
        const float inL = l[i];
        const float inR = r[i];
        const float level = juce::jmax(std::abs(inL), std::abs(inR));
        peak = juce::jmax(peak, level);
        envelope += (level - envelope) * (level > envelope ? attack : release);

        if (armed.load() && activePad < 0) {
            if (triggerMode.load() == Immediate || envelope >= startLevel)
                beginCapture(selected.load());
        }
        if (activePad >= 0) {
            auto& p = pads[size_t(activePad)];
            if (captured < capacity) {
                p.left[size_t(captured)] = inL;
                p.right[size_t(captured)] = inR;
                ++captured;
            }
            if (envelope < stopLevel) ++silence;
            else silence = 0;
            if (captured >= capacity || (triggerMode.load() == Threshold && silence >= holdSamples))
                finishCapture();
        }

        float outL = monitor.load() ? inL : 0.f;
        float outR = monitor.load() ? inR : 0.f;
        for (auto& p : pads) {
            if (!p.playing) continue;
            const int frames = p.frames.load();
            if (p.playPosition < frames) {
                outL += p.left[size_t(p.playPosition)];
                outR += p.right[size_t(p.playPosition)];
                ++p.playPosition;
            } else p.playing = false;
        }
        l[i] = outL;
        r[i] = outR;
        preL[size_t(preWrite)] = inL;
        preR[size_t(preWrite)] = inR;
        preWrite = (preWrite + 1) % int(preL.size());
        preCount = juce::jmin(int(preL.size()), preCount + 1);
    }
    meterDb.store(juce::Decibels::gainToDecibels(peak, -100.f));
}

void GeistPadProcessor::timerCallback()
{
    for (int index = 0; index < numPads; ++index) {
        auto& p = pads[size_t(index)];
        if (!p.exportPending.load() || activePad == index) continue;
        const int frames = p.frames.load();
        if (frames <= 0) { p.exportPending.store(false); continue; }
        juce::AudioBuffer<float> audio(2, frames);
        audio.copyFrom(0, 0, p.left.data(), frames);
        audio.copyFrom(1, 0, p.right.data(), frames);
        if (!exportFolder.createDirectory()) { status = "Cannot create export folder"; continue; }
        const juce::String stem = "Pad_" + juce::String(index + 1).paddedLeft('0', 2) + "_" +
                                   juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        auto output = exportFolder.getNonexistentChildFile(stem, ".wav", false);
        juce::WavAudioFormat format;
        std::unique_ptr<juce::FileOutputStream> stream(output.createOutputStream());
        if (stream != nullptr) {
            std::unique_ptr<juce::AudioFormatWriter> writer(
                format.createWriterFor(stream.get(), rate, 2, 24, {}, 0));
            if (writer != nullptr) {
                stream.release();
                status = writer->writeFromAudioSampleBuffer(audio, 0, frames)
                       ? "Saved " + output.getFileName() : "WAV write failed";
            } else status = "Cannot create WAV writer";
        } else status = "Cannot open WAV output";
        p.exportPending.store(false);
    }
}

void GeistPadProcessor::getStateInformation(juce::MemoryBlock& block)
{
    juce::XmlElement xml("GEISTPAD");
    xml.setAttribute("armed", armed.load());
    xml.setAttribute("selected", selected.load());
    xml.setAttribute("mode", triggerMode.load());
    xml.setAttribute("next", nextPad.load());
    xml.setAttribute("start", startDb.load());
    xml.setAttribute("stop", stopDb.load());
    xml.setAttribute("hold", holdMs.load());
    xml.setAttribute("monitor", monitor.load());
    copyXmlToBinary(xml, block);
}
void GeistPadProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size)) {
        if (!xml->hasTagName("GEISTPAD")) return;
        armed.store(xml->getBoolAttribute("armed", false));
        selected.store(juce::jlimit(0, 15, xml->getIntAttribute("selected", 0)));
        triggerMode.store(juce::jlimit(0, 1, xml->getIntAttribute("mode", 0)));
        nextPad.store(xml->getBoolAttribute("next", true));
        startDb.store(float(xml->getDoubleAttribute("start", -24)));
        stopDb.store(float(xml->getDoubleAttribute("stop", -42)));
        holdMs.store(xml->getIntAttribute("hold", 220));
        monitor.store(xml->getBoolAttribute("monitor", true));
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GeistPadProcessor(); }
