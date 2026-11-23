
#pragma once

#include <JuceHeader.h>
#include <initializer_list>

namespace nebula
{
    struct GenerationOutput
    {
        juce::ValueTree rackState { "RACK" };

        juce::StringArray effectNames;

        juce::String summary;
        juce::String detectedGenre;
        juce::String detectedIntent;

        bool success = false;
    };

    class GenerationEngine
    {
    public:
        // Local semantic music-generation engine.
        // Safe to call from the UI/control thread.
        static GenerationOutput generate(
            const juce::String& prompt);

    private:
        static bool containsAny(
            const juce::String& text,
            std::initializer_list<const char*> words);
    };
}
