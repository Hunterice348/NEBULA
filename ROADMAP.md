# NEBULA V2 roadmap

> **Status:** Planning only. A roadmap entry is not an implemented or tested feature. Baseline observations are recorded separately in `ARCHITECTURE.md`.

## Stage V2.0 — Architecture and baseline (current stage)

- Inspect the repository, CMake/source layout, DSP, UI and preset implementation.
- Verify the existing Stage 0 Actions result and document exactly what it does and does not verify.
- Record architecture risks and incremental target design.
- Add/update architecture, module, generation and build documentation without changing DSP or UI.
- **Exit gate:** documentation reviewed; no product code change; user review/approval before V2.1.

## Stage V2.1 — DSP foundations and essential expansion

- Specify module/parameter contracts and real-time constraints before extending the effect catalogue.
- Audit and fix the existing eight effects, including effect-specific parameter application, mono/stereo handling, reset behavior, range handling and automation/control access.
- Prioritize a small, testable set of essential effects (for example a limiter/clipper, gate, saturation and stereo delay) only after implementation designs and quality criteria are agreed.
- Add deterministic DSP tests: finite output, silence/pass-through behavior where applicable, ranges, sample rates, block sizes, channels and parameter/state round trips.
- Do not claim unimplemented or superficial effects. Defer spectral, convolution and complex multiband processing until their architectures and tests are ready.
- **Exit gate:** CI build and tests pass; review DSP behavior and audio-thread safety before V2.2.

## Stage V2.2 — Rack and safe runtime graph

- Introduce stable module/node IDs, descriptors, a registry and validated serial graph representation.
- Keep compatibility/migration for existing serial rack state.
- Add limits, graph validation and testable serial execution before branching/parallel routing.
- Design and test prepared-graph publication and retirement so audio never observes freed graph memory and never constructs/destroys graphs.
- Add rack editing operations and usable scrolling only after the control/audio ownership model is established.
- Add parallel/split/merge/send/return/sidechain only with defined buffer, channel, latency and resource semantics. Feedback is an explicit bounded delayed node, never an ordinary cycle.
- **Exit gate:** concurrency/lifetime review, graph tests and audio stability tests pass.

## Stage V2.3 — Deterministic GENERATE engine

- Implement offline parser/intent model using a documented finite supported vocabulary and actionable unsupported-request errors.
- Select existing registered modules and deterministic parameter presets; produce a structured candidate graph.
- Validate IDs, topology, parameter ranges, limits and estimated resource budget before preview.
- Prepare/preview a candidate without mutating the active graph; switch only after an explicit safe request.
- Add deterministic seeds/variations and save/export through validated preset formats.
- No generated C++ execution; cloud providers remain optional future adapters, not a DSP dependency.
- **Exit gate:** parser/generator/validator/serialization tests and real audio processing of generated chains.

## Stage V2.4 — UI and visual feedback

- Design coherent navigation and responsive rack/preset/generation flows.
- Upgrade controls only where bound to real parameters; add edit feedback, tooltips and accessibility/scale considerations.
- Prefer JUCE 2.5D rendering initially. Profile animation, scrolling and spectrum visualization; add a reduced-animation/performance setting.
- Replace unsafe spectrum data sharing with a tested snapshot or lock-free transfer design before adding more analysis.
- **Exit gate:** UI functional review, CPU/render performance, scale and host-window tests.

## Stage V2.5 — Modulation and presets

- Add bounded modulators and destinations, smoothing and safe multi-threaded parameter exchange.
- Keep host parameter registration stable and finite; add MIDI learn only with explicit mapping/state and host compatibility tests.
- Introduce versioned, validated portable preset data with migration, import/export, favorites/search and generated-rack save after the schema is stable.
- **Exit gate:** state restoration, automation and migration tests pass.

## Stage V2.6 — AI Studio and Sample Lab foundations

- Implement local procedural sample generators only when their actual audio generation, controls, preview, export and tests are ready.
- Define provider boundaries for optional online AI; no network/API key needed for ordinary DSP.
- Avoid representing planned assistant, instrument, sampler, groovebox or sequencer sections as functional until implemented.
- **Exit gate:** real outputs, reproducibility, offline operation and file export verified.

## Stage V2.7 — Stabilization and release readiness

- Verify supported channel layouts, sample rates, block sizes, latency, state restoration, CPU/resource limits and realtime safety.
- Build Windows and macOS VST3 and Standalone, verify artifact contents and run plugin validation where available.
- Confirm docs describe implemented behavior, archive known issues and prepare release notes.
- **Exit gate:** complete CI matrix, artifact verification, tests and user approval.

## Stage approval policy

Work stops at each stage exit gate for review. In particular, V2.1 does not begin until Stage V2.0 has been reviewed and approved. Later stages also require build/test evidence and review before proceeding.
