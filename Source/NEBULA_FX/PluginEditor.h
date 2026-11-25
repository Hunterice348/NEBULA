#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class NebulaFXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NebulaFXAudioProcessorEditor(NebulaFXAudioProcessor&);
    ~NebulaFXAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    NebulaFXAudioProcessor& audioProcessor;

    // Компоненты управления
    juce::Slider thresholdSlider;
    juce::Slider ratioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider masterGainSlider;

    using APVTSAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<APVTSAttachment> thresholdAttachment;
    std::unique_ptr<APVTSAttachment> ratioAttachment;
    std::unique_ptr<APVTSAttachment> attackAttachment;
    std::unique_ptr<APVTSAttachment> releaseAttachment;
    std::unique_ptr<APVTSAttachment> masterGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NebulaFXAudioProcessorEditor)
};
