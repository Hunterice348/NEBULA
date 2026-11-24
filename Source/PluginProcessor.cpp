#include <limits>

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>
#include <cmath>
#include <memory>
#include <vector>

NebulaFXAudioProcessor::NebulaFXAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "NEBULA_STATE", createParameterLayout())
{
    inputParameter = parameters.getRawParameterValue("input");
    outputParameter = parameters.getRawParameterValue("output");
    mixParameter = parameters.getRawParameterValue("mix");
    driveParameter = parameters.getRawParameterValue("drive");
    delayParameter = parameters.getRawParameterValue("delay");
    feedbackParameter = parameters.getRawParameterValue("feedback");
    modeParameter = parameters.getRawParameterValue("mode");
}

juce::AudioProcessorValueTreeState::ParameterLayout
NebulaFXAudioProcessor::createParameterLayout()
{
    using FloatParameter = juce::AudioParameterFloat;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<FloatParameter>(
        "input", "Input",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f));

    params.push_back(std::make_unique<FloatParameter>(
        "output", "Output",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f));

    params.push_back(std::make_unique<FloatParameter>(
        "mix", "Wet / Dry",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.5f));

    params.push_back(std::make_unique<FloatParameter>(
        "drive", "Drive",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.01f),
        6.0f));

    params.push_back(std::make_unique<FloatParameter>(
        "delay", "Delay Time",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f),
        350.0f));

    params.push_back(std::make_unique<FloatParameter>(
        "feedback", "Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.001f),
        0.35f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "mode", "Effect Mode",
        juce::StringArray { "Clean", "Delay", "Saturation" },
        0));

    return { params.begin(), params.end() };
}

void NebulaFXAudioProcessor::prepareToPlay(
    double sampleRate,
    int samplesPerBlock)
{
    audioEngine.prepare(
        sampleRate,
        getTotalNumOutputChannels(),
        samplesPerBlock);
}

void NebulaFXAudioProcessor::releaseResources()
{
    audioEngine.release();
}

bool NebulaFXAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    return !input.isDisabled()
        && input == output
        && output.size() > 0
        && output.size() <= 8;
}

void NebulaFXAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto load = [](
        const std::atomic<float>* parameter,
        float fallback)
    {
        if (parameter == nullptr)
            return fallback;

        const float value =
            parameter->load(std::memory_order_relaxed);

        return std::isfinite(value) ? value : fallback;
    };

    NebulaAudioParameters p;

    p.inputGain = juce::Decibels::decibelsToGain(
        juce::jlimit(
            -24.0f,
            24.0f,
            load(inputParameter, 0.0f)));

    p.outputGain = juce::Decibels::decibelsToGain(
        juce::jlimit(
            -24.0f,
            24.0f,
            load(outputParameter, 0.0f)));

    p.wet = juce::jlimit(
        0.0f,
        1.0f,
        load(mixParameter, 0.5f));

    p.drive = juce::jlimit(
        1.0f,
        30.0f,
        juce::Decibels::decibelsToGain(
            juce::jlimit(
                0.0f,
                24.0f,
                load(driveParameter, 6.0f))));

    p.feedback = juce::jlimit(
        0.0f,
        0.95f,
        load(feedbackParameter, 0.35f));

    const double sampleRate = getSampleRate();

    const double safeSampleRate =
        std::isfinite(sampleRate) && sampleRate > 0.0
            ? sampleRate
            : 44100.0;

    const double delayMilliseconds =
        juce::jlimit(
            1.0f,
            2000.0f,
            load(delayParameter, 350.0f));

    const double calculatedDelay =
        safeSampleRate * delayMilliseconds / 1000.0;

    const int maximumDelaySamples =
        juce::jmax(
            1,
            static_cast<int>(
                juce::jmin(
                    calculatedDelay,
                    static_cast<double>(
                        std::numeric_limits<int>::max() / 2))));

    p.delaySamples = static_cast<float>(
        juce::jlimit(
            1,
            juce::jmax(
                1,
                static_cast<int>(safeSampleRate * 2.0)),
            maximumDelaySamples));

    p.mode = juce::jlimit(
        0,
        2,
        static_cast<int>(load(modeParameter, 0.0f)));

    std::array<float*, 8> channelData {};

    const int channels = juce::jmin(
        buffer.getNumChannels(),
        static_cast<int>(channelData.size()));

    for (int channel = 0; channel < channels; ++channel)
    {
        channelData[static_cast<std::size_t>(channel)] =
            buffer.getWritePointer(channel);
    }

    audioEngine.setParameters(p);

    if (channels > 0)
    {
        audioEngine.processBlock(
            channelData.data(),
            channels,
            buffer.getNumSamples());
    }
}

void NebulaFXAudioProcessor::processBlockBypassed(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    // Host bypass: unity-gain pass-through.
    // No audio samples are modified.
    juce::ignoreUnused(buffer);
}

juce::AudioProcessorEditor*
NebulaFXAudioProcessor::createEditor()
{
    return new NebulaFXAudioProcessorEditor(*this);
}

bool NebulaFXAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String NebulaFXAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NebulaFXAudioProcessor::acceptsMidi() const
{
    return false;
}

bool NebulaFXAudioProcessor::producesMidi() const
{
    return false;
}

bool NebulaFXAudioProcessor::isMidiEffect() const
{
    return false;
}

double NebulaFXAudioProcessor::getTailLengthSeconds() const
{
    return 2.0;
}

int NebulaFXAudioProcessor::getNumPrograms()
{
    return 1;
}

int NebulaFXAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NebulaFXAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String
NebulaFXAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void NebulaFXAudioProcessor::changeProgramName(
    int index,
    const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void NebulaFXAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData)
{
    const auto state = parameters.copyState();

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void NebulaFXAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(parameters.state.getType()))
        {
            parameters.replaceState(
                juce::ValueTree::fromXml(*xml));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NebulaFXAudioProcessor();
}
