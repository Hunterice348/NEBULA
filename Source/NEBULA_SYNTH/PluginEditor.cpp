#include "PluginEditor.h"

NebulaSynthAudioProcessorEditor::NebulaSynthAudioProcessorEditor(NebulaSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setResizable(true, true);
    setResizeLimits(650, 500, 1600, 1100);
    setSize(audioProcessor.currentUIWidth, audioProcessor.currentUIHeight);

    auto setupSlider = [](juce::Slider& s, juce::Colour accent) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
        s.setColour(juce::Slider::rotarySliderFillColourId, accent);
        s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF333333));
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    };

    setupSlider(oscMixSlider, juce::Colour(0xFF00FF88));   // Neon Green
    setupSlider(cutoffSlider, juce::Colour(0xFF00FF88));
    setupSlider(resonanceSlider, juce::Colour(0xFF00FF88));
    setupSlider(attackSlider, juce::Colour(0xFF00FF88));
    setupSlider(releaseSlider, juce::Colour(0xFF00FF88));
    setupSlider(masterVolSlider, juce::Colour(0xFF00FF88));

    addAndMakeVisible(oscMixSlider);
    addAndMakeVisible(cutoffSlider);
    addAndMakeVisible(resonanceSlider);
    addAndMakeVisible(attackSlider);
    addAndMakeVisible(releaseSlider);
    addAndMakeVisible(masterVolSlider);

    oscMixAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "OSC_MIX", oscMixSlider));
    cutoffAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "CUTOFF", cutoffSlider));
    resonanceAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "RESONANCE", resonanceSlider));
    attackAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "ATTACK", attackSlider));
    releaseAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "RELEASE", releaseSlider));
    masterVolAttachment.reset(new APVTSAttachment(audioProcessor.apvts, "MASTER_VOL", masterVolSlider));

    startTimerHz(30);
}

NebulaSynthAudioProcessorEditor::~NebulaSynthAudioProcessorEditor() {}

void NebulaSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Темный фон
    juce::ColourGradient bgGradient(juce::Colour(0xFF121212), 0, 0, 
                                    juce::Colour(0xFF050505), 0, (float)getHeight(), false);
    g.setGradientFill(bgGradient);
    g.fillRect(getLocalBounds());

    // 3D Панель
    juce::Rectangle<int> panelArea = getLocalBounds().reduced(20);
    g.setDropShadow(juce::DropShadow(juce::Colours::black.withAlpha(0.8f), 15, juce::Point<int>(0, 8)));
    g.fillRoundedRectangle(panelArea.toFloat(), 12.0f);

    juce::ColourGradient metalGradient(juce::Colour(0xFF1A1A1A), 0, (float)panelArea.getY(),
                                       juce::Colour(0xFF0D0D0D), 0, (float)panelArea.getBottom(), false);
    g.setGradientFill(metalGradient);
    g.fillRoundedRectangle(panelArea.reduced(2).toFloat(), 10.0f);

    // Акцентная линия (Neon Green/Cyan для Synth)
    juce::ColourGradient accentGradient(juce::Colour(0xFF00FF88), (float)panelArea.getX(), 0,
                                        juce::Colour(0xFF00AAFF), (float)panelArea.getRight(), 0, false);
    g.setGradientFill(accentGradient);
    g.fillRect(panelArea.getX(), panelArea.getY(), panelArea.getWidth(), 3);

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.drawText("NEBULA SYNTH", panelArea.removeFromTop(50), juce::Justification::centred);
}

void NebulaSynthAudioProcessorEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(30, 70);
    
    // Группировка элементов
    juce::Rectangle<int> oscArea = area.removeFromTop(150);
    juce::Rectangle<int> filterArea = area.removeFromTop(150);
    juce::Rectangle<int> envArea = area.removeFromTop(150);
    
    auto layoutRow = [](juce::Rectangle<int>& rowArea, std::vector<std::reference_wrapper<juce::Slider>> sliders) {
        int knobSize = juce::jmin(100, rowArea.getWidth() / static_cast<int>(sliders.size()) - 20);
        for (auto& s : sliders) {
            s.get().setBounds(rowArea.removeFromLeft(knobSize + 20));
        }
    };

    layoutRow(oscArea, { oscMixSlider });
    layoutRow(filterArea, { cutoffSlider, resonanceSlider });
    layoutRow(envArea, { attackSlider, releaseSlider, masterVolSlider });

    audioProcessor.setCurrentUIWidth(getWidth());
    audioProcessor.setCurrentUIHeight(getHeight());
}

void NebulaSynthAudioProcessorEditor::timerCallback() {
    repaint();
}
