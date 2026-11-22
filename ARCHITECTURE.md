# NEBULA architecture: current baseline and V2 direction

> **Status:** Stage V2.0 planning documentation. This file describes the repository at baseline commit `77c53a9d43e0c703842aede8258e33fe936908b0` and separates existing behavior from proposed architecture.

## Product and build baseline

NEBULA is currently a JUCE 8.0.6 C++17 audio effect plugin project using CMake. The root `CMakeLists.txt` defines a single effect-plugin target, `NebulaMultiFX`, with VST3 and Standalone formats. The implementation is organized as flat root-level C++ headers and sources. CI clones JUCE at build time; JUCE is not present in the inspected repository tree.

The latest verified Stage 0 workflow run succeeded on `feature/nebula-stage-0-build-fix` at the baseline commit. It verifies the workflow-reported Windows/macOS build outcome for that commit. It does not build this documentation commit, launch the Standalone target, or inspect downloaded artifacts.

## Current implementation

- **Processor:** stereo-default input/output, matching mono/stereo layout support, three APVTS master parameters, float audio processing and XML binary state containing APVTS state plus the rack.
- **DSP:** `EffectBase` supplies enabled, mix and ValueTree state behavior. `Rack` registers reverb, delay, distortion, chorus, phaser, compressor, bitcrusher and filter implementations.
- **Rack:** serial vector of effect unique pointers; add/remove/move/clear/save/load are implemented. There is no graph connection model or validator.
- **Presets:** factory presets plus XML user presets in the application-data directory. Saved state includes APVTS and rack state.
- **UI:** preset bar, vertical rack viewport, effect cards, master knobs, text/GENERATE control, reel graphics and spectrum display.
- **Build:** CMake declares VST3 and Standalone; workflow uploads VST3 artifacts only.

## Existing reusable components

- JUCE plugin shell and APVTS master parameters
- Eight effect implementations and shared effect interface
- Serial rack add/remove/move/save/load behavior
- Preset manager, preset bar and factory/user preset concept
- Existing premium-style look-and-feel and 2.5D reel/spectrum components
- Spectrum FFT and rack cards that can be reused after thread-safety and quality review

## Observed technical risks

1. `EffectBase::process` resizes and copies a dry buffer inside audio processing.
2. Rack/UI mutation is not separated from the audio-thread graph lifetime model.
3. Spectrum FFT writes shared display data during `processBlock` and uses an atomic readiness flag, not a complete race-safe snapshot protocol.
4. The GENERATE button is only a keyword heuristic; it is not a validated graph generator.
5. Delay, reverb decay and parameter synchronization need targeted correctness tests.
6. Preset loading lacks a complete version and validation layer.
7. Workflow does not upload or verify Standalone artifacts.

## Planned V2 architecture

Introduce the following incrementally behind tested interfaces:

- **ModuleRegistry / EffectLibrary:** stable IDs, parameter schemas, channel/resource requirements and state versions.
- **GraphEngine / RackManager:** explicit nodes/typed connections, serial graph first, later routing, validation and safe feedback nodes.
- **Prepared runtime graph:** build/prepare off the audio thread and publish with a tested lifetime strategy.
- **GenerationEngine:** deterministic parser, module selection, parameter configuration, graph validation and prepared preview.
- **PresetManager:** versioned portable `.nfx`/`.nrack` data and compatibility migration.
- **Modulation / Macro:** bounded modulators and stable host automation.
- **Visual/analysis:** JUCE 2.5D first, safe UI snapshots and configurable animations.
- **Testing:** DSP, graph, state, realtime, Windows/macOS VST3 and Standalone verification.

## Stage boundaries

- V2.0: documentation and baseline only.
- V2.1: DSP/module contracts and essential tests.
- V2.2: modular rack and safe graph replacement.
- V2.3: deterministic GENERATE engine.
- Later stages: UI, modulation, presets, AI Studio and Sample Lab.

No DSP, UI, preset format or CI behavior is changed by Stage V2.0.
