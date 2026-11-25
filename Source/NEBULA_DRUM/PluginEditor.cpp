#include "PluginEditor.h"

NebulaDrumAudioProcessorEditor::NebulaDrumAudioProcessorEditor(NebulaDrumAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setResizable(true, true);
    setResizeLimits(700, 500, 1800, 1000);
    setSize(audioProcessor.currentUIWidth, audioProcessor.currentUIHeight);

    // Настройка слайдеров
    auto setupSlider = [](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF00AAFF)); // Neon Blue Accent
        s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF333333));
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    };

    setupSlider(tempoSlider);
    setupSlider(swingSlider);
    setupSlider(volumeSlider);
    addAndMakeVisible(tempoSlider);
    addAndMakeVisible(swingSlider);
    addAndMakeVisible(volumeSlider);

    tempoAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "TEMPO", tempoSlider));
    swingAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "SWING", swingSlider));
    volumeAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "VOLUME", volumeSlider));

    // Настройка кнопок секвенсора
    for (int i = 0; i < 6; ++i) {
        instrumentLabels[i].setText(instrumentNames[i], juce::dontSendNotification);
        instrumentLabels[i].setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
        instrumentLabels[i].setFont(juce::Font(14.0f, juce::Font::bold));
        instrumentLabels[i].setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(instrumentLabels[i]);

        for (int j = 0; j < 16; ++j) {
            stepButtons[i][j].setClickingTogglesState(true);
            // Кастомные цвета для кнопок через LookAndFeel (упрощенно через setColour)
            stepButtons[i][j].setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFFF0055)); // Active: Neon Red
            stepButtons[i][j].setColour(juce::ToggleButton::textColourId, juce::Colours::transparentBlack);
            
            // Лямбда для сохранения состояния при клике
            stepButtons[i][j].onClick = [this, i, j]() {
                audioProcessor.sequencerGrid[i][j] = stepButtons[i][j].getToggleState();
            };
            addAndMakeVisible(stepButtons[i][j]);
        }
    }

    // Инициализация состояния кнопок из процессора
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 16; ++j) {
            stepButtons[i][j].setToggleState(audioProcessor.sequencerGrid[i][j], juce::dontSendNotification);
        }
    }

    startTimerHz(30);
}

NebulaDrumAudioProcessorEditor::~NebulaDrumAudioProcessorEditor() {}

void NebulaDrumAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Фон
    juce::ColourGradient bgGradient(juce::Colour(0xFF151515), 0, 0, 
                                    juce::Colour(0xFF080808), 0, (float)getHeight(), false);
    g.setGradientFill(bgGradient);
    g.fillRect(getLocalBounds());

    // Основная панель
    juce::Rectangle<int> panelArea = getLocalBounds().reduced(20);
    g.setDropShadow(juce::DropShadow(juce::Colours::black.withAlpha(0.8f), 15, juce::Point<int>(0, 8)));
    g.fillRoundedRectangle(panelArea.toFloat(), 12.0f);

    juce::ColourGradient metalGradient(juce::Colour(0xFF1E1E1E), 0, (float)panelArea.getY(),
                                       juce::Colour(0xFF101010), 0, (float)panelArea.getBottom(), false);
    g.setGradientFill(metalGradient);
    g.fillRoundedRectangle(panelArea.reduced(2).toFloat(), 10.0f);

    // Акцентная линия (Purple/Blue)
    juce::ColourGradient accentGradient(juce::Colour(0xFF7000FF), (float)panelArea.getX(), 0,
                                        juce::Colour(0xFF00AAFF), (float)panelArea.getRight(), 0, false);
    g.setGradientFill(accentGradient);
    g.fillRect(panelArea.getX(), panelArea.getY(), panelArea.getWidth(), 3);

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.drawText("NEBULA DRUM", panelArea.removeFromTop(50), juce::Justification::centred);
}

void NebulaDrumAudioProcessorEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(30, 70);
    
    // Верхняя панель управления
    juce::Rectangle<int> controlsArea = area.removeFromTop(120);
    int knobSize = juce::jmin(100, controlsArea.getWidth() / 3 - 40);
    tempoSlider.setBounds(controlsArea.removeFromLeft(knobSize + 20));
    swingSlider.setBounds(controlsArea.removeFromLeft(knobSize + 20));
    volumeSlider.setBounds(controlsArea.removeFromLeft(knobSize + 20));

    // Сетка секвенсора
    int rowHeight = (area.getHeight() - 20) / 6;
    int stepWidth = (area.getWidth() - 100) / 16;
    int labelWidth = 80;

    for (int i = 0; i < 6; ++i) {
        juce::Rectangle<int> rowArea = area.removeFromTop(rowHeight);
        instrumentLabels[i].setBounds(rowArea.removeFromLeft(labelWidth).reduced(0, 5));
        
        for (int j = 0; j < 16; ++j) {
            juce::Rectangle<int> btnArea = rowArea.removeFromLeft(stepWidth).reduced(2, 4);
            stepButtons[i][j].setBounds(btnArea);
            
            // Визуальное разделение групп по 4 шага (как в TR-808)
            if (j % 4 == 3) {
                rowArea.removeFromLeft(4); // Дополнительный отступ
            }
        }
    }

    audioProcessor.setCurrentUIWidth(getWidth());
    audioProcessor.setCurrentUIHeight(getHeight());
}

void NebulaDrumAudioProcessorEditor::timerCallback() {
    repaint();
}
