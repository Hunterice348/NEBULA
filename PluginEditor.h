
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PresetBar.h"
#include "SpectrumDisplay.h"
#include "ReelComponent.h"
#include "NebulaLookAndFeel.h"

// Карточка эффекта работает через Rack API,
// а не хранит ссылку на объект DSP-эффекта.
class EffectCard : public juce::Component
{
public:
    EffectCard(
        Rack& rack,
        int effectIndex,
        const Rack::EffectInfo& info);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    Rack& rack;
    int index = -1;

    juce::Label nameLabel;
    juce::ToggleButton onButton { "ON" };
    juce::Slider mixKnob;

    std::vector<std::unique_ptr<juce::Slider>> paramKnobs;
    std::vector<std::unique_ptr<juce::Label>> paramLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectCard)
};

class NebulaAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit NebulaAudioProcessorEditor(NebulaAudioProcessor&);
    ~NebulaAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    NebulaAudioProcessor& proc;
    NebulaLookAndFeel lf;

    PresetBar presetBar;
    ReelComponent leftReel;
    ReelComponent rightReel;
    SpectrumDisplay spectrum;

    juce::Viewport rackViewport;
    juce::Component rackContent;

    std::vector<std::unique_ptr<EffectCard>> effectCards;

    juce::TextEditor aiInput;
    juce::TextButton generateBtn { "GENERATE" };

    juce::Slider inputKnob;
    juce::Slider mixKnob;
    juce::Slider outputKnob;

    juce::Label inputLbl;
    juce::Label mixLbl;
    juce::Label outputLbl;

    std::unique_ptr<
        juce::AudioProcessorValueTreeState::SliderAttachment
    > aIn, aMix, aOut;

    uint64_t lastRackRevision = 0;

    void rebuildRackUI();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        NebulaAudioProcessorEditor
    )
};
