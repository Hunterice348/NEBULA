#include "PluginProcessor.h"
#include "PluginEditor.h"

NebulaSynthAudioProcessor::NebulaSynthAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
#endif
      apvts(*this, nullptr, "Parameters", {
          std::make_unique<juce::AudioParameterFloat>("OSC_MIX", "Oscillator Mix", 0.0f, 1.0f, 0.5f),
          std::make_unique<juce::AudioParameterFloat>("CUTOFF", "Filter Cutoff", 20.0f, 20000.0f, 5000.0f, juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f)),
          std::make_unique<juce::AudioParameterFloat>("RESONANCE", "Filter Resonance", 0.0f, 0.95f, 0.2f),
          std::make_unique<juce::AudioParameterFloat>("ATTACK", "Attack", 0.001f, 2.0f, 0.01f),
          std::make_unique<juce::AudioParameterFloat>("RELEASE", "Release", 0.01f, 5.0f, 0.5f),
          std::make_unique<juce::AudioParameterFloat>("MASTER_VOL", "Master Volume", -48.0f, 0.0f, -6.0f)
      })
{
}

NebulaSynthAudioProcessor::~NebulaSynthAudioProcessor() {}

void NebulaSynthAudioProcessor::prepareToPlay(double sr, int samplesPerBlock) {
    sampleRate = sr;
    for (auto& voice : voices) {
        voice.isActive = false;
        voice.phase = 0.0;
        voice.envelope = 0.0f;
        voice.filter.prepare({ sr, static_cast<juce::uint32>(samplesPerBlock), 2 });
        voice.filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    }
}

void NebulaSynthAudioProcessor::releaseResources() {}

void NebulaSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    float oscMix = *apvts.getRawParameterValue("OSC_MIX");
    float cutoff = *apvts.getRawParameterValue("CUTOFF");
    float resonance = *apvts.getRawParameterValue("RESONANCE");
    float attack = *apvts.getRawParameterValue("ATTACK");
    float release = *apvts.getRawParameterValue("RELEASE");
    float masterVol = juce::Decibels::decibelsToGain(*apvts.getRawParameterValue("MASTER_VOL"));

    // Обновляем параметры фильтра для всех голосов
    for (auto& voice : voices) {
        if (voice.isActive) {
            voice.filter.setCutoffFrequency(cutoff);
            voice.filter.setResonance(resonance);
        }
    }

    // Обработка MIDI сообщений
    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            // Найти свободный голос (простейший voice stealing)
            Voice* freeVoice = nullptr;
            for (auto& v : voices) {
                if (!v.isActive) { freeVoice = &v; break; }
            }
            if (!freeVoice) freeVoice = &voices[0]; // Steal oldest (упрощенно)

            freeVoice->isActive = true;
            freeVoice->noteNumber = msg.getNoteNumber();
            freeVoice->phase = 0.0;
            freeVoice->envelope = 0.0f;
            freeVoice->filter.reset();
        } else if (msg.isNoteOff()) {
            for (auto& v : voices) {
                if (v.isActive && v.noteNumber == msg.getNoteNumber()) {
                    // Запуск фазы релиза (упрощенно: просто помечаем для затухания)
                    // В полном продакшене здесь нужен отдельный release envelope
                }
            }
        }
    }

    // Генерация аудио
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        float outL = 0.0f, outR = 0.0f;

        for (auto& voice : voices) {
            if (!voice.isActive) continue;

            float freq = 440.0f * std::pow(2.0, (voice.noteNumber - 69.0) / 12.0);
            
            // Осцилляторы: Saw + Square mix
            float saw = 2.0f * (voice.phase - std::floor(voice.phase + 0.5f));
            float square = (saw > 0.0f) ? 1.0f : -1.0f;
            float osc = (saw * (1.0f - oscMix)) + (square * oscMix);
            
            voice.phase += freq / sampleRate;
            if (voice.phase > 1.0f) voice.phase -= 1.0f;

            // ADSR (упрощенный Attack/Release)
            if (voice.envelope < 1.0f) {
                voice.envelope += (1.0f - voice.envelope) * (attack * sampleRate * 0.001f);
            } else {
                // Здесь должна быть логика удержания и релиза по MIDI Note Off
                // Для демо-версии просто медленное затухание если не активен, но мы оставим его активным
            }

            // Фильтрация
            juce::dsp::AudioBlock<float> block(&osc, 1, 1);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            voice.filter.process(ctx);
            
            float filteredSample = osc * voice.envelope;

            // NaN защита
            if (!std::isfinite(filteredSample)) filteredSample = 0.0f;

            outL += filteredSample;
            outR += filteredSample;
        }

        buffer.setSample(0, i, outL * masterVol);
        buffer.setSample(1, i, outR * masterVol);
    }
    
    // Очистка завершенных голосов (если envelope упал до 0)
    for (auto& voice : voices) {
        if (voice.isActive && voice.envelope < 0.001f) {
            voice.isActive = false;
        }
    }
}

juce::AudioProcessorEditor* NebulaSynthAudioProcessor::createEditor() {
    return new NebulaSynthAudioProcessorEditor(*this);
}

void NebulaSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    Nebula::StateManager::saveState(apvts, destData, currentUIWidth, currentUIHeight);
}

void NebulaSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    Nebula::StateManager::restoreState(apvts, data, sizeInBytes, currentUIWidth, currentUIHeight);
}

void NebulaSynthAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue) {}
