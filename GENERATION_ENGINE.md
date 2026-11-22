# Generation engine

> **Status:** The repository contains a text field and a minimal keyword-based rack append action. The deterministic structured effect-generation architecture in this document is proposed and is **not implemented**. Current behavior must not be represented as AI, natural-language understanding or a validated DSP graph generator.

## Baseline behavior

The editor lowercases the description and appends reverb when text contains `space` or `reverb`, delay when it contains `echo` or `delay`, and distortion when it contains `warm`; then it rebuilds the rack UI. It does not clear an existing rack, configure generated parameters, validate a candidate, prepare a preview, estimate CPU, support variations, or save a generated preset. This keyword action edits the live rack and therefore inherits the baseline rack/audio-thread safety risk documented in `ARCHITECTURE.md`.

## Target behavior (planned)

Generation creates a **configuration** of registered modules and validated connections; it never creates or executes arbitrary code. Ordinary effect operation is offline and requires no API key or network access. The initial implementation is deterministic rules, not a claim of cloud AI.

### Proposed pipeline

1. **Request normalization:** bound input size, normalize casing/spacing and retain the original prompt for display. Do not execute prompt contents.
2. **Description parser:** map a deliberately documented vocabulary to a typed intent (requested effects, order/combinations, style adjectives, intensity/space qualifiers and optional seed). Return a useful unsupported/ambiguous request message rather than inventing unsupported modules.
3. **Module selection:** select only IDs registered by the module registry. Use deterministic mappings and documented ordering rules. Never select a module not actually available in the running build.
4. **Parameter configuration:** map qualitative descriptors to declared parameter IDs/ranges and clamp/validate values. Parameters not expressible in current module descriptors remain defaults.
5. **Graph construction:** build a candidate with stable node IDs and explicit serial connections initially. Parallel/modulation/feedback structures remain unsupported until their runtime semantics are implemented.
6. **Validation:** check schema/version, known module IDs, unique IDs, parameter keys/types/ranges, edge endpoints and port compatibility, DAG constraints, maximum module count, and any resource/latency budget available. Report why invalid candidates were rejected.
7. **Preparation and preview:** prepare the candidate away from the audio callback, keep active graph untouched, and make preview activation an explicit safe handoff after graph lifetime/retirement is implemented and tested. If the safe handoff does not yet exist, generation must not be wired to live processing.
8. **Variation:** derive deterministic alternatives from a user-visible seed and bounded parameter changes; validate each candidate using the same path.
9. **Persistence/export:** save the full validated graph and parameters in a versioned portable data format (planned `.nfx` for a generated effect and/or `.nrack` for a rack) through the preset system. Do not serialize executable code.

## Structured data boundary (proposal)

The internal model should be typed C++ data, not untrusted generated source. An eventual serialized representation should include a schema version; generation seed/provenance; module node IDs/type IDs; bounded parameter maps; typed edges; and any persisted modulation/routing metadata that the implementation actually supports. Parsing must reject unknown required schema versions, duplicate IDs, unknown modules, invalid edges and non-finite/out-of-range values. Optional fields require explicit compatibility rules. Avoid accepting arbitrary serialized callbacks, scripts, paths or executable content.

No wire schema is frozen at Stage V2.0. Implementers should propose and review one alongside the module and graph interfaces before shipping it.

## Initial supported scope (future implementation)

Start with simple requests composing the existing implemented effects only, for example explicit chorus + distortion + reverb, after those modules' parameter and safety behavior is validated. An adjective such as “dark” may only adjust documented parameters if a deterministic mapping has been reviewed. Do not infer “granular,” “shimmer,” “metallic resonator,” “AI sample” or other unsupported effects from prose. Show the resulting actual modules, parameter values, validation result and estimated resource data only when those values are truly calculated.

## Safety and verification criteria

- No generated C++ compilation, scripting, dynamic library loading or arbitrary code execution.
- No network request or blocking operation from the audio callback.
- No background worker directly edits the active audio graph.
- Candidate graph preparation and validation happen outside `processBlock`; live switching only after an explicitly tested safe lifetime strategy exists.
- Deterministic seed produces reproducible candidate data.
- Parser, module selection, ranges, invalid graph rejection, module limits, serialization round trips and variation bounds have automated tests.
- End-to-end test processes real input through the resulting registered DSP modules and checks output is finite; visual graph creation alone is not success.
- Unsupported requests never silently become fake DSP modules or claim CPU estimates that were not implemented.

Optional cloud provider adapters are a later, isolated enhancement and must return the same validated structured configuration boundary. They must not be a requirement for ordinary plugin DSP.
