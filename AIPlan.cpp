
#include "AIPlan.h"

#include <cmath>
#include <limits>
#include <utility>

namespace nebula
{
namespace
{
constexpr int minimumPlanVersion = 1;
constexpr int maximumPlanVersion = AIPlan::currentVersion;

bool isSupportedType(const juce::String& typeID)
{
    return AIPlanValidator::getParameterCount(
        typeID.trim().toLowerCase()) >= 0;
}

juce::String parameterProperty(int index)
{
    return "p" + juce::String(index);
}
}

// ============================================================
// REPORT
// ============================================================

juce::String AIPlanValidation::getReport() const
{
    juce::String report = success
        ? "AI plan validation successful."
        : "AI plan validation failed.";

    for (const auto& warning : warnings)
        report += "\nWarning: " + warning;

    for (const auto& error : errors)
        report += "\nError: " + error;

    return report;
}

// ============================================================
// EFFECT PARAMETER COUNTS — ALL 18 EFFECTS
// ============================================================

int AIPlanValidator::getParameterCount(const juce::String& rawTypeID)
{
    const auto typeID = rawTypeID.trim().toLowerCase();

    if (typeID == "reverb")          return 3;
    if (typeID == "delay")           return 2;
    if (typeID == "distortion")      return 2;
    if (typeID == "chorus")          return 2;
    if (typeID == "phaser")          return 3;
    if (typeID == "compressor")      return 3;
    if (typeID == "bitcrusher")      return 2;
    if (typeID == "filter")          return 2;

    if (typeID == "stereo_widener")  return 2;
    if (typeID == "autopan")         return 2;
    if (typeID == "noise_gate")      return 2;
    if (typeID == "transient")       return 2;
    if (typeID == "tremolo")         return 2;
    if (typeID == "ring_mod")        return 2;
    if (typeID == "comb_filter")     return 2;
    if (typeID == "tape_saturation") return 2;
    if (typeID == "lofi")            return 2;
    if (typeID == "haas_delay")      return 2;

    return -1;
}

// ============================================================
// JSON VALUE HELPERS
// ============================================================

bool AIPlanValidator::readNumber(
    const juce::var& value,
    double& result)
{
    if (value.isBool())
        return false;

    if (!value.isInt()
        && !value.isInt64()
        && !value.isDouble())
        return false;

    result = static_cast<double>(value);
    return std::isfinite(result);
}

bool AIPlanValidator::readBoolean(
    const juce::var& value,
    bool& result)
{
    if (!value.isBool())
        return false;

    result = static_cast<bool>(value);
    return true;
}

// ============================================================
// PLAN VALIDATION
// ============================================================

bool AIPlanValidator::validate(
    AIPlan& plan,
    juce::StringArray& errors,
    juce::StringArray& warnings)
{
    errors.clear();
    warnings.clear();

    if (plan.version < minimumPlanVersion
        || plan.version > maximumPlanVersion)
    {
        errors.add(
            "Unsupported plan version: "
            + juce::String(plan.version));
        return false;
    }

    if (plan.effects.size() > static_cast<size_t>(maximumEffects))
    {
        errors.add(
            "The plan exceeds the maximum number of effects ("
            + juce::String(maximumEffects) + ").");
        return false;
    }

    if (plan.description.length() > 512)
    {
        plan.description = plan.description.substring(0, 512);
        warnings.add("Description was truncated to 512 characters.");
    }

    for (size_t effectIndex = 0;
         effectIndex < plan.effects.size();
         ++effectIndex)
    {
        auto& effect = plan.effects[effectIndex];

        const juce::String prefix =
            "Effect "
            + juce::String(static_cast<int>(effectIndex + 1))
            + ": ";

        effect.typeID = effect.typeID.trim().toLowerCase();

        if (!isSupportedType(effect.typeID))
        {
            errors.add(
                prefix + "unsupported effect type '" + effect.typeID + "'.");
            continue;
        }

        if (!std::isfinite(effect.mix)
            || effect.mix < 0.0f
            || effect.mix > 1.0f)
        {
            errors.add(prefix + "mix must be finite and between 0.0 and 1.0.");
            continue;
        }

        const int expectedParameters = getParameterCount(effect.typeID);

        if (effect.parameters.size()
            != static_cast<size_t>(expectedParameters))
        {
            errors.add(
                prefix
                + "expected " + juce::String(expectedParameters)
                + " parameters, received "
                + juce::String(static_cast<int>(effect.parameters.size()))
                + ".");
            continue;
        }

        bool parametersValid = true;

        for (size_t parameterIndex = 0;
             parameterIndex < effect.parameters.size();
             ++parameterIndex)
        {
            const float value = effect.parameters[parameterIndex];

            if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
            {
                errors.add(
                    prefix
                    + "parameter "
                    + juce::String(static_cast<int>(parameterIndex))
                    + " must be finite and between 0.0 and 1.0.");

                parametersValid = false;
                break;
            }
        }

        if (!parametersValid)
            continue;
    }

    return errors.isEmpty();
}

// ============================================================
// JSON PARSING — JUCE 8 COMPATIBLE
// ============================================================

AIPlanValidation AIPlanValidator::parseAndValidate(
    const juce::String& json)
{
    AIPlanValidation result;

    if (json.trim().isEmpty())
    {
        result.errors.add("Input JSON is empty.");
        return result;
    }

    if (json.length() > 65536)
    {
        result.errors.add(
            "Input JSON exceeds the 65536 character limit.");
        return result;
    }

    // JUCE 8 API:
    // JSON::parse(text, destinationVar) returns juce::Result.
    // The old JSON::parse(text, &result) form is invalid.
    juce::var root;
    const juce::Result parseResult = juce::JSON::parse(json, root);

    if (parseResult.failed())
    {
        result.errors.add(
            "Invalid JSON: " + parseResult.getErrorMessage());
        return result;
    }

    if (!root.isObject())
    {
        result.errors.add("The JSON root must be an object.");
        return result;
    }

    const auto* object = root.getDynamicObject();

    if (object == nullptr)
    {
        result.errors.add("The JSON root object is unavailable.");
        return result;
    }

    // VERSION
    const juce::var versionValue = object->getProperty("version");
    double versionNumber = 0.0;

    if (!readNumber(versionValue, versionNumber)
        || std::floor(versionNumber) != versionNumber)
    {
        result.errors.add("The 'version' field must be an integer.");
        return result;
    }

    if (versionNumber < minimumPlanVersion
        || versionNumber > maximumPlanVersion)
    {
        result.errors.add(
            "Unsupported JSON plan version: "
            + juce::String(versionNumber));
        return result;
    }

    result.plan.version = static_cast<int>(versionNumber);

    // DESCRIPTION
    const juce::var descriptionValue =
        object->getProperty("description");

    if (!descriptionValue.isVoid())
    {
        if (!descriptionValue.isString())
        {
            result.errors.add(
                "The 'description' field must be a string.");
            return result;
        }

        result.plan.description = descriptionValue.toString();
    }

    // EFFECTS ARRAY
    const juce::var effectsValue = object->getProperty("effects");

    if (!effectsValue.isArray())
    {
        result.errors.add("'effects' must be an array.");
        return result;
    }

    const auto* effectsArray = effectsValue.getArray();

    if (effectsArray == nullptr)
    {
        result.errors.add("The effects array is unavailable.");
        return result;
    }

    if (effectsArray->size() > maximumEffects)
    {
        result.errors.add(
            "The plan contains too many effects. Maximum: "
            + juce::String(maximumEffects) + ".");
        return result;
    }

    // EFFECT OBJECTS
    for (int index = 0; index < effectsArray->size(); ++index)
    {
        const juce::var effectValue = effectsArray->getReference(index);

        if (!effectValue.isObject())
        {
            result.errors.add(
                "Effect " + juce::String(index + 1)
                + " must be a JSON object.");
            return result;
        }

        const auto* effectObject = effectValue.getDynamicObject();

        if (effectObject == nullptr)
        {
            result.errors.add(
                "Effect " + juce::String(index + 1)
                + " object is unavailable.");
            return result;
        }

        AIPlanEffect effect;

        // TYPE
        const juce::var typeValue = effectObject->getProperty("type");

        if (!typeValue.isString())
        {
            result.errors.add(
                "Effect " + juce::String(index + 1)
                + " requires a string 'type' field.");
            return result;
        }

        effect.typeID = typeValue.toString().trim().toLowerCase();

        if (!isSupportedType(effect.typeID))
        {
            result.errors.add(
                "Unsupported effect type: " + effect.typeID);
            return result;
        }

        // ENABLED
        const juce::var enabledValue =
            effectObject->getProperty("enabled");

        if (!enabledValue.isVoid()
            && !readBoolean(enabledValue, effect.enabled))
        {
            result.errors.add(
                "Effect " + juce::String(index + 1)
                + " field 'enabled' must be boolean.");
            return result;
        }

        // MIX
        const juce::var mixValue = effectObject->getProperty("mix");

        if (!mixValue.isVoid())
        {
            double number = 0.0;

            if (!readNumber(mixValue, number)
                || number < 0.0
                || number > 1.0)
            {
                result.errors.add(
                    "Effect " + juce::String(index + 1)
                    + " field 'mix' must be between 0.0 and 1.0.");
                return result;
            }

            effect.mix = static_cast<float>(number);
        }

        // PARAMETERS
        const juce::var parametersValue =
            effectObject->getProperty("parameters");

        if (!parametersValue.isArray())
        {
            result.errors.add(
                "Effect " + juce::String(index + 1)
                + " requires a 'parameters' array.");
            return result;
        }

        const auto* parametersArray = parametersValue.getArray();

        if (parametersArray == nullptr)
        {
            result.errors.add("Effect parameter array is unavailable.");
            return result;
        }

        const int expectedCount = getParameterCount(effect.typeID);

        if (parametersArray->size() != expectedCount)
        {
            result.errors.add(
                "Effect " + juce::String(index + 1)
                + " (" + effect.typeID + ") expects "
                + juce::String(expectedCount) + " parameters.");
            return result;
        }

        for (int parameterIndex = 0;
             parameterIndex < parametersArray->size();
             ++parameterIndex)
        {
            double number = 0.0;

            if (!readNumber(
                    parametersArray->getReference(parameterIndex),
                    number))
            {
                result.errors.add(
                    "Effect " + juce::String(index + 1)
                    + " parameter " + juce::String(parameterIndex)
                    + " must be a finite number.");
                return result;
            }

            if (number < 0.0 || number > 1.0)
            {
                result.errors.add(
                    "Effect " + juce::String(index + 1)
                    + " parameter " + juce::String(parameterIndex)
                    + " is outside the 0.0 to 1.0 range.");
                return result;
            }

            effect.parameters.push_back(static_cast<float>(number));
        }

        result.plan.effects.push_back(std::move(effect));
    }

    // FINAL VALIDATION
    if (!validate(result.plan, result.errors, result.warnings))
        return result;

    result.rackState = toRackState(result.plan);

    if (!result.rackState.isValid())
    {
        result.errors.add("Could not create Rack state.");
        return result;
    }

    result.success = true;
    return result;
}

// ============================================================
// VALUE TREE CONVERSION
// ============================================================

juce::ValueTree AIPlanValidator::toRackState(const AIPlan& plan)
{
    AIPlan checkedPlan = plan;

    juce::StringArray errors;
    juce::StringArray warnings;

    if (!validate(checkedPlan, errors, warnings))
        return {};

    juce::ValueTree rackState("RACK");

    for (const auto& effect : checkedPlan.effects)
    {
        juce::ValueTree effectState(effect.typeID);

        effectState.setProperty("on", effect.enabled, nullptr);
        effectState.setProperty("mix", effect.mix, nullptr);

        for (size_t i = 0; i < effect.parameters.size(); ++i)
        {
            effectState.setProperty(
                parameterProperty(static_cast<int>(i)),
                effect.parameters[i],
                nullptr);
        }

        rackState.appendChild(effectState, nullptr);
    }

    return rackState;
}

// ============================================================
// SCHEMA DESCRIPTION
// ============================================================

juce::String AIPlanValidator::getSchemaDescription()
{
    return R"json(
{
  "version": 1,
  "description": "Musical explanation of the generated effect chain",
  "effects": [
    {
      "type": "filter",
      "enabled": true,
      "mix": 1.0,
      "parameters": [0.72, 0.25]
    },
    {
      "type": "distortion",
      "enabled": true,
      "mix": 0.65,
      "parameters": [0.80, 0.42]
    },
    {
      "type": "stereo_widener",
      "enabled": true,
      "mix": 0.70,
      "parameters": [0.75, 0.50]
    },
    {
      "type": "reverb",
      "enabled": true,
      "mix": 0.35,
      "parameters": [0.75, 0.80, 0.40]
    }
  ]
}
)json";
}

} // namespace nebula
