
#include "GenerationEngine.h"

#include <array>
#include <cmath>
#include <vector>

namespace nebula
{
namespace
{
    enum class Genre
    {
        unknown,
        industrialTechno,
        gothicIndustrial,
        darkAmbient,
        ambient,
        lofiHipHop,
        hipHop,
        drumAndBass,
        house,
        trance,
        cinematic,
        experimental
    };

    struct Traits
    {
        bool dark = false;
        bool aggressive = false;
        bool soft = false;
        bool wide = false;
        bool warm = false;
        bool dirty = false;
        bool clean = false;
        bool spacious = false;
        bool dry = false;
        bool punchy = false;
        bool vintage = false;
        bool metallic = false;
        bool hypnotic = false;
        bool bright = false;
        bool minimal = false;
    };

    struct EffectPlan
    {
        const char* id;
        float p0;
        float p1;
        float p2;
        float mix;
        int parameterCount;
    };

    struct EffectFlags
    {
        bool reverb = false;
        bool delay = false;
        bool distortion = false;
        bool chorus = false;
        bool phaser = false;
        bool compressor = false;
        bool bitcrusher = false;
        bool filter = false;
    };

    bool has(const juce::String& text,
             std::initializer_list<const char*> words)
    {
        for (const auto* word : words)
        {
            if (text.contains(juce::String(word)))
                return true;
        }

        return false;
    }

    float clamp01(float value)
    {
        if (!std::isfinite(value))
            return 0.0f;

        return juce::jlimit(0.0f, 1.0f, value);
    }

    Genre detectGenre(const juce::String& text)
    {
        // More specific genres must be checked first.

        if (has(text, {
            "gothic industrial", "gothic techno",
            "готик индастриал", "готический индастриал",
            "готик техно", "готическое техно"
        }))
            return Genre::gothicIndustrial;

        if (has(text, {
            "industrial techno", "industrial rave",
            "индастриал техно", "индустриал техно",
            "индустриальное техно", "индустриальный рейв"
        }))
            return Genre::industrialTechno;

        if (has(text, {
            "dark ambient", "dark drone",
            "тёмный эмбиент", "темный эмбиент",
            "мрачный эмбиент", "дарк эмбиент"
        }))
            return Genre::darkAmbient;

        if (has(text, {
            "drum and bass", "drum & bass",
            "drum n bass", "dnb", "джангл",
            "драм энд бейс", "драм-н-бейс"
        }))
            return Genre::drumAndBass;

        if (has(text, {
            "lofi hip hop", "lo-fi hip hop",
            "lofi hip-hop", "лоуфай хип хоп",
            "лоу-фай хип-хоп"
        }))
            return Genre::lofiHipHop;

        if (has(text, {
            "experimental", "avant-garde",
            "эксперименталь", "авангард"
        }))
            return Genre::experimental;

        if (has(text, {
            "cinematic", "film score",
            "саундтрек", "кинематограф"
        }))
            return Genre::cinematic;

        if (has(text, {
            "ambient", "atmospheric",
            "эмбиент", "атмосферная музыка",
            "атмосферный"
        }))
            return Genre::ambient;

        if (has(text, {
            "hip hop", "hip-hop", "hiphop",
            "хип хоп", "хип-хоп"
        }))
            return Genre::hipHop;

        if (has(text, {
            "trance", "транс"
        }))
            return Genre::trance;

        if (has(text, {
            "house", "хаус"
        }))
            return Genre::house;

        if (has(text, {
            "techno", "ebm", "техно",
            "электроиндастриал"
        }))
            return Genre::industrialTechno;

        return Genre::unknown;
    }

    juce::String genreName(Genre genre)
    {
        switch (genre)
        {
            case Genre::industrialTechno:
                return "Industrial Techno";

            case Genre::gothicIndustrial:
                return "Gothic Industrial";

            case Genre::darkAmbient:
                return "Dark Ambient";

            case Genre::ambient:
                return "Ambient";

            case Genre::lofiHipHop:
                return "Lo-Fi Hip-Hop";

            case Genre::hipHop:
                return "Hip-Hop";

            case Genre::drumAndBass:
                return "Drum and Bass";

            case Genre::house:
                return "House";

            case Genre::trance:
                return "Trance";

            case Genre::cinematic:
                return "Cinematic";

            case Genre::experimental:
                return "Experimental";

            default:
                return {};
        }
    }

    Traits detectTraits(const juce::String& text)
    {
        Traits t;

        t.dark = has(text, {
            "dark", "gothic", "noir", "evil",
            "темн", "мрач", "готич", "зловещ"
        });

        t.aggressive = has(text, {
            "aggressive", "brutal", "hard",
            "heavy", "violent", "harsh",
            "агрессив", "бруталь", "жестк",
            "тяжел", "ярост", "злой"
        });

        t.soft = has(text, {
            "soft", "gentle", "smooth",
            "мягк", "нежн", "плавн"
        });

        t.wide = has(text, {
            "wide", "stereo", "широк",
            "стерео", "объемн", "объёмн"
        });

        t.warm = has(text, {
            "warm", "analog", "tape",
            "тепл", "аналог", "ленточ"
        });

        t.dirty = has(text, {
            "dirty", "gritty", "saturated",
            "грязн", "насыщенн", "перегруж"
        });

        t.clean = has(text, {
            "clean", "transparent", "pristine",
            "чист", "прозрачн", "аккуратн"
        });

        t.spacious = has(text, {
            "spacious", "huge space", "deep space",
            "пространств", "огромное пространство",
            "глубокое пространство"
        });

        t.dry = has(text, {
            "dry", "no reverb", "without reverb",
            "без реверба", "без реверберации",
            "сухой звук", "сухое звучание"
        });

        t.punchy = has(text, {
            "punchy", "punch", " удар",
            "панч", "ударн", "пробивн"
        });

        t.vintage = has(text, {
            "vintage", "retro", "old school",
            "винтаж", "ретро", "олдскул"
        });

        t.metallic = has(text, {
            "metallic", "industrial metal",
            "metal", "металлическ", "металл",
            "железн", "механическ"
        });

        t.hypnotic = has(text, {
            "hypnotic", "repetitive",
            "гипнотическ", "повторяющ"
        });

        t.bright = has(text, {
            "bright", "brighter", "airy",
            "ярк", "светл", "воздушн"
        });

        t.minimal = has(text, {
            "minimal", "minimalistic",
            "минимал", "минималист"
        });

        return t;
    }

    void applyGenreProfile(
        Genre genre,
        EffectFlags& f)
    {
        switch (genre)
        {
            case Genre::industrialTechno:
                f.filter = true;
                f.compressor = true;
                f.distortion = true;
                f.delay = true;
                break;

            case Genre::gothicIndustrial:
                f.filter = true;
                f.distortion = true;
                f.compressor = true;
                f.reverb = true;
                f.phaser = true;
                break;

            case Genre::darkAmbient:
                f.filter = true;
                f.reverb = true;
                f.delay = true;
                f.chorus = true;
                f.phaser = true;
                break;

            case Genre::ambient:
                f.reverb = true;
                f.delay = true;
                f.chorus = true;
                f.filter = true;
                break;

            case Genre::lofiHipHop:
                f.bitcrusher = true;
                f.filter = true;
                f.distortion = true;
                f.compressor = true;
                break;

            case Genre::hipHop:
                f.compressor = true;
                f.filter = true;
                f.distortion = true;
                break;

            case Genre::drumAndBass:
                f.compressor = true;
                f.filter = true;
                f.distortion = true;
                f.delay = true;
                break;

            case Genre::house:
                f.compressor = true;
                f.filter = true;
                f.chorus = true;
                break;

            case Genre::trance:
                f.filter = true;
                f.delay = true;
                f.reverb = true;
                f.chorus = true;
                break;

            case Genre::cinematic:
                f.reverb = true;
                f.delay = true;
                f.chorus = true;
                f.phaser = true;
                break;

            case Genre::experimental:
                f.filter = true;
                f.distortion = true;
                f.delay = true;
                f.phaser = true;
                f.bitcrusher = true;
                break;

            default:
                break;
        }
    }

    void applyTraits(
        const Traits& t,
        EffectFlags& f)
    {
        if (t.dark)
        {
            f.filter = true;
            f.distortion = true;
            f.reverb = true;
        }

        if (t.aggressive)
        {
            f.distortion = true;
            f.compressor = true;
            f.filter = true;
        }

        if (t.soft)
        {
            f.chorus = true;
        }

        if (t.wide)
        {
            f.chorus = true;
            f.phaser = true;
        }

        if (t.warm)
        {
            f.distortion = true;
            f.compressor = true;
        }

        if (t.dirty)
        {
            f.distortion = true;
            f.bitcrusher = true;
        }

        if (t.spacious)
        {
            f.reverb = true;
            f.delay = true;
        }

        if (t.metallic)
        {
            f.distortion = true;
            f.phaser = true;
            f.filter = true;
        }

        if (t.vintage)
        {
            f.bitcrusher = true;
            f.filter = true;
            f.chorus = true;
        }

        if (t.hypnotic)
        {
            f.delay = true;
            f.phaser = true;
        }

        if (t.bright)
        {
            f.filter = true;
            f.chorus = true;
        }
    }

    void applyExplicitEffects(
        const juce::String& text,
        EffectFlags& f)
    {
        if (has(text, {
            "reverb", "reverberation",
            "реверб", "реверберац"
        }))
            f.reverb = true;

        if (has(text, {
            "delay", "echo", "дилей", "эхо"
        }))
            f.delay = true;

        if (has(text, {
            "distortion", "overdrive", "fuzz",
            "saturation", "искаж", "сатурац",
            "перегруз"
        }))
            f.distortion = true;

        if (has(text, {
            "chorus", "хорус"
        }))
            f.chorus = true;

        if (has(text, {
            "phaser", "фейзер", "фазер"
        }))
            f.phaser = true;

        if (has(text, {
            "compressor", "compression",
            "компрессор", "компрессия"
        }))
            f.compressor = true;

        if (has(text, {
            "bitcrusher", "bit crusher",
            "биткрашер", "биткраш"
        }))
            f.bitcrusher = true;

        if (has(text, {
            "filter", "lowpass", "highpass",
            "фильтр", "срез частот"
        }))
            f.filter = true;
    }

    void applyNegativeCommands(
        const juce::String& text,
        EffectFlags& f)
    {
        const bool noReverb = has(text, {
            "no reverb", "without reverb",
            "remove reverb", "без реверба",
            "без реверберации", "убери реверб",
            "убрать реверб"
        });

        const bool noDelay = has(text, {
            "no delay", "without delay",
            "без дилея", "без эха",
            "убери дилей", "убрать дилей"
        });

        const bool noDistortion = has(text, {
            "no distortion", "without distortion",
            "без дисторшна", "без искажений",
            "убери дисторшн"
        });

        const bool noChorus = has(text, {
            "no chorus", "without chorus",
            "без хоруса", "убери хорус"
        });

        const bool noPhaser = has(text, {
            "no phaser", "without phaser",
            "без фейзера", "убери фейзер"
        });

        const bool noCompressor = has(text, {
            "no compressor", "without compressor",
            "без компрессора", "убери компрессор"
        });

        const bool noBitcrusher = has(text, {
            "no bitcrusher", "without bitcrusher",
            "без биткрашера", "убери биткрашер"
        });

        const bool noFilter = has(text, {
            "no filter", "without filter",
            "без фильтра", "убери фильтр"
        });

        if (noReverb) f.reverb = false;
        if (noDelay) f.delay = false;
        if (noDistortion) f.distortion = false;
        if (noChorus) f.chorus = false;
        if (noPhaser) f.phaser = false;
        if (noCompressor) f.compressor = false;
        if (noBitcrusher) f.bitcrusher = false;
        if (noFilter) f.filter = false;
    }

    std::vector<EffectPlan> buildEffectPlan(
        const EffectFlags& f,
        const Traits& t,
        Genre genre)
    {
        std::vector<EffectPlan> plan;

        auto add = [&plan](
            const char* id,
            float p0,
            float p1,
            float p2,
            float mix,
            int count)
        {
            plan.push_back({
                id,
                clamp01(p0),
                clamp01(p1),
                clamp01(p2),
                clamp01(mix),
                count
            });
        };

        const bool hard = t.aggressive || t.metallic;
        const bool ambient =
            genre == Genre::ambient ||
            genre == Genre::darkAmbient ||
            genre == Genre::cinematic;

        const bool lofi =
            genre == Genre::lofiHipHop ||
            t.vintage;

        // Order matters: corrective shaping first,
        // dynamics and saturation in the middle,
        // spatial effects at the end.

        if (f.filter)
        {
            float cutoff = 0.70f;

            if (t.dark)
                cutoff = 0.32f;

            if (t.bright)
                cutoff = 0.82f;

            if (t.warm)
                cutoff = 0.55f;

            if (t.minimal)
                cutoff = 0.62f;

            add(
                "filter",
                cutoff,
                hard ? 0.48f : 0.24f,
                0.0f,
                1.0f,
                2);
        }

        if (f.compressor)
        {
            add(
                "compressor",
                hard ? 0.72f : 0.52f,
                hard ? 0.82f : 0.48f,
                t.punchy ? 0.18f : 0.34f,
                1.0f,
                3);
        }

        if (f.distortion)
        {
            float drive = 0.44f;

            if (hard) drive = 0.82f;
            if (t.warm) drive = 0.36f;
            if (t.dirty) drive = 0.74f;
            if (t.clean) drive = 0.20f;
            if (t.dark) drive = juce::jmax(drive, 0.58f);

            const float tone =
                t.dark ? 0.38f :
                t.bright ? 0.82f :
                t.warm ? 0.48f : 0.65f;

            const float mix =
                t.clean ? 0.28f :
                hard ? 0.88f :
                t.soft ? 0.42f : 0.72f;

            add(
                "distortion",
                drive,
                tone,
                0.0f,
                mix,
                2);
        }

        if (f.bitcrusher)
        {
            add(
                "bitcrusher",
                lofi ? 0.24f : 0.46f,
                lofi ? 0.64f : 0.42f,
                0.0f,
                t.clean ? 0.22f : 0.68f,
                2);
        }

        if (f.chorus)
        {
            add(
                "chorus",
                t.wide ? 0.36f : 0.22f,
                t.wide ? 0.84f : 0.52f,
                0.0f,
                t.soft ? 0.58f : 0.72f,
                2);
        }

        if (f.phaser)
        {
            add(
                "phaser",
                t.hypnotic ? 0.42f : 0.28f,
                t.wide ? 0.82f : 0.58f,
                0.36f,
                0.68f,
                3);
        }

        if (f.delay)
        {
            const float time =
                ambient ? 0.66f :
                t.hypnotic ? 0.52f :
                0.34f;

            const float feedback =
                ambient ? 0.44f :
                hard ? 0.24f : 0.32f;

            const float mix =
                t.dry ? 0.0f :
                ambient ? 0.78f : 0.58f;

            add(
                "delay",
                time,
                feedback,
                0.0f,
                mix,
                2);
        }

        if (f.reverb)
        {
            const float size =
                ambient ? 0.88f :
                t.dark ? 0.72f : 0.56f;

            const float decay =
                ambient ? 0.90f :
                t.dark ? 0.72f : 0.48f;

            const float damp =
                t.dark ? 0.62f :
                t.bright ? 0.28f : 0.42f;

            const float mix =
                t.dry ? 0.0f :
                ambient ? 0.82f :
                t.dark ? 0.62f : 0.42f;

            add(
                "reverb",
                size,
                decay,
                damp,
                mix,
                3);
        }

        return plan;
    }

    juce::String describeTraits(const Traits& t)
    {
        juce::StringArray names;

        if (t.dark) names.add("Dark");
        if (t.aggressive) names.add("Aggressive");
        if (t.soft) names.add("Soft");
        if (t.wide) names.add("Wide stereo");
        if (t.warm) names.add("Warm");
        if (t.dirty) names.add("Dirty");
        if (t.clean) names.add("Clean");
        if (t.spacious) names.add("Spacious");
        if (t.dry) names.add("Dry");
        if (t.punchy) names.add("Punchy");
        if (t.vintage) names.add("Vintage");
        if (t.metallic) names.add("Metallic");
        if (t.hypnotic) names.add("Hypnotic");
        if (t.bright) names.add("Bright");
        if (t.minimal) names.add("Minimal");

        return names.joinIntoString(", ");
    }
}

bool GenerationEngine::containsAny(
    const juce::String& text,
    std::initializer_list<const char*> words)
{
    return has(text, words);
}

GenerationOutput GenerationEngine::generate(
    const juce::String& prompt)
{
    GenerationOutput output;

    const auto text = prompt.trim().toLowerCase();

    if (text.isEmpty())
    {
        output.summary = "Enter a sound or genre description.";
        return output;
    }

    const Genre genre = detectGenre(text);
    const Traits traits = detectTraits(text);

    EffectFlags flags;

    // 1. Build a musical profile from the genre.
    applyGenreProfile(genre, flags);

    // 2. Add sound characteristics.
    applyTraits(traits, flags);

    // 3. Respect explicitly requested effects.
    applyExplicitEffects(text, flags);

    // 4. Negative instructions have priority.
    applyNegativeCommands(text, flags);

    const auto plan = buildEffectPlan(
        flags,
        traits,
        genre);

    if (plan.empty())
    {
        output.detectedGenre = genreName(genre);
        output.detectedIntent = describeTraits(traits);

        if (genre == Genre::unknown)
        {
            output.summary =
                "I could not identify a supported genre or effect. "
                "Try a genre such as Industrial Techno, Dark Ambient, "
                "Lo-Fi Hip-Hop, House, Trance or Experimental, "
                "or name an effect such as Distortion or Reverb.";

            return output;
        }

        output.summary =
            "The requested profile produced no active effects. "
            "Check whether all effects were explicitly disabled.";

        return output;
    }

    for (const auto& item : plan)
    {
        juce::ValueTree effect(item.id);

        effect.setProperty("on", true, nullptr);
        effect.setProperty("mix", item.mix, nullptr);

        const std::array<float, 3> parameters {
            item.p0,
            item.p1,
            item.p2
        };

        for (int i = 0; i < item.parameterCount; ++i)
        {
            effect.setProperty(
                "p" + juce::String(i),
                parameters[static_cast<size_t>(i)],
                nullptr);
        }

        output.rackState.appendChild(effect, nullptr);

        output.effectNames.add(
            juce::String(item.id).toUpperCase());
    }

    output.detectedGenre = genreName(genre);
    output.detectedIntent = describeTraits(traits);

    output.success = output.rackState.getNumChildren() > 0;

    juce::StringArray explanation;

    if (output.detectedGenre.isNotEmpty())
        explanation.add("Genre: " + output.detectedGenre);

    if (output.detectedIntent.isNotEmpty())
        explanation.add("Character: " + output.detectedIntent);

    explanation.add(
        "FX chain: " +
        output.effectNames.joinIntoString(" -> "));

    output.summary =
        "NEBULA local generation completed. " +
        explanation.joinIntoString(" | ");

    return output;
}

}
