
#include "GenerationEngine.h"

namespace nebula
{

bool GenerationEngine::containsAny(
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

GenerationOutput GenerationEngine::generate(
    const juce::String& prompt)
{
    GenerationOutput output;

    const auto text = prompt.trim().toLowerCase();

    if (text.isEmpty())
    {
        output.summary = "Enter a sound description.";
        return output;
    }

    const bool dark = containsAny(text, {
        "dark", "gothic", "industrial",
        "темн", "мрач", "готич", "индустри"
    });

    const bool aggressive = containsAny(text, {
        "aggressive", "heavy", "hard", "brutal",
        "агрессив", "тяжел", "жестк", "бруталь"
    });

    const bool ambient = containsAny(text, {
        "ambient", "atmosphere", "cinematic", "space",
        "атмосфер", "эмбиент", "космос", "пространств"
    });

    const bool lofi = containsAny(text, {
        "lofi", "lo-fi", "vintage", "retro",
        "лоуфай", "лоу-фай", "винтаж", "ретро"
    });

    const bool techno = containsAny(text, {
        "techno", "ebm", "техно"
    });

    const bool wide = containsAny(text, {
        "wide", "stereo", "широк", "стерео", "объем"
    });

    const bool warm = containsAny(text, {
        "warm", "analog", "tape",
        "тепл", "аналог", "ленточ"
    });

    bool reverb = containsAny(text, {
        "reverb", "реверб"
    });

    bool delay = containsAny(text, {
        "delay", "echo", "дилей", "эхо"
    });

    bool distortion = containsAny(text, {
        "distortion", "saturation", "overdrive",
        "fuzz", "искаж", "сатурац", "перегруз"
    });

    bool chorus = containsAny(text, {
        "chorus", "хорус"
    });

    bool phaser = containsAny(text, {
        "phaser", "phase", "фейзер", "фаз"
    });

    bool compressor = containsAny(text, {
        "compressor", "compression",
        "компресс", "панч", "плотн"
    });

    bool bitcrusher = containsAny(text, {
        "bitcrusher", "crusher", "биткрашер",
        "биткраш"
    });

    bool filter = containsAny(text, {
        "filter", "lowpass", "highpass",
        "фильтр", "срез"
    });

    if (dark)
    {
        filter = true;
        distortion = true;
        reverb = true;
    }

    if (aggressive)
    {
        distortion = true;
        compressor = true;
    }

    if (ambient)
    {
        reverb = true;
        delay = true;
        chorus = true;
    }

    if (lofi)
    {
        bitcrusher = true;
        filter = true;
        distortion = true;
    }

    if (techno)
    {
        compressor = true;
        filter = true;
        delay = true;
    }

    if (wide)
    {
        chorus = true;
        phaser = true;
    }

    if (warm)
    {
        distortion = true;
        compressor = true;
    }

    auto addEffect = [&output](
        const juce::String& id,
        std::initializer_list<float> params,
        float mix)
    {
        juce::ValueTree effect(id);

        effect.setProperty("on", true, nullptr);
        effect.setProperty("mix", mix, nullptr);

        int index = 0;

        for (const float value : params)
        {
            effect.setProperty(
                "p" + juce::String(index++),
                juce::jlimit(0.0f, 1.0f, value),
                nullptr);
        }

        output.rackState.appendChild(effect, nullptr);
        output.effectNames.add(id.toUpperCase());
    };

    if (filter)
    {
        addEffect("filter", {
            dark ? 0.30f : 0.72f,
            aggressive ? 0.48f : 0.22f
        }, 1.0f);
    }

    if (compressor)
    {
        addEffect("compressor", {
            aggressive ? 0.68f : 0.52f,
            aggressive ? 0.78f : 0.45f,
            aggressive ? 0.12f : 0.35f
        }, 1.0f);
    }

    if (distortion)
    {
        addEffect("distortion", {
            aggressive ? 0.78f :
            warm ? 0.38f :
            dark ? 0.58f : 0.45f,

            dark ? 0.38f : 0.68f
        }, 0.85f);
    }

    if (bitcrusher)
    {
        addEffect("bitcrusher", {
            lofi ? 0.25f : 0.48f,
            lofi ? 0.62f : 0.35f
        }, 0.72f);
    }

    if (chorus)
    {
        addEffect("chorus", {
            wide ? 0.35f : 0.22f,
            wide ? 0.82f : 0.55f
        }, 0.75f);
    }

    if (phaser)
    {
        addEffect("phaser", {
            0.28f,
            wide ? 0.80f : 0.55f,
            0.35f
        }, 0.70f);
    }

    if (delay)
    {
        addEffect("delay", {
            ambient ? 0.62f : 0.34f,
            ambient ? 0.42f : 0.25f
        }, ambient ? 0.78f : 0.65f);
    }

    if (reverb)
    {
        addEffect("reverb", {
            ambient ? 0.85f : 0.68f,
            ambient ? 0.88f : 0.62f,
            dark ? 0.58f : 0.35f
        }, ambient ? 0.88f : 0.75f);
    }

    output.success =
        output.rackState.getNumChildren() > 0;

    if (!output.success)
    {
        output.summary =
            "No sound profile detected. Try: "
            "dark industrial techno, ambient space, "
            "lofi distortion or wide stereo.";

        return output;
    }

    output.summary =
        "Generated " +
        juce::String(output.effectNames.size()) +
        " effects: " +
        output.effectNames.joinIntoString(" -> ");

    return output;
}

}
