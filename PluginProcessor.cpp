
#include "PluginProcessor.h"
#include "PluginEditor.h"

NebulaAudioProcessor::NebulaAudioProcessor()
    : AudioProcessor(
        BusesProperties()
            .withInput(
                "Input",
                juce::AudioChannelSet::stereo(),
                true
            )
            .withOutput(
                "Output",
                juce::AudioChannelSet::stereo(),
                true
            )
    ),
    apvts(
        *this,
        nullptr,
        "Parameters",
        createParameterLayout()
    ),
    presetManager(apvts, rack)
{
    rack.onChanged = [this]()
    {
        // Reserved for future UI notification.
    };
}

NebulaAudioProcessor::~NebulaAudioProcessor() = default;


// ============================================================
// BUS LAYOUT
// ============================================================

bool NebulaAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    const auto input = layouts.getMainInputChannelSet();

    if (output != juce::AudioChannelSet::mono()
        && output != juce::AudioChannelSet::stereo())
    {
        return false;
    }

    return output == input;
}


// ============================================================
// PARAMETERS
// ============================================================

juce::AudioProcessorValueTreeState::ParameterLayout
NebulaAudioProcessor::createParameterLayout()
{
    std::vector<
        std::unique_ptr<juce::RangedAudioParameter>
    > parameters;

    parameters.push_back(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "INPUT", 1 },
            "Input",
            -24.0f,
            24.0f,
            0.0f
        )
    );

    parameters.push_back(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "MIX", 1 },
            "Mix",
            0.0f,
            100.0f,
            100.0f
        )
    );

    parameters.push_back(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "OUTPUT", 1 },
            "Output",
            -24.0f,
            24.0f,
            0.0f
        )
    );

    return { parameters.begin(), parameters.end() };
}


// ============================================================
// PREPARE
// ============================================================

void NebulaAudioProcessor::prepareToPlay(
    double sampleRate,
    int samplesPerBlock)
{
    const int safeBlockSize = juce::jmax(
        1,
        samplesPerBlock
    );

    rack.prepare(
        sampleRate,
        safeBlockSize
    );

    // Reserve storage before audio processing starts.
    dryBuffer.setSize(
        2,
        safeBlockSize,
        false,
        true,
        true
    );

    dryBuffer.clear();

    fftInputBuffer.fill(0.0f);
    fftWorkBuffer.fill(0.0f);

    fftWritePosition = 0;
    spectrumSequence = 0;

    inputGainSmoothed.reset(sampleRate, 0.02);
    outputGainSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);

    inputGainSmoothed.setCurrentAndTargetValue(1.0f);
    outputGainSmoothed.setCurrentAndTargetValue(1.0f);
    mixSmoothed.setCurrentAndTargetValue(1.0f);
}

void NebulaAudioProcessor::releaseResources()
{
}


// ============================================================
// SPECTRUM COLLECTION
// ============================================================

void NebulaAudioProcessor::pushSpectrumSample(
    float sample) noexcept
{
    if (!std::isfinite(sample))
        sample = 0.0f;

    fftInputBuffer[
        static_cast<size_t>(fftWritePosition)
    ] = sample;

    ++fftWritePosition;

    if (fftWritePosition >= FFT_SIZE)
    {
        publishSpectrumFrame();
        fftWritePosition = 0;
    }
}


// ============================================================
// FFT PUBLICATION
// ============================================================

void NebulaAudioProcessor::publishSpectrumFrame() noexcept
{
    fftWorkBuffer.fill(0.0f);

    for (int i = 0; i < FFT_SIZE; ++i)
    {
        fftWorkBuffer[
            static_cast<size_t>(i)
        ] = fftInputBuffer[
            static_cast<size_t>(i)
        ];
    }

    window.multiplyWithWindowingTable(
        fftWorkBuffer.data(),
        FFT_SIZE
    );

    fft.performFrequencyOnlyForwardTransform(
        fftWorkBuffer.data()
    );

    nebula::SpectrumFrame frame;

    frame.sequence = ++spectrumSequence;

    for (size_t i = 0; i < frame.bins.size(); ++i)
    {
        const float magnitude = juce::jmax(
            0.0f,
            fftWorkBuffer[i] / static_cast<float>(FFT_SIZE)
        );

        const float db = juce::Decibels::gainToDecibels(
            magnitude,
            -100.0f
        );

        frame.bins[i] = juce::jlimit(
            0.0f,
            1.0f,
            (db + 80.0f) / 80.0f
        );
    }

    // Never wait for the UI thread.
    // If the queue is full, this frame is discarded.
    (void)spectrumFrames.tryPush(frame);
}


// ============================================================
// AUDIO PROCESSING
// ============================================================

void NebulaAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int inputChannels = getTotalNumInputChannels();
    const int outputChannels = getTotalNumOutputChannels();

    const int samples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    if (samples <= 0 || channels <= 0)
        return;

    // Clear unused output channels.
    for (int ch = inputChannels; ch < outputChannels; ++ch)
    {
        if (ch < channels)
            buffer.clear(ch, 0, samples);
    }

    // This processor supports mono and stereo layouts.
    if (channels > 2)
    {
        buffer.clear();
        return;
    }

    auto* inputParameter =
        apvts.getRawParameterValue("INPUT");

    auto* mixParameter =
        apvts.getRawParameterValue("MIX");

    auto* outputParameter =
        apvts.getRawParameterValue("OUTPUT");

    const float inputDb =
        inputParameter != nullptr
            ? inputParameter->load(std::memory_order_relaxed)
            : 0.0f;

    const float mixPercent =
        mixParameter != nullptr
            ? mixParameter->load(std::memory_order_relaxed)
            : 100.0f;

    const float outputDb =
        outputParameter != nullptr
            ? outputParameter->load(std::memory_order_relaxed)
            : 0.0f;

    const float inputTarget = juce::Decibels::decibelsToGain(
        inputDb
    );

    const float outputTarget = juce::Decibels::decibelsToGain(
        outputDb
    );

    const float mixTarget = juce::jlimit(
        0.0f,
        1.0f,
        mixPercent * 0.01f
    );

    inputGainSmoothed.setTargetValue(inputTarget);
    outputGainSmoothed.setTargetValue(outputTarget);
    mixSmoothed.setTargetValue(mixTarget);


    // --------------------------------------------------------
    // INPUT GAIN
    // --------------------------------------------------------

    for (int sample = 0; sample < samples; ++sample)
    {
        const float gain = inputGainSmoothed.getNextValue();

        for (int ch = 0; ch < channels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);

            float value = data[sample];

            if (!std::isfinite(value))
                value = 0.0f;

            data[sample] = value * gain;
        }
    }


    // --------------------------------------------------------
    // SPECTRUM ANALYSIS
    // --------------------------------------------------------

    // Analyze the first channel after input gain.
    // Samples are accumulated across arbitrary host blocks.
    const float* analysisData = buffer.getReadPointer(0);

    for (int i = 0; i < samples; ++i)
        pushSpectrumSample(analysisData[i]);


    // --------------------------------------------------------
    // CHUNKED RACK + GLOBAL DRY/WET
    // --------------------------------------------------------

    const int dryCapacity = dryBuffer.getNumSamples();

    const bool dryStorageAvailable =
        dryCapacity > 0
        && channels <= dryBuffer.getNumChannels();

    if (dryStorageAvailable)
    {
        // The host may send blocks larger than the size
        // requested in prepareToPlay().
        //
        // Process the host block in prepared-size chunks.
        // This avoids audio-thread allocation and preserves
        // the global Dry/Wet mix for oversized host blocks.

        std::array<float*, 2> channelPointers {};

        for (int ch = 0; ch < channels; ++ch)
            channelPointers[static_cast<size_t>(ch)] =
                buffer.getWritePointer(ch);

        int startSample = 0;

        while (startSample < samples)
        {
            const int chunkSamples = juce::jmin(
                dryCapacity,
                samples - startSample
            );

            // Non-owning view into the host audio buffer.
            juce::AudioBuffer<float> blockView(
                channelPointers.data(),
                channels,
                startSample,
                chunkSamples
            );

            // Save the dry signal before processing.
            for (int ch = 0; ch < channels; ++ch)
            {
                dryBuffer.copyFrom(
                    ch,
                    0,
                    blockView,
                    ch,
                    0,
                    chunkSamples
                );
            }

            // Process this chunk through the effect rack.
            rack.process(blockView);

            // Apply global mix and output gain.
            for (int sample = 0; sample < chunkSamples; ++sample)
            {
                const float wetAmount =
                    mixSmoothed.getNextValue();

                const float outputGain =
                    outputGainSmoothed.getNextValue();

                for (int ch = 0; ch < channels; ++ch)
                {
                    float* wet =
                        blockView.getWritePointer(ch);

                    const float* dry =
                        dryBuffer.getReadPointer(ch);

                    float wetSample = wet[sample];

                    if (!std::isfinite(wetSample))
                        wetSample = 0.0f;

                    const float drySample = dry[sample];

                    float outputSample =
                        drySample * (1.0f - wetAmount)
                        + wetSample * wetAmount;

                    outputSample *= outputGain;

                    if (!std::isfinite(outputSample))
                        outputSample = 0.0f;

                    wet[sample] = outputSample;
                }
            }

            startSample += chunkSamples;
        }
    }
    else
    {
        // Defensive fallback for an invalid or unprepared
        // processing state. Do not access unavailable storage.
        //
        // Normal DAW operation should use the chunked path.

        for (int sample = 0; sample < samples; ++sample)
        {
            const float outputGain =
                outputGainSmoothed.getNextValue();

            for (int ch = 0; ch < channels; ++ch)
            {
                float* data = buffer.getWritePointer(ch);

                float value = data[sample];

                if (!std::isfinite(value))
                    value = 0.0f;

                value *= outputGain;

                if (!std::isfinite(value))
                    value = 0.0f;

                data[sample] = value;
            }
        }
    }
}


// ============================================================
// PROGRAMS
// ============================================================

int NebulaAudioProcessor::getNumPrograms()
{
    return juce::jmax(
        1,
        presetManager.getAllPresetNames().size()
    );
}

int NebulaAudioProcessor::getCurrentProgram()
{
    const auto names = presetManager.getAllPresetNames();

    return juce::jmax(
        0,
        names.indexOf(
            presetManager.getCurrentPresetName()
        )
    );
}

void NebulaAudioProcessor::setCurrentProgram(int index)
{
    presetManager.loadPresetByIndex(index);
}

const juce::String NebulaAudioProcessor::getProgramName(int index)
{
    const auto names = presetManager.getAllPresetNames();

    if (index >= 0 && index < names.size())
        return names[index];

    return {};
}


// ============================================================
// EDITOR
// ============================================================

juce::AudioProcessorEditor*
NebulaAudioProcessor::createEditor()
{
    return new NebulaAudioProcessorEditor(*this);
}

bool NebulaAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String NebulaAudioProcessor::getName() const
{
    return "NEBULA";
}


// ============================================================
// STATE SAVE
// ============================================================

void NebulaAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    state.appendChild(
        rack.saveState(),
        nullptr
    );

    state.setProperty(
        "presetName",
        presetManager.getCurrentPresetName(),
        nullptr
    );

    if (auto xml = state.createXml())
    {
        copyXmlToBinary(
            *xml,
            destData
        );
    }
}


// ============================================================
// STATE RESTORE
// ============================================================

void NebulaAudioProcessor::setStateInformation(
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

    auto tree = juce::ValueTree::fromXml(*xml);

    if (!tree.isValid())
        return;

    apvts.replaceState(tree);

    if (tree.hasProperty("presetName"))
    {
        presetManager.setCurrentPresetName(
            tree.getProperty("presetName").toString()
        );
    }

    auto rackTree = tree.getChildWithName("RACK");

    if (rackTree.isValid())
        rack.loadState(rackTree);
}


// ============================================================
// PLUGIN ENTRY
// ============================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NebulaAudioProcessor();
}
