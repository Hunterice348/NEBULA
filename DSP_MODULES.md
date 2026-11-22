# NEBULA DSP inventory and concurrency notes

> Inventory at current implementation. Planned modules are not implemented effects.

## Registered effects

| ID | DSP | Parameters | Concurrency status |
|---|---|---|---|
| `reverb` | JUCE `dsp::Reverb`; decay maps into room-size range | Size, Decay, Damp; mix/on | Ordinary effect fields and JUCE setter may race UI edits. |
| `delay` | Preallocated two-second ring delay; mono left-line, stereo dual-line | Time, Feedback; mix/on | Ordinary time/feedback fields can race. |
| `distortion` | tanh saturation + low-pass filtering | Drive, Tone; mix/on | Tone coefficient replacement is not synchronized with DSP; unsafe concurrent parameter update. |
| `chorus` | JUCE `dsp::Chorus` | Rate, Depth; mix/on | Fields and JUCE setter calls need audio-thread ownership. |
| `phaser` | JUCE `dsp::Phaser`, feedback bounded below unity | Rate, Depth, Feedback; mix/on | Fields and JUCE setter calls need audio-thread ownership. |
| `compressor` | JUCE `dsp::Compressor`; fixed 100 ms release | Threshold, Ratio, Attack; mix/on | Fields and setter calls need audio-thread ownership. |
| `bitcrusher` | Quantization + persistent per-channel sample hold | Bits, Rate; mix/on | Ordinary controls can race despite audio-owned hold state. |
| `filter` | JUCE TPT low-pass with bounded resonance | Cutoff, Resonance; mix/on | Fields and JUCE setter calls need audio-thread ownership. |

Legacy IDs and `on`, `mix`, indexed `pN` state remain the compatibility format. The base class preallocates its wet/dry buffer during `prepare`; oversized input is bypassed by that effect rather than causing buffer resize.

## Concurrency warning

Atomic enable/mix controls do not make an effect or rack thread-safe. The current UI retains `EffectBase&`; rack and preset methods can destroy effects while `processBlock` traverses them. Per-effect parameter fields are ordinary floats, and some setters mutate JUCE DSP processors. These hazards remain. See `RUNTIME_OWNERSHIP.md` for the audit and selected design.

The spectrum analyzer now uses a bounded fixed-record SPSC queue with a single audio producer and single UI consumer. When full, it drops the newest frame; no slot is overwritten while being read. This does not support concurrent multiple editor consumers.

## Not implemented

No distinct implementation is registered for parametric EQ, multiband compression, transient shaping, tape emulation, ping-pong/multi-tap delay, convolution reverb, granular processing, spectral freeze, frequency shifting, ring modulation, waveshaping, noise generation, filter bank or modulation matrix.

## Verification

The repository includes a queue test source and CTest target. Build/test execution has not been performed for the queue change; no effects are claimed newly tested or validated.
