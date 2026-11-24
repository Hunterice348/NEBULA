
#include "PluginEditor.h"

NebulaFXAudioProcessorEditor::NebulaFXAudioProcessorEditor(
    NebulaFXAudioProcessor& processor)
    : AudioProcessorEditor(&processor),
      audioProcessor(processor)
{
    setSize(760, 480);
    setResizable(true, true);
    setResizeLimits(600, 400, 1400, 900);

    titleLabel.setText("NEBULA FX", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(32.0f, juce::Font::bold)));
    titleLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xffff354f)
    );
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText(
        "INDUSTRIAL MODULAR EFFECT PROCESSOR",
        juce::dontSendNotification
    );
    subtitleLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    subtitleLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xffa7a7b5)
    );
    addAndMakeVisible(subtitleLabel);

    configureSlider(inputSlider, "INPUT");
    configureSlider(outputSlider, "OUTPUT");
    configureSlider(mixSlider, "WET / DRY");
    configureSlider(driveSlider, "DRIVE");
    configureSlider(delaySlider, "DELAY MS");
    configureSlider(feedbackSlider, "FEEDBACK");

    addAndMakeVisible(modeBox);
    modeBox.addItem("CLEAN", 1);
    modeBox.addItem("DELAY", 2);
    modeBox.addItem("SATURATION", 3);

    auto& state = audioProcessor.parameters;

    inputAttachment = std::make_unique<SliderAttachment>(
        state, "input", inputSlider
    );

    outputAttachment = std::make_unique<SliderAttachment>(
        state, "output", outputSlider
    );

    mixAttachment = std::make_unique<SliderAttachment>(
        state, "mix", mixSlider
    );

    driveAttachment = std::make_unique<SliderAttachment>(
        state, "drive", driveSlider
    );

    delayAttachment = std::make_unique<SliderAttachment>(
        state, "delay", delaySlider
    );

    feedbackAttachment = std::make_unique<SliderAttachment>(
        state, "feedback", feedbackSlider
    );

    modeAttachment = std::make_unique<ComboAttachment>(
        state, "mode", modeBox
    );
}

void NebulaFXAudioProcessorEditor::configureSlider(
    juce::Slider& slider,
    const juce::String& name)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(
        juce::Slider::TextBoxBelow,
        false,
        90,
        22
    );

    slider.setName(name);
    slider.setColour(
        juce::Slider::rotarySliderFillColourId,
        juce::Colour(0xffff354f)
    );
    slider.setColour(
        juce::Slider::rotarySliderOutlineColourId,
        juce::Colour(0xff343440)
    );
    slider.setColour(
        juce::Slider::thumbColourId,
        juce::Colour(0xffe7e7ed)
    );
    slider.setColour(
        juce::Slider::textBoxTextColourId,
        juce::Colours::white
    );
    slider.setColour(
        juce::Slider::textBoxBackgroundColourId,
        juce::Colour(0xff17171f)
    );

    addAndMakeVisible(slider);
}

void NebulaFXAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101016));

    g.setColour(juce::Colour(0xff24242f));
    g.fillRoundedRectangle(
        getLocalBounds().toFloat().reduced(12.0f),
        14.0f
    );

    g.setColour(juce::Colour(0xffff354f));
    g.fillRect(24, 100, getWidth() - 48, 2);

    g.setColour(juce::Colour(0xff343440));
    g.drawRoundedRectangle(
        getLocalBounds().toFloat().reduced(12.0f),
        14.0f,
        1.0f
    );

    g.setColour(juce::Colour(0xffa7a7b5));
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText(
        "HUNTERICE RECORDS  /  AUDIO TECHNOLOGY",
        28,
        getHeight() - 32,
        getWidth() - 56,
        18,
        juce::Justification::centredLeft
    );
}

void NebulaFXAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);

    auto header = area.removeFromTop(70);

    titleLabel.setBounds(header.removeFromTop(38));
    subtitleLabel.setBounds(header);

    area.removeFromTop(24);

    auto modeArea = area.removeFromTop(42);
    modeBox.setBounds(modeArea.removeFromLeft(220));

    area.removeFromTop(18);

    const int rowHeight = area.getHeight() / 2;
    auto topRow = area.removeFromTop(rowHeight);
    auto bottomRow = area;

    const int knobWidth = area.getWidth() / 3;

    inputSlider.setBounds(topRow.removeFromLeft(knobWidth));
    outputSlider.setBounds(topRow.removeFromLeft(knobWidth));
    mixSlider.setBounds(topRow);

    driveSlider.setBounds(bottomRow.removeFromLeft(knobWidth));
    delaySlider.setBounds(bottomRow.removeFromLeft(knobWidth));
    feedbackSlider.setBounds(bottomRow);
}
