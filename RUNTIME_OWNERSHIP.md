# NEBULA runtime ownership and thread-safety audit

> Audit baseline: `f672e71ac97536f29b4f30215ef8ca10560ee79a`. This document distinguishes observed behavior from the limited spectrum-transfer improvement in the follow-up commit. It does not claim the live rack is thread-safe.

## Observed ownership and access

- `NebulaAudioProcessor` owns `Rack`, `PresetManager`, APVTS, master scratch buffers, FFT state and (after the isolated telemetry change) a bounded spectrum queue.
- `Rack` owns effects through `std::vector<std::unique_ptr<EffectBase>>`. `processBlock` traverses the same rack via `rack.process`.
- UI `EffectCard` stores an `EffectBase&`, and callbacks call `setEnabled`, `setMix`, and `setParam`. Rack rebuild reads raw pointers via `Rack::get` and creates these references.
- `PresetManager` stores a `Rack&`. Factory and user preset loads call `clearAll`, `addEffect`, or `loadState`; plugin `setStateInformation` calls `rack.loadState`.
- Effect state consists of normal floats (per-effect controls) and JUCE DSP processors. `enabled` and `mixAmount` are atomic after the prior patch, but effect control fields and DSP setter calls are not synchronized.
- APVTS owns the three master host parameters. `processBlock` reads raw parameter atomics; smoothing state is processor-owned. Preset state and rack state are serialized separately through ValueTrees.
- Before the spectrum change, audio wrote a shared ordinary `spectrumData` array and UI read it under an atomic ready flag. The flag did not make array accesses race-free.

## Risks and exact categories

| Path | Risk |
|---|---|
| `processBlock -> Rack::process -> effects` vs `Rack::addEffect/removeEffect/moveEffect/clearAll/loadState` | Vector mutation, effect destruction and traversal may race; use-after-free or invalid iteration. |
| `EffectCard` direct references/callbacks vs rack edits | Dangling effect reference after removal/rebuild; ordinary effect parameter reads/writes race. |
| Distortion `setParam -> updateTone` vs `processWet` | Coefficients and filter state can be accessed concurrently; atomic floats alone cannot protect JUCE coefficient objects. |
| Reverb, compressor, chorus, phaser, filter setters vs DSP processing | DSP processor state may be mutated concurrently; JUCE objects are not presumed thread-safe. |
| Preset load/state restore vs processing | Live rack destruction/replacement can occur while audio uses it. APVTS replacement and rack update are not one atomic transaction. |
| Preset save/getState vs parameter/rack edits | Snapshot can race and can represent inconsistent rack/effect state. Filesystem/XML work is outside processBlock in observed paths, but state callback thread assumptions are host-dependent. |
| Spectrum prior shared array | Data race between audio writer and UI reader. |
| Master APVTS reads | Scalar atomic loads are appropriate; smoother is audio-thread-owned. Host/APVTS internal safety still relies on JUCE's parameter contract. |
| Audio buffers | Prior change preallocated effect dry buffers at prepare. Processor master dry buffer is prepared, with a capacity fallback added alongside telemetry. Other JUCE internals should be verified before claiming zero allocations globally. |

No explicit mutex lock is present in `processBlock`. The principal hazards are unsynchronized mutation and destruction, not a known audio-thread lock. Filesystem work is not called directly in `processBlock` in the inspected paths.

## Architecture alternatives

### A. Immutable prepared snapshots with deferred reclamation

Builder creates and prepares an immutable rack snapshot off audio. Audio adopts a pointer only at a block boundary, processes only that snapshot, and returns the old pointer through an audio-to-builder retirement channel. This is sound only if reclamation is explicit: an atomic pointer swap alone does not ensure the old object is no longer in use. Queue full/backpressure must prevent a swap when retirement capacity is unavailable.

### B. Preallocated fixed slots

A fixed number of effect slots avoids pointer lifetime changes, but changing type/parameters/stateful DSP in-place remains unsafe. Reusing an inactive slot requires quiescence and prepare/destruction outside audio; slot exhaustion and slot generation IDs need protocol. This may be useful for bounded host parameters, not sufficient as the only rack strategy.

### C. Audio-owned rack plus lock-free commands

Audio owns effects and applies commands, but effect construction/preparation/destruction must not occur in the callback. Commands would need to refer to already constructed prepared objects and return retired objects to a builder. This converges on snapshot/handoff with more per-command complexity.

## Selected direction (design, not yet live-rack implementation)

Use a serialized off-audio builder plus immutable prepared serial-rack candidates and bounded SPSC ownership transfer. One producer (builder) publishes complete candidates; one consumer (audio) adopts at a block boundary; audio returns old candidates through a second SPSC retirement queue; builder alone destroys retired/cancelled candidates. Audio checks retirement capacity before consuming a replacement, so it never waits and never loses ownership. Publication failure retains/coalesces work off audio. Multiple UI, host state and preset requests must first be serialized to the single builder producer.

Object destruction rules:

1. Candidate is builder-owned until successful enqueue.
2. Incoming queue owns a published candidate until audio dequeue.
3. Audio owns only the active rack.
4. After a successful boundary swap, retirement queue owns the old rack.
5. Builder owns/deletes retired racks after dequeue.
6. Shutdown stops audio callbacks before draining queues and destroying active/pending racks.

The queue protocol still needs implementation plus stress tests before applying this to the live rack. Host callback serialization and teardown order must be verified for plugin and Standalone lifecycles.

## Parameter protocol

Do not pass effect references/pointers into UI controls. Use stable rack/effect identity with generation tokens. UI/host writes bounded scalar targets to atomics or a fixed-capacity command mailbox; audio reads targets at a block boundary and owns smoothing and all JUCE DSP state. Compound updates, coefficients and topology must be prepared/published as a coherent candidate, not assembled through unrelated atomics. Preset writes need an immutable control snapshot or builder-owned state source. Existing IDs and `pN` ValueTree properties remain the migration input.

The current controls do not implement this protocol. The existing per-effect ordinary floats remain unsafe under concurrent edits.

## Preset protocol

Parse/validate legacy XML and build a candidate away from audio. Preserve APVTS master IDs, effect IDs, `on`, `mix` and indexed `pN`. Only mark a preset active after successful publication. Save from a coherent state snapshot; do not read live effect objects concurrently. Current implementation directly mutates live rack and is not safe under concurrent audio operation.

## Spectrum transfer now implemented

`SpectrumFrameQueue` is a fixed-capacity (four record) SPSC ring of fixed-size 512-bin records. Producer is the audio callback; consumer is the single UI timer. Producer writes a whole slot, then release-publishes the write index. Consumer acquire-loads the write index, copies the slot to its own local frame and release-publishes the read index. Producer acquire-loads the read index before reusing a slot. If full, producer drops the new frame without waiting or overwriting. UI drains queued records and renders its own local snapshot; stale frames are naturally skipped when several are available.

This mechanism requires exactly one consumer. It must not be shared by multiple simultaneous editors. Frame data and indices are fixed-size; the audio callback does no queue allocation or waiting. Queue correctness has targeted standalone stress-test source, but that test was not executed in this session.

## Implemented isolated improvement

The follow-up change replaces the shared mutable spectrum array with the SPSC frame queue and UI-local copy. It does not change effect/rack ownership or per-effect parameter communication. It also adds a bounded master dry-buffer capacity check, but that is not proof of global callback allocation freedom.

## Remaining blockers

- Rack mutation and effect destruction still race active processing.
- Effect-specific float fields remain concurrently accessed by UI/DSP.
- Distortion coefficient update and JUCE DSP setter concurrency remain unresolved.
- Preset/state restoration still replaces live rack directly.
- UI EffectCard references can dangle after rack replacement.
- Multi-editor spectrum consumption is unsupported by SPSC; enforce single consumer or redesign if multiple editors are required.
- Full callback allocation-freedom has not been measured; JUCE implementation behavior and host block-size edge cases need instrumentation.
- No runtime ownership test harness, sanitizer, or build has been run for this change.

## Validation status

A standalone queue test source checks capacity/drop/FIFO behavior and concurrent order/content over 200,000 records. It is included in CMake/CTest. No compiler, CMake configure, test run, sanitizer, plugin validation or artifact inspection was available during this source update. Do not treat the test source as a passing test result.
