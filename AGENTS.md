
# NEBULA — MASTER ENGINEERING SPECIFICATION

## 1. PROJECT IDENTITY

NEBULA is a professional modular audio software suite.
It is not a demo, toy, mockup or collection of placeholder controls.

Primary products:
- NEBULA FX — professional multi-effect audio processor.
- NEBULA DRUM — synthesizer/sample-based drum machine.
- NEBULA SYNTH — advanced software synthesizer.

Priority:
1. Shared audio architecture.
2. NEBULA FX.
3. NEBULA DRUM.
4. NEBULA SYNTH.
5. Unified premium interface.
6. Validation and release.

## 2. DEVELOPMENT ENVIRONMENT

- Language: C++17.
- Framework: JUCE 8.0.6.
- Build system: CMake.
- Primary platform: Windows 10/11 x64.
- Plugin format: VST3.
- Application format: Standalone.
- CI/CD: GitHub Actions.
- Build environment: GitHub-hosted runners.
- No local build requirement for the project owner.

All dependencies must use pinned, trusted versions.
Do not download or execute arbitrary binaries.
Do not use unverified third-party repositories.

Read the JUCE license before distribution.

## 3. SHARED CORE

Implement reusable modules:

- AudioEngine
- DSP utilities
- ParameterManager
- ParameterSmoothing
- MIDIEngine
- TransportManager
- PresetManager
- StateManager
- ModulationMatrix
- RealtimeSafeQueue
- AudioMetering
- PluginConfiguration

Shared modules must not duplicate DSP implementations.

## 4. NEBULA FX

Implement a real-time audio effects processor.

Required architecture:
- Serial effect chains.
- Parallel effect routing.
- Effect enable/bypass.
- Per-effect wet/dry.
- Input/output gain.
- Stereo processing.
- Parameter automation.
- Preset serialization.
- Safe parameter updates.

Required effects:
Reverb, Plate Reverb, Hall Reverb, Delay,
Ping-Pong Delay, Granular Delay, Compressor,
Multiband Compressor, Limiter, Gate, Distortion,
Saturation, Bitcrusher, Chorus, Phaser, Flanger,
Tremolo, Ring Modulator, Auto Pan, Stereo Widener,
Parametric EQ, Filters and Transient Shaper.

Every effect must have:
- Real DSP implementation.
- Documented parameters.
- Stable parameter IDs.
- Correct reset and prepare lifecycle.
- Mono and stereo support where applicable.
- Unit tests.
- No placeholder processing.

## 5. NEBULA DRUM

Implement a professional drum instrument.

Required:
- 16 independent drum tracks.
- Kick, Snare, Hi-Hat, Clap, Tom and Percussion.
- Synthesized drum voices.
- WAV sample playback.
- Sample import.
- 16-step and 32-step sequencing.
- Velocity.
- Swing.
- Probability.
- Ratchets.
- Microtiming.
- Pattern storage.
- Pattern chaining.
- MIDI synchronization.
- MIDI export.
- Per-track processing.
- Preset and project state saving.

The sequencer must support accurate timing.
Do not process all MIDI events at block start.

## 6. NEBULA SYNTH

Implement:
- Multiple oscillators.
- Sub oscillator.
- Noise generator.
- Wavetable synthesis.
- FM and AM synthesis.
- Unison.
- Polyphonic voice manager.
- ADSR envelopes.
- LFO.
- Modulation matrix.
- Filter engine.
- Arpeggiator.
- MIDI input.
- Preset system.

## 7. USER INTERFACE

Visual identity:
- Industrial gothic.
- Dark metallic surfaces.
- Premium 3D-inspired controls.
- Red, purple and blue accent lighting.
- High-quality knobs and switches.
- Audio meters.
- Waveform and spectrum visualizers.
- Resizable interface.
- HiDPI support.
- Responsive layout.

Every visible control must operate a real parameter.
No decorative non-functional controls.

## 8. REAL-TIME AUDIO SAFETY

The audio callback must not:
- Allocate or free memory.
- Access the filesystem.
- Perform network operations.
- Wait on blocking mutexes.
- Sleep.
- Perform unbounded processing.
- Log to the console.

Use parameter smoothing and thread-safe communication.
Protect against NaN and infinity.
Avoid audio clicks during parameter changes.

## 9. PORTABILITY

- No hardcoded installation drive.
- No absolute project paths.
- Resources must resolve relative to the application/plugin bundle.
- Presets must use a portable and writable user-data strategy.
- Support custom VST3 scan paths.
- Do not assume a specific DAW installation directory.

## 10. GITHUB AUTOMATION

GitHub Actions must:
- Configure CMake.
- Acquire pinned JUCE sources.
- Build VST3.
- Build Standalone.
- Run automated tests.
- Validate generated artifacts.
- Run security and dependency checks.
- Upload build artifacts.
- Produce readable reports.

A failed build must never be reported as successful.

AI-generated patches must be reviewed and tested.
Never silently bypass failed tests.
Never delete project files to make an audit pass.
Never force-push.
Never modify main directly through an unreviewed repair process.

## 11. QUALITY GATES

A feature is complete only when:
- Its implementation exists.
- It is connected to the actual audio engine.
- Its parameters work.
- Its state can be restored.
- Its tests pass.
- Its documentation exists.

A keyword or filename match is not proof of implementation.

A release is complete only when:
- VST3 builds successfully.
- Standalone builds successfully.
- All registered tests pass.
- Security checks pass.
- Release artifacts exist.
- Final audit passes.

## 12. DEVELOPMENT RULE

Always inspect the existing repository before changing it.
Preserve working functionality.
Implement complete files, not disconnected snippets.
Prefer small, testable modules.
Do not introduce placeholder implementations.
Document every architectural change.

NEBULA must become a real, stable,
professional audio software suite.
