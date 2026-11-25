#include "PluginEditor.h"

NebulaFXAudioProcessorEditor::NebulaFXAudioProcessorEditor(NebulaFXAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // 1. Включаем масштабирование окна (Критично для FL Studio)
    setResizable(true, true);
    setResizeLimits(600, 450, 1600, 1200);
    
    // Восстанавливаем размер из StateManager, если он был сохранен
    setSize(audioProcessor.currentUIWidth, audioProcessor.currentUIHeight);

    // Настройка слайдеров
    auto setupSlider = [](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFFF0055)); // Neon Red Accent
        s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF333333));
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    };

    setupSlider(thresholdSlider);
    setupSlider(ratioSlider);
    setupSlider(attackSlider);
    setupSlider(releaseSlider);
    setupSlider(masterGainSlider);

    addAndMakeVisible(thresholdSlider);
    addAndMakeVisible(ratioSlider);
    addAndMakeVisible(attackSlider);
    addAndMakeVisible(releaseSlider);
    addAndMakeVisible(masterGainSlider);

    thresholdAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "THRESHOLD", thresholdSlider));
    ratioAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "RATIO", ratioSlider));
    attackAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "ATTACK", attackSlider));
    releaseAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "RELEASE", releaseSlider));
    masterGainAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "MASTER_GAIN", masterGainSlider));

    startTimerHz(30);
}

NebulaFXAudioProcessorEditor::~NebulaFXAudioProcessorEditor() {}

void NebulaFXAudioProcessorEditor::paint(juce::Graphics& g)
{
    // 1. Глубокий темный металлический фон (Industrial Gothic)
    juce::ColourGradient bgGradient(juce::Colour(0xFF1A1A1A), 0, 0, 
                                    juce::Colour(0xFF0A0A0A), 0, (float)getHeight(), false);
    g.setGradientFill(bgGradient);
    g.fillRect(getLocalBounds());

    // 2. "3D" Вдавленная основная панель
    juce::Rectangle<int> panelArea = getLocalBounds().reduced(20);
    
    // Внешняя тень для эффекта "парения"
    g.setDropShadow(juce::DropShadow(juce::Colours::black.withAlpha(0.7f), 12, juce::Point<int>(0, 6)));
    g.fillRoundedRectangle(panelArea.toFloat(), 16.0f);

    // Внутренний градиент для эффекта металлической поверхности
    juce::ColourGradient metalGradient(juce::Colour(0xFF222222), 0, (float)panelArea.getY(),
                                       juce::Colour(0xFF111111), 0, (float)panelArea.getBottom(), false);
    g.setGradientFill(metalGradient);
    g.fillRoundedRectangle(panelArea.reduced(2).toFloat(), 14.0f);

    // 3. Неоновая акцентная линия сверху (Red/Purple)
    juce::ColourGradient accentGradient(juce::Colour(0xFFFF0055), (float)panelArea.getX(), 0,
                                        juce::Colour(0xFF7000FF), (float)panelArea.getRight(), 0, false);
    g.setGradientFill(accentGradient);
    g.fillRect(panelArea.getX(), panelArea.getY(), panelArea.getWidth(), 3);

    // 4. Заголовок
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(juce::Font(24.0f, juce::Font::bold));
    g.drawText("NEBULA FX", panelArea.removeFromTop(60), juce::Justification::centred);
}

void NebulaFXAudioProcessorEditor::resized()
{
    // Responsive Layout: элементы масштабируются пропорционально размеру окна
    juce::Rectangle<int> area = getLocalBounds().reduced(30, 80);
    
    int knobSize = juce::jmin(120, area.getWidth() / 5 - 20);
    int spacing = (area.getWidth() - (knobSize * 5)) / 6;

    auto placeKnob = [&](juce::Slider& slider, juce::String name) {
        slider.setBounds(area.removeFromLeft(knobSize));
        area.removeFromLeft(spacing);
    };

    placeKnob(thresholdSlider, "Threshold");
    placeKnob(ratioSlider, "Ratio");
    placeKnob(attackSlider, "Attack");
    placeKnob(releaseSlider, "Release");
    placeKnob(masterGainSlider, "Gain");

    // Сохраняем текущий размер для StateManager (чтобы FL Studio запомнила масштаб)
    audioProcessor.setCurrentUIWidth(getWidth());
    audioProcessor.setCurrentUIHeight(getHeight());
}

void NebulaFXAudioProcessorEditor::timerCallback() {
    // Здесь можно обновлять визуализацию (например, аудио-метры) без нагрузки на CPU
    repaint();
}
