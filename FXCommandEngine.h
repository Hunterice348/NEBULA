
#pragma once

#include <JuceHeader.h>

namespace nebula
{
    struct FXCommandResult
    {
        juce::ValueTree rackState { "RACK" };
        juce::String summary;
        bool recognized = false;
        bool success = false;
        bool changed = false;
    };

    class FXCommandEngine
    {
    public:
        // Processes an instruction against a copy of the current rack.
        // The original rack is never modified by this class.
        static FXCommandResult execute(
            const juce::String& command,
            const juce::ValueTree& currentRack);
    };
}
