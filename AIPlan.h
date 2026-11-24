#pragma once

#include <JuceHeader.h>

#include <vector>

namespace nebula
{

struct AIPlanEffect
{
    juce::String typeID;
    bool enabled = true;
    float mix = 1.0f;
    std::vector<float> parameters;
};

struct AIPlan
{
    static constexpr int currentVersion = 1;

    int version = currentVersion;
    juce::String description;
    std::vector<AIPlanEffect> effects;
};

struct AIPlanValidation
{
    bool success = false;

    AIPlan plan;
    juce::ValueTree rackState { "RACK" };

    juce::StringArray errors;
    juce::StringArray warnings;

    juce::String getReport() const;
};

class AIPlanValidator
{
public:
    static constexpr int maximumEffects = 16;

    // Parses JSON, validates its structure and checks all effect values.
    // This function is intended for the message/control thread.
    static AIPlanValidation parseAndValidate(
        const juce::String& json);

    // Validates an already constructed plan.
    static bool validate(
        AIPlan& plan,
        juce::StringArray& errors,
        juce::StringArray& warnings);

    // Converts a validated plan into the Rack ValueTree format.
    static juce::ValueTree toRackState(
        const AIPlan& plan);

    // Returns the supported JSON format description.
    static juce::String getSchemaDescription();

    // Returns the expected parameter count for an effect.
    // Returns -1 for unsupported effects.
    static int getParameterCount(
        const juce::String& typeID);

private:
    static bool readNumber(
        const juce::var& value,
        double& result);

    static bool readBoolean(
        const juce::var& value,
        bool& result);
};

} // namespace nebula