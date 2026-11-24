# NEBULA

NEBULA FX is a C++17/JUCE 8.0.6 audio plug-in project built with CMake. The current branch contains an initial NEBULA FX audio foundation, packaged by CMake as VST3 and Standalone targets.

## Current functionality

- Stable APVTS parameter IDs for input gain, output gain, wet/dry, drive, delay time, feedback, and Clean/Delay/Saturation mode.
- Reusable audio engine with mono and supported multichannel processing, per-sample parameter ramps, finite-value sanitation, and variable-sized callback support.
- Real per-channel feedback delay and tanh saturation DSP. Clean mode remains a pass-through with input/output gain staging.
- Host bypass is a unity pass-through. Parameter state continues to use JUCE APVTS XML serialization.
- Tests execute production engine/effect classes directly (see `Tests/DSPTests.cpp`).

## Build

CMake fetches the JUCE 8.0.6 source tag. Configure and build using a supported C++17 toolchain:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The repository's Windows workflow is intended to build VST3 and Standalone. These targets are not claimed as locally verified unless a build run is reported separately.

## Scope

This is a foundation, not a finished production effects suite. Reverb, compressor, modulation effects, preset management, serial/parallel routing, drum and synth engines, and a JUCE processor state round-trip test are not implemented yet. Review `docs/ARCHITECTURE.md` and `docs/NEBULA_V2_AUDIT.md` for design and audit status.
