#include "FXCommandEngine.h"
#include "PluginEditor.h"
#include "GenerationEngine.h"

// ============================================================
// EFFECT CARD
// ============================================================

EffectCard::EffectCard(
    Rack& rackRef,
    int effectIndex,
    const Rack::EffectInfo& info)
    : rack(rackRef),
      index(effectIndex)
{
    nameLabel.setText(
        info.displayName,
        juce::dontSendNotification);

    nameLabel.setFont(
        juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));

    nameLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xFFE0E0E0));

    addAndMakeVisible(nameLabel);

    onButton.setToggleState(
        info.enabled,
        juce::dontSendNotification);

    onButton.onClick = [this]()
    {
        rack.setEffectEnabled(
            index,
            onButton.getToggleState());
    };

    addAndMakeVisible(onButton);

    mixKnob.setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag);

    mixKnob.setTextBoxStyle(
        juce::Slider::TextBoxBelow,
        false,
        48,
        14);

    mixKnob.setRange(0.0, 1.0, 0.001);
    mixKnob.setValue(info.mix, juce::dontSendNotification);
    mixKnob.setTooltip("Effect wet/dry mix");

    mixKnob.onValueChange = [this]()
    {
        rack.setEffectMix(
            index,
            static_cast<float>(mixKnob.getValue()));
    };

    addAndMakeVisible(mixKnob);

    const int parameterCount =
        static_cast<int>(info.parameterNames.size());

    paramKnobs.reserve(static_cast<size_t>(parameterCount));
    paramLabels.reserve(static_cast<size_t>(parameterCount));

    for (int i = 0; i < parameterCount; ++i)
    {
        auto knob = std::make_unique<juce::Slider>();

        knob->setSliderStyle(
            juce::Slider::RotaryHorizontalVerticalDrag);

        knob->setTextBoxStyle(
            juce::Slider::TextBoxBelow,
            false,
            48,
            14);

        knob->setRange(0.0, 1.0, 0.001);

        const float value =
            i < static_cast<int>(info.parameterValues.size())
                ? info.parameterValues[static_cast<size_t>(i)]
                : 0.5f;

        knob->setValue(
            value,
            juce::dontSendNotification);

        knob->setTooltip(
            info.parameterNames[static_cast<size_t>(i)]);

        knob->onValueChange = [this, i, raw = knob.get()]()
        {
            rack.setEffectParameter(
                index,
                i,
                static_cast<float>(raw->getValue()));
        };

        addAndMakeVisible(*knob);
        paramKnobs.push_back(std::move(knob));

        auto label = std::make_unique<juce::Label>();

        label->setText(
            info.parameterNames[static_cast<size_t>(i)],
            juce::dontSendNotification);

        label->setJustificationType(
            juce::Justification::centred);

        label->setColour(
            juce::Label::textColourId,
            juce::Colour(0xFFB0B8C8));

        label->setFont(
            juce::Font(juce::FontOptions(10.0f)));

        addAndMakeVisible(*label);
        paramLabels.push_back(std::move(label));
    }

    setSize(850, 66);
}

void EffectCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colour(0xFF161A26));
    g.fillRoundedRectangle(bounds, 7.0f);

    g.setColour(
        NebulaLookAndFeel::getCyan().withAlpha(0.35f));

    g.drawRoundedRectangle(bounds, 7.0f, 1.0f);

    g.setColour(
        NebulaLookAndFeel::getViolet().withAlpha(0.12f));

    g.fillRoundedRectangle(
        bounds.removeFromTop(2.0f),
        2.0f);
}

void EffectCard::resized()
{
    auto area = getLocalBounds().reduced(7);

    nameLabel.setBounds(
        area.removeFromLeft(105));

    onButton.setBounds(
        area.removeFromRight(42).reduced(0, 12));

    mixKnob.setBounds(
        area.removeFromRight(62));

    const int count =
        static_cast<int>(paramKnobs.size());

    if (count <= 0)
        return;

    const int cellWidth =
        juce::jmax(1, area.getWidth() / count);

    for (int i = 0; i < count; ++i)
    {
        auto cell = area.removeFromLeft(cellWidth);

        paramLabels[static_cast<size_t>(i)]->setBounds(
            cell.removeFromTop(13));

        paramKnobs[static_cast<size_t>(i)]->setBounds(cell);
    }
}


// ============================================================
// EDITOR
// ============================================================

NebulaAudioProcessorEditor::NebulaAudioProcessorEditor(
    NebulaAudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor),
      proc(processor),
      presetBar(processor.presetManager),
      leftReel(processor, true),
      rightReel(processor, false),
      spectrum(processor)
{
    setLookAndFeel(&lf);

    setSize(900, 640);
    setResizable(true, true);
    setResizeLimits(760, 560, 1600, 1100);

    addAndMakeVisible(presetBar);
    addAndMakeVisible(leftReel);
    addAndMakeVisible(rightReel);
    addAndMakeVisible(spectrum);

    rackViewport.setViewedComponent(
        &rackContent,
        false);

    rackViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(rackViewport);

    // --------------------------------------------------------
    // GENERATION ENGINE
    // --------------------------------------------------------

    aiInput.setTextToShowWhenEmpty(
        "Describe a sound: dark industrial techno, ambient space...",
        juce::Colours::grey);

    aiInput.setColour(
        juce::TextEditor::backgroundColourId,
        juce::Colour(0xFF0A0E1A));

    aiInput.setColour(
        juce::TextEditor::textColourId,
        juce::Colour(0xFFE0E0E0));

    aiInput.setColour(
        juce::TextEditor::outlineColourId,
        NebulaLookAndFeel::getCyan().withAlpha(0.5f));

    addAndMakeVisible(aiInput);

    generateBtn.setColour(
        juce::TextButton::buttonColourId,
        juce::Colour(0xFF172B3A));

    generateBtn.setColour(
        juce::TextButton::textColourOffId,
        NebulaLookAndFeel::getCyan());

    generateBtn.onClick = [this]()
    {
        const auto result =
            nebula::GenerationEngine::generate(
                aiInput.getText());

        if (!result.success)
        {
            aiInput.setTooltip(result.summary);
            return;
        }

        proc.rack.loadState(result.rackState);

        rebuildRackUI();

        aiInput.setTooltip(result.summary);

        generateBtn.setButtonText(
            "GENERATED: " +
            juce::String(result.effectNames.size()));
    };

    addAndMakeVisible(generateBtn);

    // --------------------------------------------------------
    // MASTER CONTROLS
    // --------------------------------------------------------

    auto setupMaster = [this](
        juce::Slider& slider,
        juce::Label& label,
        const juce::String& title,
        std::unique_ptr<
            juce::AudioProcessorValueTreeState::SliderAttachment
        >& attachment,
        const juce::String& parameterID)
    {
        slider.setSliderStyle(
            juce::Slider::RotaryHorizontalVerticalDrag);

        slider.setTextBoxStyle(
            juce::Slider::TextBoxBelow,
            false,
            58,
            15);

        label.setText(
            title,
            juce::dontSendNotification);

        label.setJustificationType(
            juce::Justification::centred);

        label.setColour(
            juce::Label::textColourId,
            NebulaLookAndFeel::getCyan());

        addAndMakeVisible(slider);
        addAndMakeVisible(label);

        attachment =
            std::make_unique<
                juce::AudioProcessorValueTreeState::SliderAttachment
            >(
                proc.apvts,
                parameterID,
                slider);
    };

    setupMaster(
        inputKnob,
        inputLbl,
        "INPUT",
        aIn,
        "INPUT");

    setupMaster(
        mixKnob,
        mixLbl,
        "GLOBAL MIX",
        aMix,
        "MIX");

    setupMaster(
        outputKnob,
        outputLbl,
        "OUTPUT",
        aOut,
        "OUTPUT");

    rebuildRackUI();

    startTimerHz(5);
}

NebulaAudioProcessorEditor::~NebulaAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void NebulaAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient gradient(
        juce::Colour(0xFF080B14),
        0.0f,
        0.0f,
        juce::Colour(0xFF1A2030),
        0.0f,
        static_cast<float>(getHeight()),
        false);

    g.setGradientFill(gradient);
    g.fillAll();

    g.setColour(NebulaLookAndFeel::getCyan());

    g.setFont(
        juce::Font(juce::FontOptions(23.0f, juce::Font::bold)));

    g.drawText(
        "NEBULA",
        getWidth() - 160,
        5,
        145,
        30,
        juce::Justification::centredRight);

    g.setColour(
        NebulaLookAndFeel::getViolet().withAlpha(0.5f));

    g.drawHorizontalLine(
        39,
        10.0f,
        static_cast<float>(getWidth() - 10));
}

void NebulaAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);

    presetBar.setBounds(
        area.removeFromTop(30));

    area.removeFromTop(8);

    auto visualRow = area.removeFromTop(140);

    leftReel.setBounds(
        visualRow.removeFromLeft(120).reduced(8));

    rightReel.setBounds(
        visualRow.removeFromRight(120).reduced(8));

    spectrum.setBounds(
        visualRow.reduced(8, 0));

    area.removeFromTop(8);

    auto rackArea = area.removeFromTop(
        juce::jmax(150, area.getHeight() - 175));

    rackViewport.setBounds(rackArea);

    const int rackWidth =
        juce::jmax(100, rackArea.getWidth() - 14);

    const int cardHeight = 72;

    rackContent.setSize(
        rackWidth,
        juce::jmax(
            rackArea.getHeight(),
            static_cast<int>(effectCards.size()) * cardHeight));

    for (size_t i = 0; i < effectCards.size(); ++i)
    {
        effectCards[i]->setBounds(
            0,
            static_cast<int>(i) * cardHeight,
            rackWidth,
            66);
    }

    area.removeFromTop(7);

    auto aiRow = area.removeFromTop(32);

    generateBtn.setBounds(
        aiRow.removeFromRight(105));

    aiRow.removeFromRight(7);

    aiInput.setBounds(aiRow);

    area.removeFromTop(5);

    auto masterRow = area.removeFromTop(
        juce::jmin(76, area.getHeight()));

    const int knobWidth =
        masterRow.getWidth() / 3;

    auto placeMaster = [](
        juce::Slider& slider,
        juce::Label& label,
        juce::Rectangle<int> bounds)
    {
        bounds = bounds.reduced(8, 0);

        label.setBounds(
            bounds.removeFromTop(15));

        slider.setBounds(bounds);
    };

    placeMaster(
        inputKnob,
        inputLbl,
        masterRow.removeFromLeft(knobWidth));

    placeMaster(
        mixKnob,
        mixLbl,
        masterRow.removeFromLeft(knobWidth));

    placeMaster(
        outputKnob,
        outputLbl,
        masterRow);
}


// ============================================================
// RACK UI
// ============================================================

void NebulaAudioProcessorEditor::rebuildRackUI()
{
    effectCards.clear();
    rackContent.removeAllChildren();

    const int count = proc.rack.size();

    for (int i = 0; i < count; ++i)
    {
        Rack::EffectInfo info;

        if (!proc.rack.getEffectInfo(i, info))
            continue;

        auto card = std::make_unique<EffectCard>(
            proc.rack,
            i,
            info);

        rackContent.addAndMakeVisible(*card);

        effectCards.push_back(std::move(card));
    }

    lastRackRevision = proc.rack.getRevision();

    resized();
}

void NebulaAudioProcessorEditor::timerCallback()
{
    const auto currentRevision =
        proc.rack.getRevision();

    if (currentRevision != lastRackRevision)
        rebuildRackUI();
}
