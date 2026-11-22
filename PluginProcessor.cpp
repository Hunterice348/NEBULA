#include "PluginProcessor.h"
#include "PluginEditor.h"

NebulaAudioProcessor::NebulaAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      presetManager(apvts, rack)
{
    rack.onChanged = [this]() { /* UI notification hook; no synchronization is provided by this callback. */ };
}

NebulaAudioProcessor::~NebulaAudioProcessor() {}

bool NebulaAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

juce::AudioProcessorValueTreeState::ParameterLayout NebulaAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "INPUT", 1 }, "Input", -24.0f, 24.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 100.0f, 100.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "OUTPUT", 1 }, "Output", -24.0f, 24.0f, 0.0f));
    return { p.begin(), p.end() };
}

void NebulaAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    rack.prepare(sampleRate, samplesPerBlock);
    // Host reconfiguration is expected to occur with processing stopped. Reserve
    // two channels to support the declared mono/stereo layouts without callback growth.
    dryBuffer.setSize(2, juce::jmax(1, samplesPerBlock), false, true, true);
    fftBuffer.fill(0.0f);
    inputGainSmoothed.reset(sampleRate, 0.02);
    outputGainSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);
}

void NebulaAudioProcessor::releaseResources() {}

void NebulaAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int inputChannels = getTotalNumInputChannels();
    const int outputChannels = getTotalNumOutputChannels();
    const int samples = buffer.getNumSamples();

    for (int ch = inputChannels; ch < outputChannels; ++ch)
        buffer.clear(ch, 0, samples);

    // Do not resize a buffer in the callback. This is an interim bounded fallback;
    // the host normally prepares with its maximum block size. Oversized blocks
    // bypass the global dry/wet blend rather than reading past prepared storage.
    const bool dryCapacityAvailable = samples <= dryBuffer.getNumSamples()
                                   && buffer.getNumChannels() <= dryBuffer.getNumChannels();
    if (dryCapacityAvailable)
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, samples);

    if (buffer.getNumChannels() > 0 && samples >= FFT_SIZE)
    {
        const auto* data = buffer.getReadPointer(0);
        for (int i = 0; i < FFT_SIZE; ++i)
            fftBuffer[(size_t) i] = std::isfinite(data[i]) ? data[i] : 0.0f;
        window.multiplyWithWindowingTable(fftBuffer.data(), FFT_SIZE);
        fft.performFrequencyOnlyForwardTransform(fftBuffer.data());

        nebula::SpectrumFrame frame;
        frame.sequence = ++spectrumSequence;
        for (size_t i = 0; i < frame.bins.size(); ++i)
        {
            const float magnitude = juce::jmax(0.0f, fftBuffer[i] / (float) FFT_SIZE);
            const float db = juce::Decibels::gainToDecibels(magnitude, -100.0f);
            frame.bins[i] = juce::jlimit(0.0f, 1.0f, (db + 80.0f) / 80.0f);
        }
        // Bounded telemetry is lossy by design: if UI is behind, preserve queued
        // complete frames and drop this one instead of waiting or overwriting.
        (void) spectrumFrames.tryPush(frame);
    }

    auto* inParam = apvts.getRawParameterValue("INPUT");
    auto* outParam = apvts.getRawParameterValue("OUTPUT");
    auto* mixParam = apvts.getRawParameterValue("MIX");
    const float inputDb = inParam != nullptr ? inParam->load() : 0.0f;
    const float outputDb = outParam != nullptr ? outParam->load() : 0.0f;
    const float mix = mixParam != nullptr ? mixParam->load() / 100.0f : 1.0f;

    inputGainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(inputDb));
    outputGainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(outputDb));
    mixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, mix));
    buffer.applyGain(inputGainSmoothed.getNextValue());

    // NOTE: rack remains mutable and is not safe for concurrent structural edits.
    rack.process(buffer);

    if (dryCapacityAvailable)
    {
        const float outputGain = outputGainSmoothed.getNextValue();
        const float wetMix = mixSmoothed.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* dry = dryBuffer.getReadPointer(ch);
            auto* wet = buffer.getWritePointer(ch);
            for (int i = 0; i < samples; ++i)
                wet[i] = (dry[i] * (1.0f - wetMix) + wet[i] * wetMix) * outputGain;
        }
    }
}

int NebulaAudioProcessor::getNumPrograms() { return juce::jmax(1, presetManager.getAllPresetNames().size()); }
int NebulaAudioProcessor::getCurrentProgram() { return juce::jmax(0, presetManager.getAllPresetNames().indexOf(presetManager.getCurrentPresetName())); }
void NebulaAudioProcessor::setCurrentProgram(int i) { presetManager.loadPresetByIndex(i); }
const juce::String NebulaAudioProcessor::getProgramName(int i)
{
    auto names = presetManager.getAllPresetNames();
    return (i >= 0 && i < names.size()) ? names[i] : juce::String();
}
juce::AudioProcessorEditor* NebulaAudioProcessor::createEditor() { return new NebulaAudioProcessorEditor(*this); }
bool NebulaAudioProcessor::hasEditor() const { return true; }
const juce::String NebulaAudioProcessor::getName() const { return "NEBULA"; }

void NebulaAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.appendChild(rack.saveState(), nullptr);
    state.setProperty("presetName", presetManager.getCurrentPresetName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void NebulaAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid())
        {
            apvts.replaceState(tree);
            if (tree.hasProperty("presetName"))
                presetManager.setCurrentPresetName(tree.getProperty("presetName").toString());
            if (auto rackTree = tree.getChildWithName("RACK"); rackTree.isValid())
                rack.loadState(rackTree);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NebulaAudioProcessor(); }
