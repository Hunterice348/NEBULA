# DSP modules

> **Status:** Inventory of observed baseline modules. Proposed catalogue entries are not implementations.

## Current registered modules

| ID | Display name | Parameters | Baseline implementation |
|---|---|---|---|
| `reverb` | Reverb | Size, Decay, Damp, mix | JUCE Reverb; decay is stored but not applied in `updateParams`. |
| `delay` | Delay | Time, Feedback, mix | 2-second stereo vector delay. Mono behavior needs correction/tests. |
| `distortion` | Distortion | Drive, Tone, mix | tanh gain plus low-pass filter. |
| `chorus` | Chorus | Rate, Depth, mix | JUCE Chorus. |
| `phaser` | Phaser | Rate, Depth, Feedback, mix | JUCE Phaser. |
| `compressor` | Compressor | Threshold, Ratio, Attack, mix | JUCE Compressor; release fixed. |
| `bitcrusher` | Bitcrusher | Bits, Rate, mix | Quantization/sample hold. |
| `filter` | Filter | Cutoff, Resonance, mix | JUCE TPT filter configured low-pass. |

All share enabled/mix behavior and indexed `pN` ValueTree serialization. The shared wet/dry path copies/resizes a buffer during processing and is a realtime-safety concern.

## Not implemented in baseline

No registered module implementations were observed for the extensive proposed dynamics, saturation, delay, reverb, filter, creative, spatial, instrument or modulation lists. A UI label or planned name does not count as implementation.

## V2 module contract

Add only through a documented interface:

- Stable type and state IDs
- Parameter schema, ranges, defaults and units
- Channel-layout requirements
- `prepare/reset/process` lifecycle
- Atomic or safely smoothed control exchange
- Bounded processing with no allocation or blocking work
- State validation and migration
- CPU/latency/resource metadata
- Tests for finite output, ranges, channels, block sizes and sample rates

## Quality priorities

1. Correct and test existing effects before catalogue expansion.
2. Add essential effects one at a time with real parameter/state/tests.
3. Oversample nonlinear effects only after aliasing and latency analysis.
4. Defer convolution/spectral modules until overlap, memory and latency architecture exists.
5. Do not claim effect coverage beyond registered and tested module IDs.
