# DSP modules

> **Status:** Inventory of current baseline plus implementation guidance. The proposed catalogue is not a statement that those effects exist. At the inspected baseline commit, only the eight modules listed as current below are registered in the rack.

## Current registered effects

`Rack::create` recognizes these stable string IDs:

| ID | Display name | Parameters exposed by module | Baseline implementation notes |
|---|---|---|---|
| `reverb` | Reverb | Size, Decay, Damp; shared enable/mix | JUCE `dsp::Reverb`. `decay` is serialized but currently not applied to JUCE reverb parameters. |
| `delay` | Delay | Time, Feedback; shared enable/mix | Preallocated-at-prepare stereo vectors, up to 2 seconds. Wet processor returns without processing if fewer than two channels; feedback/time handling needs review. |
| `distortion` | Distortion | Drive, Tone; shared enable/mix | `tanh` drive followed by per-channel low-pass filters. Coefficient updates occur from `setParam`; thread-safety/smoothing needs review. |
| `chorus` | Chorus | Rate, Depth; shared enable/mix | JUCE `dsp::Chorus`; parameters are set during processing. Channel-layout and control synchronization need review. |
| `phaser` | Phaser | Rate, Depth, Feedback; shared enable/mix | JUCE `dsp::Phaser`; parameters are set during processing. Control synchronization needs review. |
| `compressor` | Compressor | Threshold, Ratio, Attack; shared enable/mix | JUCE `dsp::Compressor`; release is fixed internally. Control synchronization and complete parameter interface need review. |
| `bitcrusher` | Bitcrusher | Bits, Rate; shared enable/mix | Quantization and sample holding. The hold counter is local to each block, so continuity across blocks requires review. |
| `filter` | Filter | Cutoff, Resonance; shared enable/mix | JUCE TPT state-variable filter configured as low-pass. Type selection and channel handling need review. |

`EffectBase` exposes `prepare`, `reset`, type/name/parameter methods, enabled and normalized wet/dry state, plus a ValueTree save/load convention. In the baseline, the shared wet/dry process path resizes/copies a dry buffer during processing, which is an audio-thread allocation risk. Its ordinary bool/float state is not a safe cross-thread automation interface.

## Not currently implemented as distinct effects

No distinct rack registrations were observed for limiter, clipper, gate, expander, transient shaper, multiband/OTT dynamics, sidechain compression, tape/tube saturation, wavefolder, flanger, tremolo, ring/frequency shifting, ping-pong/tape/granular delay, plate/convolution/shimmer reverb, EQ families, spectral effects, glitch/stutter, spatial processors, samplers, instruments, modulators or utility nodes. Some named families may be approximated through existing generic effects only; do not present that as a distinct implementation.

## Proposed module contract (to design and test in V2.1/V2.2)

Each module should have:

- Stable type ID and state/schema version; stable instance identity belongs to graph nodes, not a generated class name.
- Declarative parameter metadata: stable parameter key, normalized/real range, default, units, mapping, smoothing policy and whether host automation is exposed.
- Explicit supported channel count/layout and port types; clear mono-to-stereo behavior.
- `prepare(sampleRate, maximumBlockSize, channelLayout)` and `reset`, plus bounded processing with no file/network/UI work, allocation or blocking locks.
- Parameter/control publication that avoids unsynchronized UI mutation. Expensive coefficient or graph preparation occurs outside the callback or at bounded safe update points.
- State serialization with validation, finite/range checks and migration; reject or safely default invalid state.
- Resource estimate/limits and latency reporting where applicable.
- Tests for silence, finite output, state, parameter boundaries, channels, block sizes, sample rates, reset and pathological input.

The current `EffectBase` interface does not yet satisfy this complete contract; this section is a target specification, not an existing API.

## DSP implementation priorities and quality gates

1. First fix/test current behavior without deleting any registered effect; preserve saved IDs and add migrations if behavior/schema changes.
2. Select a few essential additions based on actual product needs and implementation capacity. Each must have meaningful DSP, documented parameter semantics, state handling and tests before being listed as implemented.
3. Plan nonlinear oversampling only after aliasing/latency/CPU tests demonstrate the need and the implementation can be prepared safely.
4. Defer convolution and large FFT/spectral processors until memory, latency, overlap/state handling and resource budgets are designed.
5. Add graph routing effects only after routing and latency semantics exist; a label or card with no signal processing is not an effect.

## State and compatibility

Current effect state is a `ValueTree` named by effect type with `on`, `mix` and indexed `pN` properties. The containing rack stores effect trees as ordered children. This is not yet a versioned portable graph format. Any future stable IDs, parameter names or state format must retain an explicit migration path for these existing trees.
