#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PresetBar.h"
#include "SpectrumDisplay.h"
#include "ReelComponent.h"
#include "NebulaLookAndFeel.h"

class EffectCard : public juce::Component
{
public:
    EffectCard(EffectBase& fx, NebulaAudioProcessor& proc);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    EffectBase& effect;
    juce::ToggleButton onButton;
    juce::Slider mixKnob;
    std::vector<std::unique_ptr<juce::Slider>> paramKnobs;
    std::vector<std::unique_ptr<juce::Label>> paramLabels;
    juce::Label nameLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt;
};

class NebulaAudioProcessorEditor : public juce::AudioProcessorEditor
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
    ReelComponent leftReel, rightReel;
    SpectrumDisplay spectrum;

    juce::Viewport rackViewport;
    juce::Component rackContent;
    std::vector<std::unique_ptr<EffectCard>> effectCards;

    juce::TextEditor aiInput;
    juce::TextButton generateBtn { "GENERATE" };
    juce::Slider inputKnob, mixKnob, outputKnob;
    juce::Label inputLbl, mixLbl, outputLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aIn, aMix, aOut;

    void rebuildRackUI();
};
