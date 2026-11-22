#include "PluginProcessor.h"
#include "PluginEditor.h"

NebulaAudioProcessor::NebulaAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      presetManager(apvts, rack)
{
    rack.onChanged = [this]() { /* Уведомление редактора при необходимости */ };
}

NebulaAudioProcessor::~NebulaAudioProcessor() {}

bool NebulaAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // FL Studio требует четкой валидации входов/выходов
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Вход должен совпадать с выходом для эффекта
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

juce::AudioProcessorValueTreeState::ParameterLayout
NebulaAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"INPUT", 1}, "Input", -24.0f, 24.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"MIX", 1}, "Mix", 0.0f, 100.0f, 100.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"OUTPUT", 1}, "Output", -24.0f, 24.0f, 0.0f));
    return { p.begin(), p.end() };
}

void NebulaAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    rack.prepare(sampleRate, samplesPerBlock);
    dryBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    fftBuffer.fill(0.0f);

    inputGainSmoothed.reset(sampleRate, 0.02);
    outputGainSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);
}

void NebulaAudioProcessor::releaseResources() {}

void NebulaAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    dryBuffer.makeCopyOf(buffer, true);

    // 1. FFT для спектра
    if (buffer.getNumSamples() >= FFT_SIZE)
    {
        auto* data = buffer.getReadPointer(0);
        for (int i = 0; i < FFT_SIZE; ++i) fftBuffer[i] = data[i];
        window.multiplyWithWindowingTable(fftBuffer.data(), FFT_SIZE);
        fft.performFrequencyOnlyForwardTransform(fftBuffer.data());
        for (int i = 0; i < FFT_SIZE / 2; ++i)
        {
            float v = juce::Decibels::gainToDecibels(fftBuffer[i] / (float)FFT_SIZE);
            spectrumData[i] = juce::jlimit(0.0f, 1.0f, (v + 80.0f) / 80.0f);
        }
        spectrumReady.store(true);
    }

    // 2. Чтение параметров без копирования std::atomic
    auto* inParam  = apvts.getRawParameterValue("INPUT");
    auto* outParam = apvts.getRawParameterValue("OUTPUT");
    auto* mixParam = apvts.getRawParameterValue("MIX");

    float inGain  = juce::Decibels::decibelsToGain(inParam  ? inParam->load()  : 0.0f);
    float outGain = juce::Decibels::decibelsToGain(outParam ? outParam->load() : 0.0f);
    float mix     = (mixParam ? mixParam->load() : 100.0f) / 100.0f;

    inputGainSmoothed.setTargetValue(inGain);
    outputGainSmoothed.setTargetValue(outGain);
    mixSmoothed.setTargetValue(mix);

    // 3. Входной гейн
    buffer.applyGain(inputGainSmoothed.getNextValue());

    // 4. Rack
    rack.process(buffer);

    // 5. Выходной микс и гейн
    float currentOutGain = outputGainSmoothed.getNextValue();
    float currentMix     = mixSmoothed.getNextValue();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* dry = dryBuffer.getReadPointer(ch);
        auto* wet = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            wet[i] = (dry[i] * (1.0f - currentMix) + wet[i] * currentMix) * currentOutGain;
    }
}

// === DAW Programs ===
int NebulaAudioProcessor::getNumPrograms() { return juce::jmax(1, presetManager.getAllPresetNames().size()); }
int NebulaAudioProcessor::getCurrentProgram() { return juce::jmax(0, presetManager.getAllPresetNames().indexOf(presetManager.getCurrentPresetName())); }
void NebulaAudioProcessor::setCurrentProgram(int i) { presetManager.loadPresetByIndex(i); }
const juce::String NebulaAudioProcessor::getProgramName(int i) {
    auto n = presetManager.getAllPresetNames();
    return (i >= 0 && i < n.size()) ? n[i] : juce::String();
}

juce::AudioProcessorEditor* NebulaAudioProcessor::createEditor()
{
    return new NebulaAudioProcessorEditor(*this);
}

bool NebulaAudioProcessor::hasEditor() const { return true; }
const juce::String NebulaAudioProcessor::getName() const { return "NEBULA"; }

void NebulaAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.appendChild(rack.saveState(), nullptr);
    state.setProperty("presetName", presetManager.getCurrentPresetName(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NebulaAudioProcessor();
}
