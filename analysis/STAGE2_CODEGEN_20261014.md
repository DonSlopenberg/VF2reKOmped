# Stage 2 — C++ code generation

14 October 2026. The user approved downloading ReXGlue and a generation attempt within a 30-minute limit, and specified two Xbox pads with native Windows support.

## Result

ReXGlue v0.10.0 for Windows x64 was downloaded to `tools/rexglue-sdk-0.10.0`. The ZIP's SHA-256 matches the GitHub release metadata: `ad4485da853fbe932e9f5dbb643f9b5fe85dc73775ebbad4df6879fe6df0c11b`. No system-wide installation was performed.

The CLI correctly decrypted and decompressed the XEX for analysis and created the `native` project and sources. This is an observation of a successful read and instruction translation, not a test of running the game.

The first analysis reported six unresolved jumps. A one-off diagnostic `--force` allowed the sources to be inspected, but inserted six `REX_FATAL`; that result was not accepted as a valid port.

Six entry addresses were added in `native/vf2_analysis.toml`, with automatic boundary detection (size=0). No XEX bytes were changed and no instructions were skipped. After that, generation without `--force` finished with exit code 0 and passed validation.

Result: **13,198 function definitions**, 61 C++ translation files, 63,813,119 bytes of those files. A bounded search in all `vf2_recomp.*.cpp` found no `REX_FATAL` and no `unresolved` text. This is not proof that all instructions are correct, that indirect calls are complete, or that the runtime works.

Newly found entries that were checked:

| Address | Observation from the generated instructions |
| --- | --- |
| 0x82130C30 | 60 instructions, blr endings |
| 0x82444F38 | 6 instructions, indirect bctr jump |
| 0x82444F50 | 6 instructions, indirect bctr jump |
| 0x82444F68 | 6 instructions, indirect bctr jump |
| 0x82444F80 | 6 instructions, indirect bctr jump |
| 0x82444F98 | 6 instructions, indirect bctr jump |

The last five are short entries calling functions through a table. No semantic names were established for them. Summary and instruction lists: `codegen_summary.json`. Log of the successful analysis: `rexglue-codegen-seeded-console.log`. Import names in the generated code include `XamInputGetCapabilities`, `XamInputGetState`, `XamInputSetState`. 237 distinct `__imp__` names were found; whether the runtime implements them correctly remains to be checked when linking and running.

## Pads and UHD

The local CLI help confirms an `xinput` backend and resolution presets 720p/1080p/1440p/4k. The ReXGlue v0.10.0 code creates an XInput driver for this setting and applies `SlotAssignment` by default, assigning separate devices to separate game users. Source: [input_system.cpp](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/input/input_system.cpp).

A default of `xinput` was added in the application's `OnPreSetup` hook, keeping the user's setting from the configuration or CLI. This small piece of code has not been compiled yet. The pads were not checked on hardware. There is no resolution selection interface and no confirmation of UHD rendering yet.

## Further requirements

The missing compiler and Windows SDK block the first build. `native/build-tools.vsconfig` was prepared for the x64/x86 C++ components, CMake/Ninja and Windows SDK 22621. Plan: Visual Studio Build Tools 2022 plus a separate LLVM/Clang 20 or newer. Microsoft lists Clang 19.1.5 among the VS 2022 components, so choosing Clang in that installer alone may not meet the SDK requirements. Sources: [Microsoft](https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools?view=vs-2022), [ReXGlue](https://github.com/rexglue/rexglue-sdk/wiki/Getting-Started).

Installing these tools and the first build require separate consent. The actual installation size must be read from the installer. Proposed limit for the next stage: 60 minutes, with a pause to assess the results if bigger problems appear. There is no guarantee that gameplay will run in that time. No BIOS was needed to read and generate the code.

The original package and the extracted XEX were checked again by SHA-256 after the analysis. Their content was not changed. REA did not run a native session; the logs of this stage come from ReXGlue and local scripts.
