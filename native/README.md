# Virtua Fighter 2 — native project in progress

This is a working native C++ test build with a ReXGlue runtime. Compilation,
linking and the title screen have been verified. The user confirmed local
two-player Versus on an earlier test build and completed two Arcade fights
on the final filtered build. The game's Achievements menu currently blocks
in an unimplemented runtime UI call. See `../analysis/STAGE3_FIRST_APP_20261014.md`.

ReXGlue v0.10.0 successfully decoded the original XEX and passed static
validation after six additional function entry seeds in `vf2_analysis.toml`.
Additional entries were recovered from runtime failures and function-pointer
tables. Interior pointers and system import thunks were excluded; the final
generated sources contain no fatal or unresolved-call markers. These hints
do not patch or skip game code. Source XEX and assets stay in
`../extracted`. Generated code is private game-derived material.

## Reproduce generation

From the workspace root in PowerShell:

```powershell
& '.\tools\rexglue-sdk-0.10.0\win-amd64\bin\rexglue.exe' codegen 'native\vf2_manifest.toml'
```

Do not use `--force` for normal generation. The earlier forced diagnostic run
is documented in `analysis`; its unresolved functions were subsequently fixed
through entry discovery hints and regeneration without force.

## Compiler setup — installed

Visual Studio Build Tools 2022: x64/x86 C++ build tools, Windows SDK, CMake
and Ninja. `build-tools.vsconfig` records the selected component IDs. Also
LLVM/Clang 20.1.8 is installed; the VS 2022 bundled Clang listed by Microsoft
is 19.1.5, below the ReXGlue quickstart requirement. Check actual versions.

Use the x64 Visual Studio developer environment with LLVM on PATH. Configure
from this directory with the SDK prefix, then build (after user approval):

```powershell
$vf2SdkPrefix = (Resolve-Path '..\tools\rexglue-sdk-0.10.0\win-amd64').Path
cmake --preset win-amd64-relwithdebinfo "-DCMAKE_PREFIX_PATH=$vf2SdkPrefix"
cmake --build --preset win-amd64-relwithdebinfo --parallel 2
```

The build commands have passed. `build.cmd` reproduces the build with the
installed toolchain; `build.cmd launcher` builds only the settings executable.
Close a running EXE before replacing it. Runtime issues remain possible.

## Input and display targets

The scaffold selects `input_backend=xinput` when the user has not provided
an explicit override. The SDK uses `SlotAssignment` for separate physical
controllers. Both physical Xbox controllers were detected, and local Versus
was reported working by the user. Rumble and longer sessions are unverified.

The SDK accepts `--resolution 720p`, `1080p`, `1440p`, `4k` or dimensions.
It distinguishes guest video mode and window size. A 4K window alone is not
proof of 4K scene rendering. `src/launcher.cpp` provides an English native
resolution selector and passes explicit video-mode dimensions independently
of the desktop-fitted window dimensions. Actual UHD scene rendering is unmeasured.

The packaged test build is in `../Windows`. Run `Virtua Fighter 2.exe` there.
For manual diagnostics, a launch can pass the absolute extracted
asset directory with `--game_data_root`, `--input_backend xinput` and
`--resolution 1080p`. Confirm the actual output EXE location first.

Sources: [ReXGlue quickstart](https://github.com/rexglue/rexglue-sdk/wiki/Getting-Started),
[Microsoft component IDs](https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools?view=vs-2022).
