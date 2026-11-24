# NEBULA V2 audio foundation audit

Audit target: `feature/nebula-v2-audio-foundation`.

## Baseline inspected

The repository initially contained a JUCE AudioProcessor/APVTS implementation (`Source/PluginProcessor.*`), a resizable editor with seven parameter attachments (`Source/PluginEditor.*`), smoothing and atomic metering utilities (`Source/Core`), a standalone DSP test that only tested a local saturation helper, CMake/JUCE VST3 and Standalone configuration, Windows workflow and Python scripts. The misspelled `Sourse/Core/ParameterSmoothing.h` is legacy and remains untouched. `README.md` and `docs/ARCHITECTURE.md` did not exist before the foundation work.

The Windows workflow's initial required-file list referenced Drum/Synth and effect-engine sources that were not present. CMake's JUCE dependency is declared as the 8.0.6 Git tag; `set(JUCE_VERSION 8.0.6)` is metadata, not a runtime assertion that the fetched source reports that version.

## Implemented Stage 2 foundation

- Extracted the production processing path into `NebulaAudioEngine` and concrete delay/saturation DSP classes.
- Preserved APVTS parameter IDs and editor attachments. APVTS state continues to serialize via JUCE XML.
- Added sample-rate-aware per-channel delay storage, mono and multichannel engine input, variable callback-size processing, input/output gain, dry/wet, parameter ramps, bounded feedback, and non-finite sample sanitation.
- Added host bypass pass-through and `EffectProcessor`/`EffectBase` lifecycle and mixing contracts.
- Replaced helper-only DSP tests with direct tests of production engine/effect code, including repeated-target ramp convergence, multiple delay rates, variable blocks, finite output, and wet/dry behavior.

## Validation status and known gaps

The repository write creates code and tests but does not execute CMake, a compiler, CTest, or a Windows workflow. Build and test results must be reported only after those commands actually run. In particular, VST3/Standalone and processor state round-trip are not verified here. The declared 8-channel upper bound is a host-layout guard, not a guarantee every format/host supports every channel arrangement. Full effect chains and the larger NEBULA FX/DRUM/SYNTH roadmap remain unimplemented.
