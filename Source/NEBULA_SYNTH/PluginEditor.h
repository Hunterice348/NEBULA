#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class NebulaSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit NebulaSynthAudioProcessorEditor(NebulaSynthAudioProcessor&);
    ~NebulaSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    NebulaSynthAudioProcessor& audioProcessor;

    juce::Slider oscMixSlider;
    juce::Slider cutoffSlider;
    juce::Slider resonanceSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider masterVolSlider;

    using APVTSAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<APVTSAttachment> oscMixAttachment;
    std::unique_ptr<APVTSAttachment> cutoffAttachment;
    std::unique_ptr<APVTSAttachment> resonanceAttachment;
    std::unique_ptr<APVTSAttachment> attackAttachment;
    std::unique_ptr<APVTSAttachment> releaseAttachment;
    std::unique_ptr<APVTSAttachment> masterVolAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NebulaSynthAudioProcessorEditor)
};
