#include "PluginEditor.h"

// ===== EffectCard =====
EffectCard::EffectCard(EffectBase& fx, NebulaAudioProcessor& proc) : effect(fx)
{
    nameLabel.setText(effect.getDisplayName(), juce::dontSendNotification);
    nameLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFE0E0E0));
    addAndMakeVisible(nameLabel);

    onButton.setButtonText("ON");
    onButton.setToggleState(effect.isEnabled(), juce::dontSendNotification);
    onButton.onClick = [this]() { effect.setEnabled(onButton.getToggleState()); };
    addAndMakeVisible(onButton);

    mixKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    mixKnob.setValue(effect.getMix(), juce::dontSendNotification);
    mixKnob.onValueChange = [this]() { effect.setMix((float)mixKnob.getValue()); };
    addAndMakeVisible(mixKnob);

    for (int i = 0; i < effect.getNumParams(); ++i) {
        auto knob = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        knob->setValue(effect.getParam(i), juce::dontSendNotification);
        knob->onValueChange = [this, i, raw = knob.get()]() { effect.setParam(i, (float)raw->getValue()); };
        addAndMakeVisible(*knob);
        paramKnobs.push_back(std::move(knob));

        auto lbl = std::make_unique<juce::Label>("", effect.getParamName(i));
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setColour(juce::Label::textColourId, juce::Colour(0xFFB0B0B0));
        lbl->setFont(juce::Font(juce::FontOptions(10.0f)));
        addAndMakeVisible(*lbl);
        paramLabels.push_back(std::move(lbl));
    }
    setSize(800, 55);
}

void EffectCard::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xFF161A26).withAlpha(0.9f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 5.0f);
    g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.3f));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 5.0f, 1.0f);
}

void EffectCard::resized()
{
    auto b = getLocalBounds().reduced(6);
    b.removeFromTop(4);
    nameLabel.setBounds(b.removeFromLeft(100));
    onButton.setBounds(b.removeFromRight(40).reduced(0, 10));
    auto mixArea = b.removeFromRight(60);
    mixKnob.setBounds(mixArea);

    int n = (int)paramKnobs.size();
    if (n == 0) return;
    int w = b.getWidth() / n;
    for (int i = 0; i < n; ++i) {
        auto cell = b.removeFromLeft(w);
        paramLabels[i]->setBounds(cell.removeFromTop(12));
        paramKnobs[i]->setBounds(cell);
    }
}

// ===== Editor =====
NebulaAudioProcessorEditor::NebulaAudioProcessorEditor(NebulaAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      presetBar(p.presetManager),
      leftReel(p, true), rightReel(p, false),
      spectrum(p)
{
    setLookAndFeel(&lf);
    setSize(900, 640);
    setResizable(false, false);

    addAndMakeVisible(presetBar);
    addAndMakeVisible(leftReel);
    addAndMakeVisible(rightReel);
    addAndMakeVisible(spectrum);

    rackViewport.setViewedComponent(&rackContent, false);
    rackViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(rackViewport);

    aiInput.setTextToShowWhenEmpty("Describe what you want... e.g. warm pad with space", juce::Colours::grey);
    aiInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF0A0E1A));
    aiInput.setColour(juce::TextEditor::textColourId, juce::Colour(0xFFE0E0E0));
    aiInput.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF00E5FF).withAlpha(0.5f));
    addAndMakeVisible(aiInput);

    generateBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF00E5FF).withAlpha(0.3f));
    generateBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF00E5FF));
    generateBtn.onClick = [this]() {
        // Простейшая "AI" генерация: парсинг ключевых слов
        auto text = aiInput.getText().toLowerCase();
        if (text.contains("space") || text.contains("reverb")) proc.rack.addEffect("reverb");
        if (text.contains("echo") || text.contains("delay")) proc.rack.addEffect("delay");
        if (text.contains("warm")) proc.rack.addEffect("distortion");
        rebuildRackUI();
    };
    addAndMakeVisible(generateBtn);

    // Мастер-ручки
    auto setupMaster = [this](juce::Slider& s, juce::Label& l, const juce::String& text,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att,
                              const juce::String& id) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 14);
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(s); addAndMakeVisible(l);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, s);
    };
    setupMaster(inputKnob, inputLbl, "INPUT", aIn, "INPUT");
    setupMaster(mixKnob, mixLbl, "MIX", aMix, "MIX");
    setupMaster(outputKnob, outputLbl, "OUTPUT", aOut, "OUTPUT");

    rebuildRackUI();
}

NebulaAudioProcessorEditor::~NebulaAudioProcessorEditor() { setLookAndFeel(nullptr); }

void NebulaAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF0A0E1A), 0, 0,
                                           juce::Colour(0xFF1A1F2E), 0, (float)getHeight(), false));
    g.fillAll();
    g.setColour(NebulaLookAndFeel::getCyan());
    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    g.drawText("NEBULA", getWidth() - 150, 5, 130, 30, juce::Justification::centredRight);
}

void NebulaAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);

    // PresetBar
    presetBar.setBounds(area.removeFromTop(30));
    area.removeFromTop(8);

    // Reels + Spectrum
    auto visualRow = area.removeFromTop(140);
    leftReel.setBounds(visualRow.removeFromLeft(120).reduced(10));
    rightReel.setBounds(visualRow.removeFromRight(120).reduced(10));
    spectrum.setBounds(visualRow.reduced(10, 0));
    area.removeFromTop(8);

    // Rack
    auto rackArea = area.removeFromTop(210);
    rackViewport.setBounds(rackArea);
    rackContent.setSize(rackArea.getWidth() - 12, (int)effectCards.size() * 60);
    for (size_t i = 0; i < effectCards.size(); ++i)
        effectCards[i]->setBounds(0, (int)i * 60, rackContent.getWidth(), 55);
    area.removeFromTop(8);

    // AI
    auto aiRow = area.removeFromTop(32);
    generateBtn.setBounds(aiRow.removeFromRight(100));
    aiRow.removeFromRight(6);
    aiInput.setBounds(aiRow);
    area.removeFromTop(6);

    // Мастер-ручки
    auto masterRow = area.removeFromTop(60);
    int kw = masterRow.getWidth() / 3;
    auto pos = [](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(14)); s.setBounds(r);
    };
    pos(inputKnob, inputLbl, masterRow.removeFromLeft(kw).reduced(10));
    pos(mixKnob, mixLbl, masterRow.removeFromLeft(kw).reduced(10));
    pos(outputKnob, outputLbl, masterRow.removeFromLeft(kw).reduced(10));
}

void NebulaAudioProcessorEditor::rebuildRackUI()
{
    effectCards.clear();
    rackContent.removeAllChildren();
    for (int i = 0; i < proc.rack.size(); ++i) {
        if (auto* fx = proc.rack.get(i)) {
            auto card = std::make_unique<EffectCard>(*fx, proc);
            rackContent.addAndMakeVisible(*card);
            effectCards.push_back(std::move(card));
        }
    }
    resized();
}
