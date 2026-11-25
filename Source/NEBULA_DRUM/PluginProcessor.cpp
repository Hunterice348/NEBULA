#include "PluginProcessor.h"
#include "PluginEditor.h"

NebulaDrumAudioProcessor::NebulaDrumAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
#endif
      apvts(*this, nullptr, "Parameters", {
          std::make_unique<juce::AudioParameterFloat>("TEMPO", "Tempo", 60.0f, 200.0f, 120.0f),
          std::make_unique<juce::AudioParameterFloat>("SWING", "Swing", 0.0f, 100.0f, 0.0f),
          std::make_unique<juce::AudioParameterFloat>("VOLUME", "Volume", -48.0f, 0.0f, -6.0f)
      })
{
    tempoParam = apvts.getRawParameterValue("TEMPO");
    swingParam = apvts.getRawParameterValue("SWING");
    volumeParam = apvts.getRawParameterValue("VOLUME");

    // Инициализация сетки (Kick на 1, 5, 9, 13; Snare на 5, 13; Hat на каждый нечетный)
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 16; ++j) {
            sequencerGrid[i][j] = false;
        }
    }
    // Простой паттерн по умолчанию
    sequencerGrid[0][0] = sequencerGrid[0][4] = sequencerGrid[0][8] = sequencerGrid[0][12] = true; // Kick
    sequencerGrid[1][4] = sequencerGrid[1][12] = true; // Snare
    for (int j = 0; j < 16; j += 2) sequencerGrid[2][j] = true; // Hi-Hat

    apvts.addParameterListener("TEMPO", this);
}

NebulaDrumAudioProcessor::~NebulaDrumAudioProcessor() {}

void NebulaDrumAudioProcessor::prepareToPlay(double sr, int samplesPerBlock) {
    sampleRate = sr;
    this->samplesPerBlock = samplesPerBlock;
    for (auto& voice : voices) {
        voice.isActive = false;
        voice.phase = 0.0;
        voice.envelope = 0.0f;
    }
    currentSample = 0;
    nextStepSample = 0;
    currentStep = 0;
    parameterChanged("TEMPO", *tempoParam);
}

void NebulaDrumAudioProcessor::releaseResources() {}

void NebulaDrumAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    if (parameterID == "TEMPO") {
        double beatsPerSecond = newValue / 60.0;
        double stepsPerBeat = 4.0; // 16-я ноты
        samplesPerStep = sampleRate / (beatsPerSecond * stepsPerBeat);
    }
}

void NebulaDrumAudioProcessor::triggerDrum(int instrumentIndex, float velocity) {
    // Найти свободный голос
    for (auto& voice : voices) {
        if (!voice.isActive) {
            voice.isActive = true;
            voice.instrumentIndex = instrumentIndex;
            voice.phase = 0.0;
            voice.envelope = 1.0f;
            
            // Частоты для синтезированных барабанов (можно заменить на загрузку сэмплов)
            if (instrumentIndex == 0) voice.frequency = 60.0f; // Kick
            else if (instrumentIndex == 1) voice.frequency = 200.0f; // Snare
            else if (instrumentIndex == 2) voice.frequency = 800.0f; // Hat
            else voice.frequency = 150.0f;
            
            // Отправка MIDI ноты (для MIDI export)
            // (Упрощено для примера, в продакшене использовать MidiBuffer)
            break;
        }
    }
}

void NebulaDrumAudioProcessor::processDrumVoice(int vIdx, juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    auto& voice = voices[vIdx];
    if (!voice.isActive) return;

    float decay = (voice.instrumentIndex == 0) ? 0.95f : ((voice.instrumentIndex == 2) ? 0.85f : 0.90f);
    
    for (int i = 0; i < numSamples; ++i) {
        float env = voice.envelope;
        if (env < 0.001f) {
            voice.isActive = false;
            break;
        }

        float sample = 0.0f;
        if (voice.instrumentIndex == 2) { // Hi-Hat (шум)
            sample = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * env;
        } else { // Kick/Snare (синус с затуханием частоты для удара)
            float currentFreq = voice.frequency;
            if (voice.instrumentIndex == 0 && voice.phase < 0.05) currentFreq *= 3.0f; // Pitch drop для Kick
            
            sample = std::sin(voice.phase * 2.0 * juce::MathConstants<double>::pi) * env;
            voice.phase += currentFreq / sampleRate;
        }

        // NaN защита
        if (!std::isfinite(sample)) sample = 0.0f;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            float* channelData = buffer.getWritePointer(ch);
            channelData[startSample + i] += sample * 0.5f; // Микширование
        }
        voice.envelope *= decay;
    }
}

void NebulaDrumAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    int numSamples = buffer.getNumSamples();
    float volGain = juce::Decibels::decibelsToGain(*volumeParam);

    for (int i = 0; i < numSamples; ++i) {
        if (currentSample >= nextStepSample) {
            // Триггер шага
            for (int inst = 0; inst < 6; ++inst) {
                if (sequencerGrid[inst][currentStep]) {
                    triggerDrum(inst, 1.0f);
                }
            }
            currentStep = (currentStep + 1) % 16;
            
            // Учет Swing (упрощенно)
            double swingFactor = 1.0 + ((*swingParam / 100.0) * 0.3 * (currentStep % 2 == 0 ? 1 : -1));
            nextStepSample += static_cast<int64>(samplesPerStep * swingFactor);
        }

        // Обработка голосов (сэмпл за сэмплом для точного триггера, в продакшене можно оптимизировать блоками)
        for (int v = 0; v < 8; ++v) {
            processDrumVoice(v, buffer, i, 1);
        }
        currentSample++;
    }

    buffer.applyGain(volGain);
}

juce::AudioProcessorEditor* NebulaDrumAudioProcessor::createEditor() {
    return new NebulaDrumAudioProcessorEditor(*this);
}

void NebulaDrumAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    // Сохраняем состояние сетки в ValueTree перед вызовом StateManager
    juce::ValueTree gridState("SequencerGrid");
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 16; ++j) {
            gridState.setProperty("cell_" + juce::String(i) + "_" + juce::String(j), sequencerGrid[i][j], nullptr);
        }
    }
    apvts.state.addChild(gridState, 0, nullptr);
    
    Nebula::StateManager::saveState(apvts, destData, currentUIWidth, currentUIHeight);
    
    apvts.state.removeChild(gridState, nullptr); // Очистка
}

void NebulaDrumAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    Nebula::StateManager::restoreState(apvts, data, sizeInBytes, currentUIWidth, currentUIHeight);
    
    // Восстановление сетки
    juce::ValueTree gridState = apvts.state.getChildWithName("SequencerGrid");
    if (gridState.isValid()) {
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 16; ++j) {
                sequencerGrid[i][j] = gridState.getProperty("cell_" + juce::String(i) + "_" + juce::String(j), false);
            }
        }
    }
}
