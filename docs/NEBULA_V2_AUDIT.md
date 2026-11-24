# NEBULA V2 initial audit

Audit target: `feature/nebula-v2-audio-foundation` (repository state inspected on 2026-11-24).

## Project structure inspected

- `CMakeLists.txt`: C++17 project, JUCE `8.0.6` tag, VST3 and Standalone plugin formats, and a minimal test target.
- `Source/PluginProcessor.{h,cpp}`: JUCE audio processor, APVTS parameters, and a hand-written delay/saturation switch.
- `Source/PluginEditor.{h,cpp}`: existing resizable UI and attachments for all seven parameters; preserve these controls and IDs.
- `Source/Core/ParameterSmoothing.{h,cpp}` and `AudioMetering.{h,cpp}`: JUCE smoothing utility and atomic peak/RMS meter.
- `Sourse/Core/ParameterSmoothing.h`: misspelled legacy directory containing a duplicate, non-ASCII-whitespace version of the smoothing header. It is not referenced by CMake; preserve it during this foundation work.
- `Tests/DSPTests.cpp`: stand-alone test executable, but tests a local `tanh` helper rather than production DSP.
- `.github/workflows/windows-build.yml`, `scripts/`, `AUTOPILOT_*.yml`, and `AGENTS.md` were inspected. The workflow's required-file list expects DSP/Drum/Synth files that are absent from this branch, so validation fails before the real build.

## Findings and scope

1. CMake sets C++17 and declares JUCE 8.0.6, VST3, and Standalone. It does not declare a JUCE `VERSION` constraint, and the dependency is tag-pinned rather than commit-pinned. The build toolchain and plugin metadata are otherwise configured in CMake. No local build environment is available through the repository tools, so compilation must be verified by CI.
2. The plug-in's current processing allocates delay vectors in `prepareToPlay` (appropriate), but its processing logic is embedded in the processor, unsmoothed parameters are read once per block, non-finite input samples can enter DSP, and the delay/saturation implementation cannot be tested directly.
3. Existing tests do not exercise production audio code. CI's source validation names several absent engines, causing it to fail before the real build.
4. The APVTS IDs and editor attachments are functioning project surface area and will be preserved.
5. The existing effects are Clean (dry pass-through), mono-per-channel feedback Delay, and tanh Saturation. This task will extract and improve those real DSP modes; it will not claim to implement the full planned effect catalogue.

## Stages

- Stage 1: complete this baseline audit and preserve scope/constraints.
- Stage 2: extract production DSP into a reusable, allocation-free-in-process audio engine; smooth controls, sanitize unsafe values, retain existing clean/delay/saturation modes and parameter IDs, and route the plug-in through it.
- Stage 3: exercise production engine behavior in automated tests and correct CI validation so it checks files present in this project and runs the actual tests/build.

No local build/test result is asserted by this audit. CI remains the required validation path for JUCE/VST3/Standalone builds. The complete NEBULA FX/DRUM/SYNTH feature set and final release gates remain future work.
