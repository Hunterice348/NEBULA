# NEBULA V2 roadmap

> **Status:** Planning only. Roadmap items are not implemented or tested features.

## Stage V2.0 — Architecture and baseline (current)

- Audit repository structure and baseline implementation.
- Verify Stage 0 Actions evidence.
- Separate current behavior from future features.
- Add architecture, module, generation and build documentation.
- **Exit gate:** documentation review and approval before V2.1.

## Stage V2.1 — DSP foundations

- Define module and parameter contracts.
- Audit and test the existing eight effects.
- Improve parameter ranges, channel behavior, reset and smoothing.
- Add essential real DSP additions incrementally.
- Add finite-output, range, mono/stereo, block-size and sample-rate tests.
- Defer complex spectral/convolution implementations until architectures exist.

## Stage V2.2 — Rack and graph runtime

- Add stable node IDs and registry metadata.
- Add graph validation and bounded module limits.
- Preserve existing serial rack behavior with migration.
- Introduce prepared graph swapping with tested lifetime management.
- Add routing, scrolling and rack operations only after realtime safety is established.

## Stage V2.3 — Deterministic GENERATE engine

- Implement bounded offline description parsing.
- Select only registered modules.
- Configure, validate and preview a candidate graph safely.
- Add seeded variations and generated preset export.
- Never execute generated code or require an API key for DSP.

## Stage V2.4 — UI and visual feedback

- Coherent rack/library/preset navigation.
- Responsive controls bound to real parameters.
- JUCE 2.5D presentation first.
- Safe, configurable analysis and animation.

## Stage V2.5 — Modulation and presets

- Stable host parameters and bounded modulation.
- MIDI Learn and automation where explicitly tested.
- Versioned, validated `.nfx`/`.nrack` presets.
- Import, export, migration and search.

## Stage V2.6 — AI Studio and Sample Lab

- Local procedural sample tooling only with real implemented controls and export.
- Optional cloud provider boundaries without ordinary DSP dependencies.
- No simulated/incomplete instrument features.

## Stage V2.7 — stabilization

- Native build and test matrix.
- Windows/macOS VST3 and Standalone verification.
- State, latency, CPU, graph and DSP tests.
- Release documentation and known-issue record.
