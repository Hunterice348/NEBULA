
#include "InstrumentProcessor.h"

NebulaInstrumentProcessor::NebulaInstrumentProcessor()
    : juce::AudioProcessor(
        BusesProperties()
            .withOutput(
                "Output",
                juce::AudioChannelSet::stereo(),
                true
            )
    ),
    apvts(
        *this,
        nullptr,
        "NebulaInstrumentParameters",
        createParameterLayout()
    )
{
    auto* master = apvts.getRawParameterValue("MASTER");
    auto* attack = apvts.getRawParameterValue("ATTACK");
    auto* decay = apvts.getRawParameterValue("DECAY");
    auto* sustain = apvts.getRawParameterValue("SUSTAIN");
    auto* release = apvts.getRawParameterValue("RELEASE");
    auto* waveform = apvts.getRawParameterValue("WAVEFORM");

    for (int i = 0; i < 16; ++i)
    {
        synthesiser.addVoice(
            new NebulaSynthVoice(
                master,
                attack,
                decay,
                sustain,
                release,
                waveform
            )
        );
    }

    synthesiser.addSound(new NebulaSynthSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout
NebulaInstrumentProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "MASTER", 1 },
            "Master",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            0.8f
        )
    );

    parameters.push_back(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "ATTACK", 1 },
            "Attack",
            juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.4f),
            0.01f
        )
    );

    parameters.push_back(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "DECAY", 1 },
            "Decay",
            juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.4f),
            0.2f
        )
    );

    parameters.push_back(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "SUSTAIN", 1 },
            "Sustain",
            0.0f,
            1.0f,
            0.7f
        )
    );

    parameters.push_back(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "RELEASE", 1 },
            "Release",
            juce::NormalisableRange<float>(0.001f, 15.0f, 0.001f, 0.4f),
            0.3f
        )
    );

    parameters.push_back(
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "WAVEFORM", 1 },
            "Waveform",
            juce::StringArray { "Sine", "Saw", "Square" },
            0
        )
    );

    return { parameters.begin(), parameters.end() };
}

bool NebulaInstrumentProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    const auto input = layouts.getMainInputChannelSet();

    if (!input.isDisabled())
        return false;

    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void NebulaInstrumentProcessor::prepareToPlay(
    double sampleRate,
    int)
{
    synthesiser.setCurrentPlaybackSampleRate(sampleRate);
    synthesiser.allNotesOff(0, false);
}

void NebulaInstrumentProcessor::releaseResources()
{
    synthesiser.allNotesOff(0, false);
}

void NebulaInstrumentProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    synthesiser.renderNextBlock(
        buffer,
        midiMessages,
        0,
        buffer.getNumSamples()
    );
}

const juce::String NebulaInstrumentProcessor::getName() const
{
    return "NEBULA INSTRUMENT";
}

bool NebulaInstrumentProcessor::acceptsMidi() const
{
    return true;
}

bool NebulaInstrumentProcessor::producesMidi() const
{
    return false;
}

bool NebulaInstrumentProcessor::isMidiEffect() const
{
    return false;
}

double NebulaInstrumentProcessor::getTailLengthSeconds() const
{
    return 15.0;
}

int NebulaInstrumentProcessor::getNumPrograms()
{
    return 1;
}

int NebulaInstrumentProcessor::getCurrentProgram()
{
    return 0;
}

void NebulaInstrumentProcessor::setCurrentProgram(int)
{
}

const juce::String NebulaInstrumentProcessor::getProgramName(int)
{
    return "Init";
}

void NebulaInstrumentProcessor::changeProgramName(
    int,
    const juce::String&)
{
}

juce::AudioProcessorEditor*
NebulaInstrumentProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

bool NebulaInstrumentProcessor::hasEditor() const
{
    return true;
}

void NebulaInstrumentProcessor::getStateInformation(
    juce::MemoryBlock& destinationData)
{
    auto state = apvts.copyState();

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destinationData);
}

void NebulaInstrumentProcessor::setStateInformation(
    const void* data,
    int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xml(
        getXmlFromBinary(data, sizeInBytes)
    );

    if (xml == nullptr)
        return;

    if (!xml->hasTagName(apvts.state.getType()))
        return;

    auto restoredState = juce::ValueTree::fromXml(*xml);

    if (!restoredState.isValid())
        return;

    apvts.replaceState(restoredState);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NebulaInstrumentProcessor();
}
