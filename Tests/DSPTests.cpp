
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
    bool approximatelyEqual(float a, float b, float tolerance = 0.0001f)
    {
        return std::abs(a - b) <= tolerance;
    }

    float saturate(float input, float drive)
    {
        return std::tanh(input * drive);
    }

    bool testSaturation()
    {
        const float result = saturate(0.5f, 2.0f);

        return std::isfinite(result)
            && result > 0.0f
            && result < 1.0f;
    }

    bool testSilence()
    {
        return approximatelyEqual(saturate(0.0f, 10.0f), 0.0f);
    }

    bool testPositiveAndNegativeSignal()
    {
        const float positive = saturate(0.7f, 3.0f);
        const float negative = saturate(-0.7f, 3.0f);

        return approximatelyEqual(positive, -negative);
    }

    bool testFiniteOutput()
    {
        const float values[] = {
            -100.0f, -10.0f, -1.0f,
            0.0f, 1.0f, 10.0f, 100.0f
        };

        for (float value : values)
        {
            if (!std::isfinite(saturate(value, 20.0f)))
                return false;
        }

        return true;
    }
}

int main()
{
    struct Test
    {
        const char* name;
        bool (*function)();
    };

    const Test tests[] = {
        { "Saturation", testSaturation },
        { "Silence", testSilence },
        { "Signal symmetry", testPositiveAndNegativeSignal },
        { "Finite output", testFiniteOutput }
    };

    int failures = 0;

    for (const auto& test : tests)
    {
        const bool passed = test.function();

        std::cout
            << (passed ? "[PASS] " : "[FAIL] ")
            << test.name
            << '\n';

        if (!passed)
            ++failures;
    }

    std::cout << "\nFailed tests: " << failures << '\n';

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

