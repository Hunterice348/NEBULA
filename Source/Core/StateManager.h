#pragma once
#include <JuceHeader.h>

namespace Nebula {
    class StateManager {
    public:
        static constexpr int CURRENT_VERSION = 1;

        static void saveState(juce::AudioProcessorValueTreeState& apvts, juce::MemoryBlock& destData, int currentWidth, int currentHeight) {
            juce::ValueTree root = apvts.copyState();
            
            // Сохраняем размеры UI для масштабирования
            juce::ValueTree uiState("UIState");
            uiState.setProperty("windowWidth", currentWidth, nullptr);
            uiState.setProperty("windowHeight", currentHeight, nullptr);
            
            if (juce::ValueTree existing = root.getChildWithName("UIState"); existing.isValid()) {
                root.removeChild(existing, nullptr);
            }
            root.appendChild(uiState, nullptr);

            std::unique_ptr<juce::XmlElement> xml(root.createXml());
            if (xml) {
                destData.replaceAll(xml->createDocumentToString("", true).toRawUTF8());
            }
        }

        static void restoreState(juce::AudioProcessorValueTreeState& apvts, const void* data, int sizeInBytes, int& restoredWidth, int& restoredHeight) {
            if (sizeInBytes > 0) {
                juce::String xmlString(static_cast<const char*>(data), sizeInBytes);
                std::unique_ptr<juce::XmlElement> xmlState(juce::XmlDocument::parse(xmlString));
                
                if (xmlState && xmlState->hasTagName(apvts.state.getType())) {
                    juce::ValueTree restoredState = juce::ValueTree::fromXml(*xmlState);
                    
                    // Восстанавливаем размеры UI
                    if (juce::ValueTree ui = restoredState.getChildWithName("UIState"); ui.isValid()) {
                        restoredWidth = ui.getProperty("windowWidth", 800);
                        restoredHeight = ui.getProperty("windowHeight", 600);
                        restoredState.removeChild(ui, nullptr); // Удаляем, чтобы не мешал APVTS
                    }
                    
                    apvts.replaceState(restoredState);
                }
            }
        }
    };
}
