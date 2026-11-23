
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
        bool success = false;
    };

    class GenerationEngine
    {
    public:
        static GenerationOutput generate(
            const juce::String& prompt);

    private:
        static bool containsAny(
            const juce::String& text,
            std::initializer_list<const char*> words);
    };
}
