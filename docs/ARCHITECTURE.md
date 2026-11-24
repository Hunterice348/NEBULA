# Audio architecture (implemented foundation)

## Audio path

`NebulaFXAudioProcessor::processBlock` reads the APVTS `std::atomic<float>` parameter values with relaxed loads, validates their ranges, converts gain and delay time to DSP units, and submits a small parameter snapshot to `NebulaAudioEngine`. The snapshot and all engine state are consumed on the audio thread; GUI code does not mutate engine objects or take a shared lock.

The engine processes channel pointers in place, sample by sample. It has no audio-thread allocation, file access, logging, or blocking synchronization. The effect delay memory is sized during `prepare`; `release` frees that memory through JUCE's resource lifecycle. Processing accepts callback sizes different from the prepared nominal block size. The processor advertises identical input/output layouts up to eight channels; support depends on what the host and JUCE format expose.

## DSP contract

`EffectProcessor` specifies `prepare`, `reset`, and bounded `processSample` operations. `EffectBase` centralizes finite input protection and linear wet/dry interpolation. `NebulaDelayEffect` owns one feedback line per channel and uses the configured sample delay. `NebulaSaturationEffect` implements the retained tanh waveshaper. The plugin's Clean mode bypasses the effect stage while retaining input/output gain staging; host bypass returns the untouched buffer.

The delay and saturation are concrete implementations, not placeholder processors. The interface is an extension point only; effects not implemented here should not be treated as available.

## Parameters and state

The existing IDs (`input`, `output`, `mix`, `drive`, `delay`, `feedback`, `mode`) and editor attachments are preserved. APVTS atomics are read by the audio callback, then targets are updated on the audio thread. The linear smoother keeps its current ramp when a repeated unchanged target arrives, so block-by-block snapshots do not prevent convergence. Parameter state is serialized/restored using JUCE's existing APVTS XML state path.

## Constraints and next work

- There is no processor-level JUCE state save/load round-trip test in the standalone DSP test target yet.
- Parallel/serial effect chains, per-effect bypass controls, MIDI/transport, preset storage, and the specified effect catalogue remain future work.
- The GUI/editor still uses the original controls; there is no dedicated multichannel UI metering or runtime layout selector.
- Validate actual VST3 and Standalone builds using the Windows workflow; do not infer them from the DSP-only test build.
