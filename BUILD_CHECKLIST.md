# Build verification checklist

This checklist covers the current NEBULA JUCE plugin targets only. It does not cover proposed future product systems.

## Prerequisites

- A C++17-capable compiler and CMake 3.22 or newer.
- The JUCE 8.0.6 source tree in `JUCE/` (the repository's GitHub Actions workflow clones this version).
- Platform build tools: Visual Studio 2022 with the C++ workload on Windows, or Xcode and command-line tools on macOS.

## Windows (Visual Studio 2022, x64)

From the repository root in PowerShell, clone JUCE if `JUCE/` is not present, then configure and build:

```powershell
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -A x64
cmake --build build --config Release
```

Verify both targets were produced under `build`:

- VST3: `build/NebulaMultiFX_artefacts/Release/VST3/NEBULA.vst3`
- Standalone: `build/NebulaMultiFX_artefacts/Release/Standalone/NEBULA.exe`

The exact intermediate directory layout can vary by generator; search under `build` for `*.vst3` and `NEBULA.exe` if needed. A successful build command alone is not sufficient if either target was not generated.

## macOS (Xcode)

From the repository root, clone JUCE if `JUCE/` is not present, then configure and build both plugin formats:

```sh
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
cmake --build build --config Release
```

Verify the VST3 bundle and Standalone application exist under `build` (search for `*.vst3` and `NEBULA.app`).

## GitHub Actions

The existing `.github/workflows/build.yml` builds on Windows and macOS and uploads the VST3 artifacts. Check the run's build steps for success and download the artifacts to confirm the VST3 output. The current workflow does not upload the Standalone target; confirm Standalone separately from the build output or by adding an artifact upload in a separately approved change.
