#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class NebulaDrumAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit NebulaDrumAudioProcessorEditor(NebulaDrumAudioProcessor&);
    ~NebulaDrumAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    NebulaDrumAudioProcessor& audioProcessor;

    // Компоненты секвенсора: 6 инструментов x 16 шагов
    juce::ToggleButton stepButtons[6][16];
    juce::Label instrumentLabels[6];

    juce::Slider tempoSlider;
    juce::Slider swingSlider;
    juce::Slider volumeSlider;

    using APVTSAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<APVTSAttachment> tempoAttachment;
    std::unique_ptr<APVTSAttachment> swingAttachment;
    std::unique_ptr<APVTSAttachment> volumeAttachment;

    const juce::String instrumentNames[6] = { "KICK", "SNARE", "HI-HAT", "CLAP", "TOM", "PERC" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NebulaDrumAudioProcessorEditor)
};
