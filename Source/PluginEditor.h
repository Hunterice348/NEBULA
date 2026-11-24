
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class NebulaFXAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit NebulaFXAudioProcessorEditor(NebulaFXAudioProcessor&);
    ~NebulaFXAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    NebulaFXAudioProcessor& audioProcessor;

    juce::Slider inputSlider;
    juce::Slider outputSlider;
    juce::Slider mixSlider;
    juce::Slider driveSlider;
    juce::Slider delaySlider;
    juce::Slider feedbackSlider;

    juce::ComboBox modeBox;

    juce::Label titleLabel;
    juce::Label subtitleLabel;

    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;

    using ComboAttachment =
        juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> inputAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> delayAttachment;
    std::unique_ptr<SliderAttachment> feedbackAttachment;
    std::unique_ptr<ComboAttachment> modeAttachment;

    void configureSlider(juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        NebulaFXAudioProcessorEditor
    )
};
