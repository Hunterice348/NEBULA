#include "GenerationEngine.h"

#include <array>
#include <cmath>
#include <initializer_list>
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

        bool stereoWidener = false;
        bool autoPan = false;
        bool noiseGate = false;
        bool transient = false;
        bool tremolo = false;
        bool ringMod = false;
        bool combFilter = false;
        bool tapeSaturation = false;
        bool lofi = false;
        bool haasDelay = false;
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

    bool has(
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

    float clamp01(float value)
    {
        if (!std::isfinite(value))
            return 0.0f;

        return juce::jlimit(0.0f, 1.0f, value);
    }

    Genre detectGenre(const juce::String& text)
    {
        // Specific genre names must be checked before generic names.

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
            case Genre::industrialTechno: return "Industrial Techno";
            case Genre::gothicIndustrial: return "Gothic Industrial";
            case Genre::darkAmbient: return "Dark Ambient";
            case Genre::ambient: return "Ambient";
            case Genre::lofiHipHop: return "Lo-Fi Hip-Hop";
            case Genre::hipHop: return "Hip-Hop";
            case Genre::drumAndBass: return "Drum and Bass";
            case Genre::house: return "House";
            case Genre::trance: return "Trance";
            case Genre::cinematic: return "Cinematic";
            case Genre::experimental: return "Experimental";
            default: return {};
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
            "жёстк", "тяжел", "тяжёл", "ярост", "злой"
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
            "punchy", "punch", "удар",
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
                f.noiseGate = true;
                f.transient = true;
                f.tapeSaturation = true;
                break;

            case Genre::gothicIndustrial:
                f.filter = true;
                f.distortion = true;
                f.compressor = true;
                f.reverb = true;
                f.phaser = true;
                f.ringMod = true;
                f.tapeSaturation = true;
                f.stereoWidener = true;
                break;

            case Genre::darkAmbient:
                f.filter = true;
                f.reverb = true;
                f.delay = true;
                f.chorus = true;
                f.phaser = true;
                f.stereoWidener = true;
                f.haasDelay = true;
                f.combFilter = true;
                break;

            case Genre::ambient:
                f.reverb = true;
                f.delay = true;
                f.chorus = true;
                f.filter = true;
                f.stereoWidener = true;
                f.haasDelay = true;
                break;

            case Genre::lofiHipHop:
                f.bitcrusher = true;
                f.filter = true;
                f.distortion = true;
                f.compressor = true;
                f.lofi = true;
                f.tapeSaturation = true;
                f.noiseGate = true;
                break;

            case Genre::hipHop:
                f.compressor = true;
                f.filter = true;
                f.distortion = true;
                f.transient = true;
                f.noiseGate = true;
                break;

            case Genre::drumAndBass:
                f.compressor = true;
                f.filter = true;
                f.distortion = true;
                f.delay = true;
                f.transient = true;
                f.autoPan = true;
                break;

            case Genre::house:
                f.compressor = true;
                f.filter = true;
                f.chorus = true;
                f.autoPan = true;
                f.stereoWidener = true;
                f.noiseGate = true;
                break;

            case Genre::trance:
                f.filter = true;
                f.delay = true;
                f.reverb = true;
                f.chorus = true;
                f.stereoWidener = true;
                f.autoPan = true;
                break;

            case Genre::cinematic:
                f.reverb = true;
                f.delay = true;
                f.chorus = true;
                f.phaser = true;
                f.stereoWidener = true;
                f.haasDelay = true;
                f.combFilter = true;
                break;

            case Genre::experimental:
                f.filter = true;
                f.distortion = true;
                f.delay = true;
                f.phaser = true;
                f.bitcrusher = true;
                f.ringMod = true;
                f.combFilter = true;
                f.tremolo = true;
                f.lofi = true;
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
            f.combFilter = true;
        }

        if (t.aggressive)
        {
            f.distortion = true;
            f.compressor = true;
            f.filter = true;
            f.transient = true;
            f.noiseGate = true;
        }

        if (t.soft)
        {
            f.chorus = true;
            f.tremolo = true;
        }

        if (t.wide)
        {
            f.chorus = true;
            f.phaser = true;
            f.stereoWidener = true;
            f.haasDelay = true;
        }

        if (t.warm)
        {
            f.distortion = true;
            f.compressor = true;
            f.tapeSaturation = true;
        }

        if (t.dirty)
        {
            f.distortion = true;
            f.bitcrusher = true;
            f.lofi = true;
            f.tapeSaturation = true;
        }

        if (t.spacious)
        {
            f.reverb = true;
            f.delay = true;
            f.stereoWidener = true;
            f.haasDelay = true;
        }

        if (t.metallic)
        {
            f.distortion = true;
            f.phaser = true;
            f.filter = true;
            f.ringMod = true;
            f.combFilter = true;
        }

        if (t.vintage)
        {
            f.bitcrusher = true;
            f.filter = true;
            f.chorus = true;
            f.lofi = true;
            f.tapeSaturation = true;
        }

        if (t.hypnotic)
        {
            f.delay = true;
            f.phaser = true;
            f.autoPan = true;
            f.tremolo = true;
        }

        if (t.bright)
        {
            f.filter = true;
            f.chorus = true;
            f.stereoWidener = true;
        }

        if (t.punchy)
            f.transient = true;

        if (t.minimal)
            f.noiseGate = true;

        if (t.dry)
        {
            // Keep the effects available but reduce their wet mix
            // later in buildEffectPlan().
        }

        if (t.clean)
        {
            f.noiseGate = true;
            f.compressor = true;
        }
    }

    void applyExplicitEffects(
        const juce::String& text,
        EffectFlags& f)
    {
        const bool explicitHaas = has(text, {
            "haas delay", "haas_delay",
            "haas effect", "эффект хааса", "хаас задержка"
        });

        const bool explicitComb = has(text, {
            "comb filter", "comb_filter",
            "гребенчатый фильтр", "комб фильтр"
        });

        const bool explicitTape = has(text, {
            "tape saturation", "tape_saturation",
            "tape saturator", "ленточная сатурация",
            "ленточный перегруз"
        });

        const bool explicitLofi = has(text, {
            "lo-fi", "lofi", "lo fi",
            "лоу фай", "лоуфай", "лоу-фай"
        });

        if (has(text, { "reverb", "reverberation", "реверб", "реверберац" }))
            f.reverb = true;

        if (has(text, { "delay", "echo", "дилей", "эхо" })
            && !explicitHaas)
            f.delay = true;

        if (has(text, {
            "distortion", "overdrive", "fuzz",
            "искаж", "дисторшн", "перегруз"
        }))
            f.distortion = true;

        if (has(text, { "saturation", "сатурац" })
            && !explicitTape)
            f.distortion = true;

        if (has(text, { "chorus", "хорус" }))
            f.chorus = true;

        if (has(text, { "phaser", "фейзер", "фазер" }))
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
        }) && !explicitComb)
            f.filter = true;

        if (has(text, {
            "stereo widener", "stereo_widener",
            "stereo width", "расширитель стерео",
            "ширина стерео"
        }))
            f.stereoWidener = true;

        if (has(text, {
            "auto pan", "autopan", "автопан",
            "автопанорама", "авто панорама"
        }))
            f.autoPan = true;

        if (has(text, {
            "noise gate", "noise_gate", "gate",
            "шумовой гейт", "гейт"
        }))
            f.noiseGate = true;

        if (has(text, {
            "transient shaper", "transient",
            "транзиент шейпер", "транзиент"
        }))
            f.transient = true;

        if (has(text, { "tremolo", "тремоло" }))
            f.tremolo = true;

        if (has(text, {
            "ring modulator", "ring modulation",
            "ring mod", "ring_mod",
            "кольцевая модуляция", "ринг модулятор"
        }))
            f.ringMod = true;

        if (explicitComb)
            f.combFilter = true;

        if (explicitTape)
            f.tapeSaturation = true;

        if (explicitLofi)
            f.lofi = true;

        if (explicitHaas)
            f.haasDelay = true;
    }

    void applyNegativeCommands(
        const juce::String& text,
        EffectFlags& f)
    {
        const auto no = [&text](
            std::initializer_list<const char*> words)
        {
            return has(text, words);
        };

        if (no({ "no reverb", "without reverb", "без реверба", "без реверберации", "убери реверб" }))
            f.reverb = false;

        if (no({ "no delay", "without delay", "без дилея", "без эха", "убери дилей" }))
            f.delay = false;

        if (no({ "no distortion", "without distortion", "без дисторшна", "без искажений", "убери дисторшн" }))
            f.distortion = false;

        if (no({ "no chorus", "without chorus", "без хоруса", "убери хорус" }))
            f.chorus = false;

        if (no({ "no phaser", "without phaser", "без фейзера", "убери фейзер" }))
            f.phaser = false;

        if (no({ "no compressor", "without compressor", "без компрессора", "убери компрессор" }))
            f.compressor = false;

        if (no({ "no bitcrusher", "without bitcrusher", "без биткрашера", "убери биткрашер" }))
            f.bitcrusher = false;

        if (no({ "no filter", "without filter", "без фильтра", "убери фильтр" }))
            f.filter = false;

        if (no({ "no stereo widener", "without stereo widener", "без расширителя стерео", "убери расширение стерео" }))
            f.stereoWidener = false;

        if (no({ "no autopan", "no auto pan", "без автопана", "убери автопан" }))
            f.autoPan = false;

        if (no({ "no noise gate", "без гейта", "убери гейт" }))
            f.noiseGate = false;

        if (no({ "no transient", "без транзиента", "убери транзиент" }))
            f.transient = false;

        if (no({ "no tremolo", "без тремоло", "убери тремоло" }))
            f.tremolo = false;

        if (no({ "no ring mod", "без ринг модулятора", "без кольцевой модуляции" }))
            f.ringMod = false;

        if (no({ "no comb filter", "без гребенчатого фильтра", "убери comb filter" }))
            f.combFilter = false;

        if (no({ "no tape saturation", "без ленточной сатурации", "убери tape saturation" }))
            f.tapeSaturation = false;

        if (no({ "no lofi", "no lo-fi", "без лоуфая", "без lo-fi" }))
            f.lofi = false;

        if (no({ "no haas delay", "без хааса", "убери haas delay" }))
            f.haasDelay = false;
    }

    std::vector<EffectPlan> buildEffectPlan(
        const EffectFlags& f,
        const Traits& t,
        Genre genre)
    {
        std::vector<EffectPlan> plan;
        plan.reserve(16);

        constexpr size_t maximumEffects = 16;

        auto add = [&plan](
            const char* id,
            float p0,
            float p1,
            float p2,
            float mix,
            int count)
        {
            if (plan.size() >= maximumEffects)
                return;

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
            genre == Genre::ambient
            || genre == Genre::darkAmbient
            || genre == Genre::cinematic;

        const bool lofiProfile =
            genre == Genre::lofiHipHop
            || t.vintage;

        const float dryScale = t.dry ? 0.45f : 1.0f;
        const float cleanScale = t.clean ? 0.75f : 1.0f;

        // 1. Corrective and tone shaping.

        if (f.filter)
        {
            const float cutoff =
                t.dark ? 0.32f :
                t.bright ? 0.82f :
                t.warm ? 0.55f :
                t.minimal ? 0.62f : 0.70f;

            add(
                "filter",
                cutoff,
                hard ? 0.48f : 0.24f,
                0.0f,
                1.0f,
                2);
        }

        if (f.noiseGate)
        {
            add(
                "noise_gate",
                t.clean ? 0.30f : 0.38f,
                hard ? 0.62f : 0.48f,
                0.0f,
                1.0f,
                2);
        }

        // 2. Dynamics.

        if (f.transient)
        {
            add(
                "transient",
                t.punchy || hard ? 0.82f : 0.62f,
                t.soft ? 0.68f : 0.38f,
                0.0f,
                0.82f,
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

        // 3. Saturation and lo-fi processing.

        if (f.distortion)
        {
            const float drive =
                t.clean ? 0.20f :
                hard ? 0.82f :
                t.warm ? 0.36f :
                t.dirty ? 0.74f :
                t.dark ? 0.58f : 0.44f;

            const float tone =
                t.dark ? 0.38f :
                t.bright ? 0.82f :
                t.warm ? 0.48f : 0.65f;

            add(
                "distortion",
                drive,
                tone,
                0.0f,
                (hard ? 0.82f : 0.68f) * cleanScale,
                2);
        }

        if (f.tapeSaturation)
        {
            add(
                "tape_saturation",
                t.warm ? 0.38f : (hard ? 0.70f : 0.48f),
                t.dark ? 0.38f : 0.62f,
                0.0f,
                (lofiProfile ? 0.72f : 0.55f) * cleanScale,
                2);
        }

        if (f.bitcrusher)
        {
            add(
                "bitcrusher",
                lofiProfile ? 0.24f : 0.46f,
                lofiProfile ? 0.64f : 0.42f,
                0.0f,
                (t.clean ? 0.22f : 0.68f) * cleanScale,
                2);
        }

        if (f.lofi)
        {
            add(
                "lofi",
                lofiProfile ? 0.32f : 0.46f,
                lofiProfile ? 0.38f : 0.55f,
                0.0f,
                (t.clean ? 0.28f : 0.62f) * cleanScale,
                2);
        }

        // 4. Modulation and movement.

        if (f.chorus)
        {
            add(
                "chorus",
                t.wide ? 0.36f : 0.22f,
                t.wide ? 0.84f : 0.52f,
                0.0f,
                (t.soft ? 0.58f : 0.68f) * dryScale,
                2);
        }

        if (f.phaser)
        {
            add(
                "phaser",
                t.hypnotic ? 0.42f : 0.28f,
                t.wide ? 0.82f : 0.58f,
                0.36f,
                0.62f * dryScale,
                3);
        }

        if (f.autoPan)
        {
            add(
                "autopan",
                t.hypnotic ? 0.34f : 0.46f,
                t.wide ? 0.88f : 0.62f,
                0.0f,
                0.62f * dryScale,
                2);
        }

        if (f.tremolo)
        {
            add(
                "tremolo",
                t.hypnotic ? 0.58f : 0.38f,
                t.soft ? 0.38f : 0.62f,
                0.0f,
                0.55f * dryScale,
                2);
        }

        if (f.ringMod)
        {
            add(
                "ring_mod",
                t.metallic ? 0.72f : 0.42f,
                t.dark ? 0.68f : 0.48f,
                0.0f,
                hard ? 0.72f : 0.48f,
                2);
        }

        // 5. Resonance and stereo space.

        if (f.combFilter)
        {
            add(
                "comb_filter",
                t.metallic ? 0.72f : 0.38f,
                t.hypnotic ? 0.62f : 0.42f,
                0.0f,
                0.48f * dryScale,
                2);
        }

        if (f.stereoWidener)
        {
            add(
                "stereo_widener",
                t.wide ? 0.88f : 0.68f,
                t.clean ? 0.58f : 0.48f,
                0.0f,
                0.72f * dryScale,
                2);
        }

        if (f.haasDelay)
        {
            add(
                "haas_delay",
                ambient ? 0.52f : 0.34f,
                t.wide ? 0.86f : 0.62f,
                0.0f,
                0.48f * dryScale,
                2);
        }

        // 6. Time-based effects at the end of the chain.

        if (f.delay)
        {
            const float time =
                ambient ? 0.66f :
                t.hypnotic ? 0.52f : 0.34f;

            const float feedback =
                ambient ? 0.44f :
                hard ? 0.24f : 0.32f;

            const float mix =
                t.dry ? 0.0f :
                ambient ? 0.78f : 0.58f;

            add("delay", time, feedback, 0.0f, mix, 2);
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

            add("reverb", size, decay, damp, mix, 3);
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

    // 1. Musical genre profile.
    applyGenreProfile(genre, flags);

    // 2. Sound characteristics.
    applyTraits(traits, flags);

    // 3. Explicit effect requests.
    applyExplicitEffects(text, flags);

    // 4. Negative commands always have priority.
    applyNegativeCommands(text, flags);

    const auto plan = buildEffectPlan(
        flags,
        traits,
        genre);

    output.detectedGenre = genreName(genre);
    output.detectedIntent = describeTraits(traits);

    if (plan.empty())
    {
        if (genre == Genre::unknown)
        {
            output.summary =
                "No supported genre or effect was detected. "
                "Try Industrial Techno, Dark Ambient, Lo-Fi Hip-Hop, "
                "House, Trance, Experimental, or name an effect.";

            return output;
        }

        output.summary =
            "The selected profile contains no active effects. "
            "Check whether all effects were disabled.";

        return output;
    }

    for (const auto& item : plan)
    {
        juce::ValueTree effect(item.id);

        effect.setProperty("on", true, nullptr);
        effect.setProperty("mix", item.mix, nullptr);

        const std::array<float, 3> parameters {{
            item.p0,
            item.p1,
            item.p2
        }};

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

    output.success =
        output.rackState.getNumChildren() > 0;

    juce::StringArray explanation;

    if (output.detectedGenre.isNotEmpty())
        explanation.add("Genre: " + output.detectedGenre);

    if (output.detectedIntent.isNotEmpty())
        explanation.add("Character: " + output.detectedIntent);

    explanation.add(
        "FX chain: " +
        output.effectNames.joinIntoString(" -> "));

    output.summary =
        "NEBULA local AI generation completed. " +
        explanation.joinIntoString(" | ");

    return output;
}

}