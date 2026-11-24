# Build verification checklist

This checklist covers the current NEBULA JUCE plugin targets and Stage 0 evidence. It does not imply planned V2 features exist.

## Prerequisites

- C++17 compiler and CMake 3.22+.
- JUCE 8.0.6 source tree at `JUCE/`.
- Windows: Visual Studio 2022 C++ workload, x64.
- macOS: Xcode/command-line tools.

## Windows

```powershell
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -A x64
cmake --build build --config Release
```

Expected artifact locations:

- VST3: `build/NebulaMultiFX_artefacts/Release/VST3/NEBULA.vst3`
- Standalone: `build/NebulaMultiFX_artefacts/Release/Standalone/NEBULA.exe`

Search `build` and record the actual paths; generator layouts may vary.

## macOS

```sh
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
cmake --build build --config Release
```

Expected artifact locations:

- VST3: `*.vst3` under `build`
- Standalone: `NEBULA.app` under `build`

## Stage 0 CI evidence

Latest successful baseline run:

- [Build VST3 run 45029388928](https://github.com/Hunterice348/NEBULA/actions/runs/45029388928)
- Reported conclusion: success
- Reported branch: `feature/nebula-stage-0-build-fix`
- Reported commit: `77c53a9d43e0c703842aede8258e33fe936908b0`

The workflow builds Windows and macOS and uploads VST3 artifacts only. It does not verify Standalone downloads/launch, plugin-host behavior, or DSP tests.

Earlier `main` runs failed because UI implementation source files were missing from CMake. Stage 0 registered those sources and produced the successful baseline run.

The workflow is currently triggered by `main`/`master` pushes and manual dispatch, not development branch pushes. It must be triggered or its branch scope improved before branch CI verification.

## Stage V2.0 verification

- Repository tree, CMake and source layout reviewed.
- Existing checklist instructions preserved.
- Successful Stage 0 run metadata reviewed.
- Documentation-only changes prepared; no DSP or UI source modified.
- Native build not run through repository tools in this session.
- Standalone artifact download and host validation not run.
- CI run for this documentation commit not available until branch workflow triggering is configured or the branch is dispatched externally.

## Future code-stage gate

Record branch/SHA, Actions run/job links, build platform, test output, and actual artifact paths for every code stage. Before release, verify Windows/macOS VST3 and Standalone artifacts, state restoration, DSP tests and supported plugin-host behavior.
