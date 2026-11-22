# Build verification checklist

This checklist covers the current NEBULA JUCE plugin targets and records the verified Stage 0 CI baseline. It does not imply that planned V2 features exist.

## Current build configuration

- CMake 3.22 or newer; C++17.
- JUCE 8.0.6 source tree expected at `JUCE/` (the current GitHub Actions workflow clones this version).
- Windows: Visual Studio 2022 with the C++ workload, x64.
- macOS: Xcode/command-line tools; current workflow requests arm64 and x86_64 with a 10.15 deployment target.
- Root CMake defines VST3 and Standalone formats. The workflow's artifact upload currently uploads VST3 bundles only.

## Windows (Visual Studio 2022, x64)

From the repository root in PowerShell, clone JUCE if `JUCE/` is not present, then configure and build:

```powershell
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -A x64
cmake --build build --config Release
```

Verify both targets were produced under `build` (paths can vary by generator):

- VST3: typically `build/NebulaMultiFX_artefacts/Release/VST3/NEBULA.vst3`
- Standalone: typically `build/NebulaMultiFX_artefacts/Release/Standalone/NEBULA.exe`

A successful build command alone is not sufficient if either target was expected. Search the build tree and record the actual paths. The current Actions artifact upload is VST3-only; Standalone build output is not uploaded by that workflow.

## macOS (Xcode)

From the repository root, clone JUCE if `JUCE/` is not present, then configure and build both plugin formats:

```sh
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
cmake --build build --config Release
```

Verify the VST3 bundle and Standalone application exist under `build` (search for `*.vst3` and `NEBULA.app`). Record the actual paths and architectures. The current Actions artifact upload is VST3-only; Standalone is not uploaded.

## GitHub Actions (verified baseline)

- Latest successful Stage 0 run: [Build VST3 run 45029388928](https://github.com/Hunterice348/NEBULA/actions/runs/45029388928).
- Run metadata reports conclusion `success`, branch `feature/nebula-stage-0-build-fix`, commit `77c53a9d43e0c703842aede8258e33fe936908b0` (the base of `feature/nebula-v2` at Stage V2.0 start), and workflow `Build VST3`.
- The workflow matrix is Windows and macOS, with a VST3 artifact upload step on each runner. This verifies the CI build workflow reported success, not local build commands, artifact download/inspection, Standalone artifact upload/launch, host validation, DSP tests or this documentation commit.
- Earlier `main` runs failed at link time because the look-and-feel, reel and spectrum implementation files were not registered in CMake. The Stage 0 branch added those source registrations; the successful run is the evidence for that baseline fix.
- The current workflow push trigger lists `main` and `master`, not `feature/nebula-v2`. A push to this development branch alone is not expected to trigger CI. Dispatch/build this branch explicitly or update workflow scope in a separately reviewed change; Stage V2.0 does not change CI configuration.

## Stage V2.0 verification record

- [x] Repository tree, root CMake configuration and C++ source layout inspected.
- [x] Existing `BUILD_CHECKLIST.md` reviewed and useful configure/build instructions retained.
- [x] Stage 0 successful run metadata verified; see link above.
- [x] Documentation changes reviewed for explicit separation of current behavior from plans.
- [ ] Native configure/build executed in a local environment: not available through the repository tools used for this task.
- [ ] New CI run for the documentation commit: not executed; the current branch is outside the workflow's push trigger, and no workflow-dispatch capability is available in this task.
- [ ] Download/verify Windows and macOS artifacts and Standalone outputs: not performed.

## Follow-up verification for future code stages

For each code change, record branch/SHA, Actions run and job links, compiler/test results, artifact names/paths and platform coverage. Before release, build VST3 and Standalone on Windows and macOS, verify artifact contents, run automated DSP/state/graph tests, and perform plugin-host validation where available. Do not report a check as passed based only on a configured workflow or a past baseline run.
