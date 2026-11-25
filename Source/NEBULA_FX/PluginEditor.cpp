#include "PluginProcessor.h"
#include "PluginEditor.h"

NebulaFXAudioProcessorEditor::NebulaFXAudioProcessorEditor (NebulaFXAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      apvts(p.apvts) // Предполагается, что у вас есть AudioProcessorValueTreeState
{
    // Включаем масштабирование
    setResizable(true, true);
    
    // Базовые размеры, будут перезаписаны при restoreState
    setSize(800, 600);
    setResizeLimits(600, 450, 1600, 1200);

    // Инициализация компонентов (пример)
    addAndMakeVisible(masterGainKnob);
    masterGainAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(apvts, "MASTER_GAIN", masterGainKnob));
    
    startTimerHz(30); // Для обновления метров
}

void NebulaFXAudioProcessorEditor::paint (juce::Graphics& g)
{
    // 1. Темный металлический фон (Industrial Gothic)
    juce::ColourGradient bgGradient(juce::Colour(0xFF1A1A1A), 0, 0, 
                                    juce::Colour(0xFF0D0D0D), 0, (float)getHeight(), false);
    g.setGradientFill(bgGradient);
    g.fillRect(getLocalBounds());

    // 2. "3D" Вдавленная панель для эффектов
    juce::Rectangle<int> panelArea(20, 20, getWidth() - 40, getHeight() - 40);
    g.setColour(juce::Colour(0xFF111111));
    g.fillRoundedRectangle(panelArea.toFloat(), 12.0f);
    
    // 3D-эффект через тени (внешняя и внутренняя)
    g.setDropShadow(juce::DropShadow(juce::Colours::black.withAlpha(0.6f), 8, juce::Point<int>(0, 4)));
    g.fillRoundedRectangle(panelArea.reduced(2).toFloat(), 10.0f);
    
    // Неоновая подсветка (Red/Purple/Blue accents)
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFFF0055), 0, 20, 
                                           juce::Colour(0xFF5500FF), (float)getWidth(), 20, false));
    g.fillRect(20, 18, getWidth() - 40, 2); // Верхняя акцентная линия
}

void NebulaFXAudioProcessorEditor::resized()
{
    // Адаптивная верстка (Responsive Layout)
    int knobSize = juce::jmin(80, getWidth() / 10);
    
    // Пример размещения: масштабируется вместе с окном
    masterGainKnob.setBounds(40, 60, knobSize, knobSize);
    
    // Сохраняем текущий размер в процессор для последующего getStateInformation
    audioProcessor.setCurrentUIWidth(getWidth());
    audioProcessor.setCurrentUIHeight(getHeight());
}

// --- Интеграция с StateManager ---
void NebulaFXAudioProcessorEditor::saveStateToProcessor() {
    audioProcessor.setCurrentUIWidth(getWidth());
    audioProcessor.setCurrentUIHeight(getHeight());
}
