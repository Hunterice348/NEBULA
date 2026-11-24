# NEBULA V2 initial audit

Audit target: `feature/nebula-v2-audio-foundation` (repository state inspected on 2026-11-24).

## Project structure inspected

- `Source/PluginProcessor.{h,cpp}`: JUCE audio processor, APVTS parameters, and a hand-written delay/saturation switch.
- `Source/PluginEditor.{h,cpp}`: existing resizable UI and attachments for all seven parameters; preserve these controls and IDs.
- `Source/Core/ParameterSmoothing.{h,cpp}` and `AudioMetering.{h,cpp}`: JUCE smoothing utility and atomic peak/RMS meter.
- `Sourse/Core/ParameterSmoothing.h`: misspelled legacy directory containing a duplicate, non-ASCII-whitespace version of the smoothing header. It is not referenced by CMake; do not silently delete existing project files.
- `Tests/DSPTests.cpp`: stand-alone test executable, but tests a local `tanh` helper rather than production DSP.
- `.github/workflows/windows-build.yml`, `scripts/`, `AUTOPILOT_*.yml`, and `AGENTS.md` were inspected. The workflow's required-file list expects DSP/Drum/Synth files that are absent from this branch.

## Findings and scope

1. CMake sets C++17 and declares JUCE 8.0.6, VST3, and Standalone. It does not declare a JUCE `VERSION` constraint, and the dependency is tag-pinned rather than commit-pinned. The build toolchain and network availability are not available through this GitHub editing interface, so a local configure/build cannot be claimed.
2. The plug-in's current processing allocates delay vectors in `prepareToPlay` (appropriate), but its processing logic is embedded in the processor, unsmoothed parameters are read once per block, non-finite samples can enter the delay feedback state, and the fallback path does not sanitize samples.
3. Existing tests do not exercise production audio code. CI's source validation names several absent engines, causing it to fail before the real build.
4. The APVTS IDs and editor attachments are functioning project surface area and will be preserved.

## Planned stages

- Stage 1 (this commit): record the repository audit and baseline constraints.
- Stage 2: extract production DSP into a reusable, allocation-free audio engine; smooth controls, sanitize unsafe values, retain existing clean/delay/saturation modes and parameter IDs, and test the actual engine.
- Stage 3: wire real sources/tests into CMake and correct CI validation so it checks files present in this project and runs the actual tests/build.

No local build/test result is asserted by this audit. CI remains the required validation path for JUCE/VST3/Standalone builds.
