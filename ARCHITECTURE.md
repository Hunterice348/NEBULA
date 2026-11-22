# NEBULA architecture: current baseline and V2 direction

> **Status:** Stage V2.0 planning documentation. This file describes the repository as inspected at baseline commit `77c53a9d43e0c703842aede8258e33fe936908b0` and separates existing behavior from proposed architecture. Proposed components below are not implemented unless explicitly marked current.

## Product and build baseline

NEBULA is currently a JUCE 8.0.6 C++17 audio effect plugin project using CMake. The root `CMakeLists.txt` defines a single effect plugin target, `NebulaMultiFX`, with VST3 and Standalone formats. The implementation is organized as flat root-level C++ headers and sources. CI clones JUCE at build time; JUCE itself is not listed in the inspected repository tree.

The latest verified Stage 0 workflow run completed successfully on `feature/nebula-stage-0-build-fix` at the baseline commit. This verifies the build on that workflow's Windows/macOS matrix as reported by Actions; the workflow uploads VST3 artifacts only. It is not a build of this documentation-only branch commit and does not verify standalone artifacts by download or launch. See [Stage 0 workflow run](https://github.com/Hunterice348/NEBULA/actions/runs/45029388928).

## Current implementation (observed)

- **Plugin entry and host state:** `PluginProcessor` provides stereo-default input/output buses, accepts matching mono or stereo layouts, registers three APVTS parameters (input gain, global mix, output gain), processes float audio, and serializes APVTS state plus a rack tree as XML binary state. MIDI is not accepted or produced.
- **DSP:** `EffectBase` supplies per-effect enabled state, a normalized wet/dry control and ValueTree serialization. Eight effect implementations are registered by `Rack`: reverb, delay, distortion, chorus, phaser, compressor, bitcrusher and filter. Processing uses a serial loop over the rack.
- **Rack:** Effects are held as `std::unique_ptr` objects in a vector. The rack supports insert, remove, reorder, clear, lookup and ValueTree save/load. There is no graph topology, connection model, groups or resource validator.
- **Presets:** `PresetManager` has a small in-code factory list and scans/writes user XML presets in the user application data directory. Saved user state includes APVTS and the rack tree. The factory list includes `Init`, `Deep Space Pad` and `Dub Echo`; the latter names/parameter entries should not be mistaken for an exhaustive, validated preset library.
- **UI:** A JUCE editor contains preset navigation, effect cards in a vertical `Viewport`, a text box and a `GENERATE` button, three master controls, reel graphics and a spectrum display. Look-and-feel code draws rotary sliders. The editor is fixed-size in the inspected source.
- **Build:** CMake declares VST3 and Standalone. GitHub Actions has Windows and macOS jobs, but artifact upload steps currently target VST3 only.

## Observed gaps and risks (not claims of completed behavior)

1. `EffectBase::process` calls `AudioBuffer::setSize` for its dry buffer while processing audio. Rack editing/loading directly changes the same rack the processor processes; there is no prepared immutable graph swap or audio-thread-safe lifetime handoff.
2. Rack structure and effect values are ordinary mutable objects. UI callbacks edit effects directly, so concurrent audio/UI access is not made safe by the current design. `onChanged` is a callback hook, not a synchronization mechanism.
3. Spectrum FFT work is performed in `processBlock`; the UI reads a shared non-atomic array after an atomic flag. The flag does not make concurrent array reads/writes race-free.
4. The current text `GENERATE` handler is a small keyword-to-append heuristic. It does not parse a structured request, build/validate a generated graph, stage a preview, estimate CPU, offer variations, or save a generated rack. It must not be described as a completed generative effect engine.
5. The reverb `decay` value is stored and serialized but is not applied in `updateParams`. Delay's wet processing returns unchanged for mono input. Other effect quality, channel-layout, parameter-threading and reset details need targeted DSP review and tests before enhancement.
6. Preset parsing/loading has limited validation and versioning. Loading mutates the live rack; saved user preset files use XML rather than the planned portable versioned `.nfx`/`.nrack` formats.
7. The workflow currently uploads VST3 only, and the successful baseline run does not by itself prove Standalone artifact upload/launch or plugin-host validation.

## V2 architecture proposal (planned, not implemented)

Preserve the existing JUCE plugin shell and introduce components incrementally behind tested interfaces:

- **AudioEngine / Processor:** own bus negotiation, fixed host automation parameters, input/output gain, dry/wet, latency reporting and audio-safe graph handoff. APVTS remains the stable host-facing parameter registry; rack changes must not create host parameters dynamically.
- **EffectBase / ModuleRegistry / EffectLibrary:** establish documented module descriptors (stable type ID, parameter schema/ranges, channel and resource requirements, prepare/reset/process, state version and estimated cost). Retain and improve the existing eight effects rather than removing them.
- **Graph model / RackManager:** represent nodes by stable IDs and typed connections, validate ordinary audio flow as a DAG, explicitly model any bounded delayed feedback, and enforce configurable graph/module/resource limits. Keep editing and graph construction outside `processBlock`.
- **Prepared runtime graph:** build and prepare an immutable candidate away from the audio thread, then publish it at a processing boundary using a rigorously owned lock-free handoff/retirement strategy. No graph destruction, allocation, blocking locks or UI/filesystem work in the callback. Select and test a lifetime strategy before implementation; atomic pointer exchange alone is not sufficient if it permits premature destruction.
- **GenerationEngine:** deterministic offline description interpretation -> module/parameter configuration -> graph validation and budget estimate -> prepared preview candidate -> user-approved graph switch. It must use registered modules and validated data, never generated code or network requirements for ordinary processing.
- **PresetManager:** versioned validated data for complete graph, module state, modulation, routing, macro mapping and optional UI metadata, with migration and import/export tests.
- **Modulation / Macro systems:** predeclared automation-safe host parameters, bounded modulator/destination capacities, smoothing and safe control/audio data exchange.
- **Presentation:** rack, patcher, library, preset workflow and analysis UI in JUCE. Begin with a performant 2.5D drawing approach; graphics must consume safe snapshots and never block DSP. Evaluate JUCE/OpenGL compatibility only if a concrete visual requirement warrants it.
- **Quality infrastructure:** unit and DSP tests, sanitizers where practical, Windows/macOS CI for VST3 and Standalone, verified artifact upload, and explicit release checklist.

## Incremental boundaries

1. V2.0 records and verifies the baseline; no DSP/UI change is part of this stage.
2. V2.1 hardens current DSP and shared module contracts with tests.
3. V2.2 advances rack/runtime ownership and graph validation only after a safe swap design is tested.
4. V2.3 builds deterministic generation on the validated module/graph interfaces.
5. Later stages add presentation, modulation/presets and local sample tools in the sequence in `ROADMAP.md`.

## Explicit non-goals for this stage

No DSP behavior, UI behavior, graph architecture, preset data format, CI workflow or product feature is changed by this documentation stage. OpenGL, cloud AI, spectral/convolution processing and arbitrary runtime code generation are not adopted by this document.
