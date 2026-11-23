
#include "FXCommandEngine.h"

#include <array>
#include <cmath>

namespace nebula
{
namespace
{
    struct EffectDefinition
    {
        const char* id;
        const char* aliases;
        int parameterCount;
        float p0;
        float p1;
        float p2;
        float mix;
    };

    constexpr std::array<EffectDefinition, 8> definitions {{
        { "reverb",     "reverb|реверб|реверберация", 3, 0.50f, 0.50f, 0.50f, 0.75f },
        { "delay",      "delay|дилей|эхо",             2, 0.30f, 0.40f, 0.00f, 0.65f },
        { "distortion", "distortion|дисторшн|искажение|сатурация|overdrive|fuzz", 2, 0.50f, 0.70f, 0.00f, 0.85f },
        { "chorus",     "chorus|хорус",               2, 0.30f, 0.50f, 0.00f, 0.75f },
        { "phaser",     "phaser|фейзер|фазер",         3, 0.30f, 0.50f, 0.40f, 0.70f },
        { "compressor", "compressor|компрессор|компрессия", 3, 0.50f, 0.50f, 0.50f, 1.00f },
        { "bitcrusher", "bitcrusher|биткрашер|биткраш", 2, 0.50f, 0.50f, 0.00f, 0.70f },
        { "filter",     "filter|фильтр",               2, 0.50f, 0.30f, 0.00f, 1.00f }
    }};

    juce::String lower(const juce::String& value)
    {
        return value.trim().toLowerCase();
    }

    bool containsAny(
        const juce::String& text,
        std::initializer_list<const char*> words)
    {
        for (const auto* word : words)
        {
            if (text.contains(juce::String(word)))
                return true;
        }

        return false;
    }

    bool hasWord(
        const juce::String& text,
        const juce::String& word)
    {
        return text.contains(word);
    }

    const EffectDefinition* findDefinition(
        const juce::String& text)
    {
        for (const auto& definition : definitions)
        {
            juce::String aliases(definition.aliases);
            juce::StringArray parts;
            parts.addTokens(aliases, "|", "");

            for (const auto& alias : parts)
            {
                if (text.contains(alias))
                    return &definition;
            }
        }

        return nullptr;
    }

    int findEffectIndex(
        const juce::ValueTree& rack,
        const juce::String& typeID)
    {
        for (int i = 0; i < rack.getNumChildren(); ++i)
        {
            if (rack.getChild(i).hasType(typeID))
                return i;
        }

        return -1;
    }

    juce::ValueTree makeDefaultEffect(
        const EffectDefinition& definition)
    {
        juce::ValueTree effect(definition.id);

        effect.setProperty("on", true, nullptr);
        effect.setProperty("mix", definition.mix, nullptr);

        const std::array<float, 3> values {{
            definition.p0,
            definition.p1,
            definition.p2
        }};

        for (int i = 0; i < definition.parameterCount; ++i)
        {
            effect.setProperty(
                "p" + juce::String(i),
                values[static_cast<size_t>(i)],
                nullptr);
        }

        return effect;
    }

    bool addEffect(
        juce::ValueTree& rack,
        const EffectDefinition& definition)
    {
        if (rack.getNumChildren() >= 16)
            return false;

        rack.appendChild(
            makeDefaultEffect(definition),
            nullptr);

        return true;
    }

    bool removeEffect(
        juce::ValueTree& rack,
        const EffectDefinition& definition)
    {
        const int index = findEffectIndex(rack, definition.id);

        if (index < 0)
            return false;

        rack.removeChild(index, nullptr);
        return true;
    }

    float readFloat(
        const juce::ValueTree& tree,
        const juce::Identifier& property,
        float fallback)
    {
        const float value = (float)tree.getProperty(property, fallback);

        if (!std::isfinite(value))
            return fallback;

        return juce::jlimit(0.0f, 1.0f, value);
    }

    void setParameter(
        juce::ValueTree& effect,
        int parameter,
        float value)
    {
        if (parameter < 0 || parameter > 2)
            return;

        const juce::Identifier property(
            "p" + juce::String(parameter));

        effect.setProperty(
            property,
            juce::jlimit(0.0f, 1.0f, value),
            nullptr);
    }

    bool adjustEffect(
        juce::ValueTree& rack,
        const EffectDefinition& definition,
        float direction)
    {
        int index = findEffectIndex(rack, definition.id);

        if (index < 0)
        {
            if (!addEffect(rack, definition))
                return false;

            index = rack.getNumChildren() - 1;
        }

        auto effect = rack.getChild(index);

        if (!effect.isValid())
            return false;

        // The primary parameter depends on the effect.
        int parameter = 0;

        const juce::String id(definition.id);

        if (id == "reverb")
            parameter = 1;       // Decay

        else if (id == "delay")
            parameter = 1;       // Feedback

        else if (id == "chorus")
            parameter = 1;       // Depth

        else if (id == "phaser")
            parameter = 1;       // Depth

        else if (id == "compressor")
            parameter = 1;       // Ratio

        else if (id == "bitcrusher")
            parameter = 0;       // Bit depth

        else if (id == "filter")
            parameter = 1;       // Resonance

        const auto property = juce::Identifier(
            "p" + juce::String(parameter));

        const float oldValue = readFloat(
            effect,
            property,
            0.5f);

        const float newValue = juce::jlimit(
            0.0f,
            1.0f,
            oldValue + direction * 0.12f);

        setParameter(effect, parameter, newValue);

        const float oldMix = readFloat(
            effect,
            "mix",
            1.0f);

        effect.setProperty(
            "mix",
            juce::jlimit(
                0.0f,
                1.0f,
                oldMix + direction * 0.05f),
            nullptr);

        return true;
    }

    bool adjustAllMixes(
        juce::ValueTree& rack,
        float amount,
        bool spatialOnly)
    {
        bool changed = false;

        for (int i = 0; i < rack.getNumChildren(); ++i)
        {
            auto effect = rack.getChild(i);
            const auto id = effect.getType().toString();

            if (spatialOnly &&
                id != "reverb" &&
                id != "delay" &&
                id != "chorus" &&
                id != "phaser")
            {
                continue;
            }

            const float oldMix = readFloat(
                effect,
                "mix",
                1.0f);

            const float newMix = juce::jlimit(
                0.0f,
                1.0f,
                oldMix + amount);

            effect.setProperty("mix", newMix, nullptr);
            changed = changed || std::abs(newMix - oldMix) > 0.0001f;
        }

        return changed;
    }

    bool moveEffect(
        juce::ValueTree& rack,
        const EffectDefinition& definition,
        bool toBeginning)
    {
        const int index = findEffectIndex(rack, definition.id);

        if (index < 0)
            return false;

        const int target = toBeginning
            ? 0
            : rack.getNumChildren() - 1;

        if (index == target)
            return false;

        auto effect = rack.getChild(index);

        rack.removeChild(index, nullptr);
        rack.addChild(effect, target, nullptr);

        return true;
    }

    bool isAddCommand(const juce::String& text)
    {
        return containsAny(text, {
            "add ", "insert ", "create ", "put ",
            "добавь", "добавить", "вставь", "создай",
            "поставь", "добавить в рэк"
        });
    }

    bool isRemoveCommand(const juce::String& text)
    {
        return containsAny(text, {
            "remove ", "delete ", "drop ",
            "убери", "удали", "удалить", "убрать",
            "выкинь", "исключи"
        });
    }

    bool isClearCommand(const juce::String& text)
    {
        return containsAny(text, {
            "clear rack", "clear all", "remove all",
            "empty rack", "очисти рэк", "очистить рэк",
            "очисти цепочку", "очистить цепочку",
            "удали все эффекты", "убери все эффекты"
        });
    }

    bool isIncreaseCommand(const juce::String& text)
    {
        return containsAny(text, {
            "stronger", "increase", "more ",
            "boost", "harder", "louder",
            "сильнее", "усиль", "увеличь",
            "больше", "жестче", "жёстче",
            "мощнее", "подними"
        });
    }

    bool isDecreaseCommand(const juce::String& text)
    {
        return containsAny(text, {
            "weaker", "decrease", "less ",
            "reduce", "softer", "lower",
            "слабее", "ослабь", "уменьши",
            "меньше", "мягче", "понизь"
        });
    }

    bool isDryCommand(const juce::String& text)
    {
        return containsAny(text, {
            "make it dry", "drier", "less wet",
            "суше", "сухой звук", "сухое звучание",
            "убери пространство", "меньше пространства"
        });
    }

    bool isWetCommand(const juce::String& text)
    {
        return containsAny(text, {
            "wetter", "more wet", "more space",
            "больше пространства", "влажнее",
            "больше реверба", "больше эха"
        });
    }

    bool isMoveFirstCommand(const juce::String& text)
    {
        return containsAny(text, {
            "move to first", "move first",
            "put first", "в начало", "первым",
            "поставь первым", "перемести в начало"
        });
    }

    bool isMoveLastCommand(const juce::String& text)
    {
        return containsAny(text, {
            "move to last", "move last",
            "put last", "в конец", "последним",
            "поставь последним", "перемести в конец"
        });
    }
}

FXCommandResult FXCommandEngine::execute(
    const juce::String& command,
    const juce::ValueTree& currentRack)
{
    FXCommandResult result;

    if (!currentRack.isValid() ||
        !currentRack.hasType("RACK"))
    {
        result.summary = "Invalid rack state.";
        return result;
    }

    result.rackState = currentRack.createCopy();

    const auto text = lower(command);

    if (text.isEmpty())
    {
        result.summary = "Enter an FX command.";
        return result;
    }

    if (isClearCommand(text))
    {
        result.recognized = true;

        if (result.rackState.getNumChildren() == 0)
        {
            result.summary = "The rack is already empty.";
            return result;
        }

        result.rackState.removeAllChildren(nullptr);
        result.success = true;
        result.changed = true;
        result.summary = "All effects removed from the rack.";
        return result;
    }

    const auto* definition = findDefinition(text);

    if (definition == nullptr)
    {
        // Dry/wet commands can target the whole rack.
        if (isDryCommand(text))
        {
            result.recognized = true;
            result.changed = adjustAllMixes(
                result.rackState,
                -0.20f,
                true);

            result.success = result.changed;
            result.summary = result.changed
                ? "Spatial effects made drier. Other settings preserved."
                : "No spatial effects found to make drier.";

            return result;
        }

        if (isWetCommand(text))
        {
            result.recognized = true;
            result.changed = adjustAllMixes(
                result.rackState,
                0.15f,
                true);

            result.success = result.changed;
            result.summary = result.changed
                ? "Spatial effects made wetter. Other settings preserved."
                : "No spatial effects found to make wetter.";

            return result;
        }

        result.summary = "No FX command recognized.";
        return result;
    }

    result.recognized = true;

    const juce::String effectName(definition->id);

    if (isRemoveCommand(text))
    {
        result.changed = removeEffect(
            result.rackState,
            *definition);

        result.success = result.changed;
        result.summary = result.changed
            ? "Removed " + effectName + "."
            : effectName + " is not in the rack.";

        return result;
    }

    if (isMoveFirstCommand(text))
    {
        result.changed = moveEffect(
            result.rackState,
            *definition,
            true);

        result.success = result.changed;
        result.summary = result.changed
            ? "Moved " + effectName + " to the beginning."
            : "Effect not found or already first.";

        return result;
    }

    if (isMoveLastCommand(text))
    {
        result.changed = moveEffect(
            result.rackState,
            *definition,
            false);

        result.success = result.changed;
        result.summary = result.changed
            ? "Moved " + effectName + " to the end."
            : "Effect not found or already last.";

        return result;
    }

    if (isIncreaseCommand(text))
    {
        result.changed = adjustEffect(
            result.rackState,
            *definition,
            1.0f);

        result.success = result.changed;
        result.summary = result.changed
            ? "Increased " + effectName + ". Existing rack settings preserved."
            : "Could not modify " + effectName + ".";

        return result;
    }

    if (isDecreaseCommand(text))
    {
        result.changed = adjustEffect(
            result.rackState,
            *definition,
            -1.0f);

        result.success = result.changed;
        result.summary = result.changed
            ? "Reduced " + effectName + ". Existing rack settings preserved."
            : "Could not modify " + effectName + ".";

        return result;
    }

    if (isDryCommand(text))
    {
        const int index = findEffectIndex(
            result.rackState,
            effectName);

        result.changed = index >= 0;

        if (result.changed)
        {
            auto effect = result.rackState.getChild(index);
            const float mix = readFloat(effect, "mix", 1.0f);

            effect.setProperty(
                "mix",
                juce::jlimit(0.0f, 1.0f, mix - 0.25f),
                nullptr);
        }

        result.success = result.changed;
        result.summary = result.changed
            ? "Reduced the wet mix of " + effectName + "."
            : effectName + " is not in the rack.";

        return result;
    }

    if (isWetCommand(text))
    {
        const int index = findEffectIndex(
            result.rackState,
            effectName);

        result.changed = index >= 0;

        if (result.changed)
        {
            auto effect = result.rackState.getChild(index);
            const float mix = readFloat(effect, "mix", 1.0f);

            effect.setProperty(
                "mix",
                juce::jlimit(0.0f, 1.0f, mix + 0.20f),
                nullptr);
        }
        else
        {
            result.changed = addEffect(
                result.rackState,
                *definition);
        }

        result.success = result.changed;
        result.summary = result.changed
            ? "Increased wet mix for " + effectName + "."
            : "Could not modify " + effectName + ".";

        return result;
    }

    if (isAddCommand(text))
    {
        result.changed = addEffect(
            result.rackState,
            *definition);

        result.success = result.changed;
        result.summary = result.changed
            ? "Added " + effectName + " to the rack."
            : "Effect could not be added. The rack may be full.";

        return result;
    }

    // A bare effect name means "add this effect".
    result.changed = addEffect(
        result.rackState,
        *definition);

    result.success = result.changed;
    result.summary = result.changed
        ? "Added " + effectName + " to the rack."
        : "Effect could not be added. The rack may be full.";

    return result;
}
}
