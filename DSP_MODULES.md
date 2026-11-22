# DSP modules

> Current implementation inventory. Planned effects are not implementations.

## Registered effects

| ID | Parameters (normalized unless stated) | Processing |
|---|---|---|
| `reverb` | Size, Decay, Damp, Mix | JUCE `dsp::Reverb`; decay now maps monotonically to the algorithm's room-size control (JUCE's algorithm has no independent T60 parameter). |
| `delay` | Time (0–1, up to 2 s), Feedback (0–1 clamped to 0.95) | Preallocated two-second delay lines; mono now uses the left line and stereo uses independent lines. |
| `distortion` | Drive, Tone | `tanh` waveshaping followed by per-channel low-pass filtering. |
| `chorus` | Rate, Depth | JUCE `dsp::Chorus`. |
| `phaser` | Rate, Depth, Feedback | JUCE `dsp::Phaser`; feedback is bounded below unity. |
| `compressor` | Threshold, Ratio, Attack | JUCE `dsp::Compressor`; release remains fixed at 100 ms. |
| `bitcrusher` | Bits (2–16), Rate (1–32 hold factor) | Quantizer and sample hold; phase and held sample persist across process calls per channel. |
| `filter` | Cutoff (20 Hz to 0.45 × sample rate), Resonance (bounded) | JUCE TPT low-pass filter. |

Legacy effect IDs and indexed `pN` ValueTree properties are preserved. The base wet/dry buffer is allocated by `prepare`, not resized in `EffectBase::process`; a block exceeding prepared buffer capacity passes through the affected effect unchanged until re-preparation.

## Limitations and safety status

This change does **not** make the live rack architecture thread-safe. Rack replacement, effect parameter fields, distortion coefficient updates, JUCE DSP setter calls, UI-held effect references and spectrum transfer still need a separately designed ownership/communication solution. Atomic enable/mix values do not make all other effect controls safe for concurrent UI/audio access. Rack add/remove/preset loading must not be used concurrently with audio until that work is implemented and tested.

No new effects are registered by this change. Multiband compression, transient shaping, tape, granular, spectral, frequency-shift, convolution and modulation systems remain unimplemented.

## Verification status

No build or DSP test execution was available for this source update. The implementation is not claimed compiled or validated. Existing Stage 0 workflow evidence predates this change.
