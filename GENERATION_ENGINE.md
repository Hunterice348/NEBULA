# GENERATION_ENGINE.md

> **Status:** Planning only. The current button is a keyword heuristic, not a generation engine.

## Current behavior

The editor lowercases text and appends modules by substring:

- `space` or `reverb` → reverb
- `echo` or `delay` → delay
- `warm` → distortion

It has no parser, graph validation, parameter generation, CPU estimate, preview, variation or generated-preset save.

## Planned offline pipeline

1. Normalize bounded text input.
2. Parse a documented deterministic intent vocabulary.
3. Select registered module IDs only.
4. Configure valid parameter values.
5. Build an explicit serial candidate graph.
6. Validate IDs, endpoints, ranges, cycles and module limits.
7. Estimate resources only from implemented measured/designed metadata.
8. Prepare and preview a candidate outside `processBlock`.
9. Publish only through a tested safe graph switch.
10. Save validated graph data and support seeded variations.

## Structured boundary

Generation must create typed C++ configuration or a validated schema, never source code or executable data. Serialized candidates must include schema version, seed/provenance, module IDs, instance IDs, parameter values, connections and supported UI metadata.

Reject unknown modules, duplicate IDs, invalid edges, non-finite values, unsupported cycles and unknown required schema versions.

## Safety

- No generated C++ compilation or runtime code execution.
- No network dependency for ordinary DSP.
- No direct active-graph modification by background workers.
- No graph allocation/destruction or filesystem/UI work in `processBlock`.
- Unsupported requests must not silently become fake modules.
- Variations and presets require the same validation path as initial generation.

## Initial scope

Only explicit combinations of existing, tested modules should be supported initially. Unsupported features such as granular delay or convolution reverb must not appear as successful generation output until their actual modules exist.

Optional cloud providers can later adapt to the validated configuration boundary. They must not become a dependency for baseline operation.
