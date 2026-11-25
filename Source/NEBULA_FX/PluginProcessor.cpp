#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/StateManager.h"

NebulaFXAudioProcessor::NebulaFXAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
#endif
      apvts(*this, nullptr, "Parameters", {
          std::make_unique<juce::AudioParameterFloat>("THRESHOLD", "Threshold", -60.0f, 0.0f, -12.0f),
          std::make_unique<juce::AudioParameterFloat>("RATIO", "Ratio", 1.0f, 20.0f, 4.0f),
          std::make_unique<juce::AudioParameterFloat>("ATTACK", "Attack", 0.1f, 100.0f, 10.0f),
          std::make_unique<juce::AudioParameterFloat>("RELEASE", "Release", 10.0f, 1000.0f, 100.0f),
          std::make_unique<juce::AudioParameterFloat>("MASTER_GAIN", "Master Gain", 0.0f, 24.0f, 0.0f)
      })
{
    apvts.addParameterListener("THRESHOLD", this);
    apvts.addParameterListener("RATIO", this);
    apvts.addParameterListener("ATTACK", this);
    apvts.addParameterListener("RELEASE", this);
}

NebulaFXAudioProcessor::~NebulaFXAudioProcessor() {}

void NebulaFXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());
    compressor.prepare(spec);
}

void NebulaFXAudioProcessor::releaseResources() {}

void NebulaFXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    
    float threshold = *apvts.getRawParameterValue("THRESHOLD");
    float ratio = *apvts.getRawParameterValue("RATIO");
    float attack = *apvts.getRawParameterValue("ATTACK");
    float release = *apvts.getRawParameterValue("RELEASE");
    float masterGain = *apvts.getRawParameterValue("MASTER_GAIN");

    compressor.process(buffer, threshold, ratio, attack, release);

    // Применяем Master Gain
    float gainLinear = juce::Decibels::decibelsToGain(masterGain);
    buffer.applyGain(gainLinear);
}

juce::AudioProcessorEditor* NebulaFXAudioProcessor::createEditor() {
    return new NebulaFXAudioProcessorEditor(*this);
}

void NebulaFXAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    Nebula::StateManager::saveState(apvts, destData, currentUIWidth, currentUIHeight);
}

void NebulaFXAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    Nebula::StateManager::restoreState(apvts, data, sizeInBytes, currentUIWidth, currentUIHeight);
}

void NebulaFXAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    // Реагируем на изменения параметров, если нужно (например, для обновления UI вне audio thread)
}
