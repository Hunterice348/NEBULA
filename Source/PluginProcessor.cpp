
#include "PluginProcessor.h"
#include "PluginEditor.h"

NebulaFXAudioProcessor::NebulaFXAudioProcessor()
    : AudioProcessor(
        BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)
    ),
    parameters(*this, nullptr, "NEBULA_STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
NebulaFXAudioProcessor::createParameterLayout()
{
    using FloatParameter = juce::AudioParameterFloat;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<FloatParameter>(
        "input", "Input",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f
    ));

    params.push_back(std::make_unique<FloatParameter>(
        "output", "Output",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f
    ));

    params.push_back(std::make_unique<FloatParameter>(
        "mix", "Wet / Dry",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.5f
    ));

    params.push_back(std::make_unique<FloatParameter>(
        "drive", "Drive",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.01f),
        6.0f
    ));

    params.push_back(std::make_unique<FloatParameter>(
        "delay", "Delay Time",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f),
        350.0f
    ));

    params.push_back(std::make_unique<FloatParameter>(
        "feedback", "Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.001f),
        0.35f
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "mode",
        "Effect Mode",
        juce::StringArray { "Clean", "Delay", "Saturation" },
        0
    ));

    return { params.begin(), params.end() };
}

void NebulaFXAudioProcessor::prepareToPlay(
    double sampleRate,
    int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;

    const int channels = juce::jmax(1, getTotalNumOutputChannels());
    const int maxDelaySamples = juce::jmax(
        1,
        static_cast<int>(sampleRate * 2.1)
    );

    delayBuffers.assign(
        static_cast<size_t>(channels),
        std::vector<float>(
            static_cast<size_t>(maxDelaySamples),
            0.0f
        )
    );

    delayWritePositions.assign(
        static_cast<size_t>(channels),
        0
    );
}

void NebulaFXAudioProcessor::releaseResources()
{
    delayBuffers.clear();
    delayWritePositions.clear();
}

bool NebulaFXAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono()
        && output != juce::AudioChannelSet::stereo())
        return false;

    return output == layouts.getMainInputChannelSet();
}

void NebulaFXAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;

    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();

    const float inputDb = parameters.getRawParameterValue("input")->load();
    const float outputDb = parameters.getRawParameterValue("output")->load();
    const float mix = parameters.getRawParameterValue("mix")->load();
    const float drive = parameters.getRawParameterValue("drive")->load();
    const float delayMs = parameters.getRawParameterValue("delay")->load();
    const float feedback = parameters.getRawParameterValue("feedback")->load();

    const int mode = static_cast<int>(
        parameters.getRawParameterValue("mode")->load()
    );

    const float inputGain = juce::Decibels::decibelsToGain(inputDb);
    const float outputGain = juce::Decibels::decibelsToGain(outputDb);

    const float driveAmount = juce::jlimit(
        1.0f,
        30.0f,
        juce::Decibels::decibelsToGain(drive)
    );

    const int delaySamples = juce::jlimit(
        1,
        static_cast<int>(currentSampleRate * 2.0),
        static_cast<int>(currentSampleRate * delayMs / 1000.0)
    );

    const float dryMix = 1.0f - mix;

    for (int channel = 0; channel < channels; ++channel)
    {
        float* data = buffer.getWritePointer(channel);

        const bool hasDelayBuffer =
            channel < static_cast<int>(delayBuffers.size());

        if (hasDelayBuffer)
        {
            auto& delayBuffer = delayBuffers[static_cast<size_t>(channel)];
            int& writePosition =
                delayWritePositions[static_cast<size_t>(channel)];

            const int bufferSize = static_cast<int>(delayBuffer.size());

            for (int i = 0; i < samples; ++i)
            {
                const float dry = data[i] * inputGain;
                float wet = dry;

                if (mode == 1 && bufferSize > 0)
                {
                    const int readPosition =
                        (writePosition - delaySamples + bufferSize)
                        % bufferSize;

                    const float delayed = delayBuffer[
                        static_cast<size_t>(readPosition)
                    ];

                    delayBuffer[static_cast<size_t>(writePosition)] =
                        dry + delayed * feedback;

                    writePosition = (writePosition + 1) % bufferSize;

                    wet = delayed;
                }
                else if (mode == 2)
                {
                    wet = std::tanh(dry * driveAmount);
                }

                const float processed =
                    mode == 0 ? dry : dry * dryMix + wet * mix;

                data[i] = processed * outputGain;

                if (!std::isfinite(data[i]))
                    data[i] = 0.0f;
            }
        }
        else
        {
            for (int i = 0; i < samples; ++i)
            {
                const float dry = data[i] * inputGain;
                data[i] = dry * outputGain;
            }
        }
    }
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

bool NebulaFXAudioProcessor::acceptsMidi() const { return false; }
bool NebulaFXAudioProcessor::producesMidi() const { return false; }
bool NebulaFXAudioProcessor::isMidiEffect() const { return false; }

double NebulaFXAudioProcessor::getTailLengthSeconds() const
{
    return 2.0;
}

int NebulaFXAudioProcessor::getNumPrograms() { return 1; }
int NebulaFXAudioProcessor::getCurrentProgram() { return 0; }

void NebulaFXAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String NebulaFXAudioProcessor::getProgramName(int index)
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
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    copyXmlToBinary(*xml, destData);
}

void NebulaFXAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(
        getXmlFromBinary(data, sizeInBytes)
    );

    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NebulaFXAudioProcessor();
}
