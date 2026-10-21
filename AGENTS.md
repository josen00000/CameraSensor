# Agent guide (StallFreeCapture / CaptureSandbox)

Standing rules for Cursor agents working in this repo. Task-specific notes belong in the PR, not here.

## Ownership

Josen owns the core Unreal and concurrency code: the queue/ring (MPSC), threading, and the capture pipeline.

Agents must **not** write, rewrite, or “fix” that code. If you see a problem there, describe it in the PR and stop.

Agents handle setup, glue, third-party libraries, the seeded test scene, CI, tooling, and docs. Do not touch Unreal plugin or game code unless the task explicitly requires it.

## Repository layout

- **CaptureSandbox** — UE 5.8 host project (`CaptureSandbox.uproject`, `Source/`, `Content/`, `Config/`).
- **Plugins/StallFreeCapture** — StallFreeCapture plugin (`StallFreeCapture.uplugin`, `Source/StallFreeCapture/`).
- **Plugins/StallFreeCapture/ThirdParty/sfc_core** — plain C++20 library (CMake + GoogleTest). Public headers live under `include/sfc/`.

## Build and test

### sfc_core (CMake)

Presets are defined in `Plugins/StallFreeCapture/ThirdParty/sfc_core/CMakePresets.json`: `debug`, `release`, and `tsan` (the `tsan` preset is disabled on Windows).

From `Plugins/StallFreeCapture/ThirdParty/sfc_core`:

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug --output-on-failure
```

Use `release` or `tsan` in place of `debug` for those presets. Test presets treat “no tests” as an error (`noTestsAction: error`).

### Smoke script (`tools/run-tests.ps1`)

Requires PowerShell 7. From the repo root:

- Default: configure, build, and run **sfc_core** tests for a CMake preset (default `debug`).
  - `./tools/run-tests.ps1`
  - `./tools/run-tests.ps1 -Preset release -Clean` — delete `sfc_core/build` before configure.
- **Unreal** automation instead of sfc_core:
  - `./tools/run-tests.ps1 -Unreal` — runs `StallFreeCapture.Version` via `UnrealEditor-Cmd` on `CaptureSandbox.uproject` (`-nullrhi`, unattended). Needs UE 5.8 on PATH or `UNREAL_ENGINE` set.

The script writes local reports under `docs/reports/` (gitignored). Do not commit those files.

### Unreal Engine build

`Plugins/StallFreeCapture/Source/StallFreeCapture/StallFreeCapture.Build.cs` links `sfc_core`. If the static library is missing, it runs:

`cmake --preset <debug|release> && cmake --build --preset <debug|release>`

in `ThirdParty/sfc_core` (`debug` for UE Debug, `release` otherwise), then links `libsfc_core.a` (Linux) or `sfc_core.lib` (Win64).

## CI

Workflow: `.github/workflows/core.yml` (`sfc_core CI`).

- Matrix: **ubuntu-latest** with GCC and Clang, **windows-latest** with MSVC.
- Steps (in `Plugins/StallFreeCapture/ThirdParty/sfc_core`): `cmake --preset debug`, `cmake --build --preset debug`, `ctest --preset debug --output-on-failure`.

Path filters (push and pull_request): only changes under

- `Plugins/StallFreeCapture/ThirdParty/sfc_core/**`
- `tools/**`
- `docs/**`
- `.github/**`

trigger the workflow. Edits elsewhere (for example root `AGENTS.md` or `README.md` alone) do **not** run CI.

## Code rules

- **sfc_core public API** must be exception-free (UE includes these headers with exceptions off). Prefer factories returning `std::optional` or `bool` instead of throwing constructors. See `Plugins/StallFreeCapture/ThirdParty/sfc_core/README.md`.
- **Warnings are errors**: `-Wall -Wextra -Werror` on Clang/GCC, `/W4 /WX` on MSVC (`CMakeLists.txt`).
- **Blueprint exposure**: do not use `BlueprintReadOnly` on unsigned 64-bit fields; use `int64` instead.
- Prefer the smallest change that completes the task; keep code simple.

## Test output

Never commit test output, logs, or result files to the repo; keep result folders in .gitignore. Put the summary of a real run (the command, pass/fail counts, tests run) in the PR description only, copied from actual command output. Never write or edit results by hand. A run that reports zero tests is a failure, not a pass.

## Pull requests

- One task card per PR.
- Include a **Test run** section with real command output (see rule above).
- List **Unverified** steps you did not run (for example UE editor, PIE, or Windows-only checks Josen runs locally).
- Agents do **not** merge; Josen merges.
- When updating a branch that is already pushed: merge `main` with a merge commit as needed. Do not rebase or force-push a pushed branch.

## Git LFS

`.gitattributes` stores these extensions with Git LFS (`filter=lfs diff=lfs merge=lfs -text`): `.blend`, `.abc`, `.jpg`, `.bmp`, `.psd`, `.wav`, `.umap`, `.png`, `.hdr`, `.psb`, `.ogg`, `.flac`, `.mov`, `.uasset`, `.fbx`, `.tga`, `.tif`, `.tiff`, `.exr`, `.xcf`, `.mp3`, `.dae`, `.jpeg`, `.mp4`, `.avi`.

There is no `LICENSE` file at the repository root. Third-party notices for sfc_core are under `Plugins/StallFreeCapture/ThirdParty/sfc_core/` (`NOTICE`, `vendor/README.md`).
