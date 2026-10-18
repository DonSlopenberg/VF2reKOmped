# Virtua Fighter 2 — first-stage assessment

Date: 14 October 2026. Goal: a Windows 11 application in C++, local play on two pads, resolution choice up to UHD 3840 × 2160. The first stage had a 30-minute limit and did not include installing tools or a long build.

## Result

The package was extracted and is suitable for further analysis. No recompilation, Windows EXE or running game exists yet. The feasibility of a full port remains unconfirmed. The most promising candidate to try is ReXGlue, which combines C++ generation with an Xbox function layer. XenonRecomp alone does not provide a runtime.

## Local evidence

The observations below come from our own Python scripts and the bytes of the supplied copy. They are not REA evidence. The REA tool `open_binary` rejected the LIVE/STFS container and the extracted XEX as unsupported formats; no analysis session was opened.

- Source: `5841129C/000D0000/65D8FAC10B21FA185AAB93607663594CB568C78658`.
- LIVE/STFS format, 66,928,640 bytes, name in the metadata: Virtua Fighter 2, Title ID `5841129C`.
- Source SHA-256: `2e4700ca9bc3ea747379a1d3648b59bcdff6c528d1aefe241a51b47757c74099`.
- The SHA-1 of the top-level table, 96 lower-level tables and all 16,232 data blocks were checked. All match. This is a consistency check against the metadata, not an authentication of the Microsoft signature.
- 37 files and 8 directories were extracted, 66,417,270 bytes of files in total. Chains, lengths and block counts were verified.
- `default.xex`: 2,277,376 bytes; SHA-256 `00677891100a73636c9b1937c996c320b437eb61f11ce86848ddf6ee8a8e289f`.
- XEX2: original name `vf2.exe`, load address `0x82000000`, entry point `0x821AA770`, declared image size 21,692,416 bytes.
- XEX encryption type 1, compression type 2 (normal/LZX), LZX window 32,768 bytes. The image was neither decrypted nor decompressed in this stage.
- Import headers: `xam.xex` — 166 references; `xboxkrnl.exe` — 332 references. References are not the number of unique functions. The ordinals require access to the unpacked image.
- Static library metadata includes D3D9, D3DX9, XGRAPHC, XAUDIO2, XNET, XONLINE and runtime libraries. The original Xbox XDK must not be installed; we need a compatible implementation of the functions used.
- Assets: FARC archives, AWB/ACB/ADX audio, a `media/rom/vf2_rom` directory with the files `rom_data.bin`, `rom_pol.bin`, `rom_tex.bin`, `ic12_13.bin` and `ic12_15.bin`.

The ROM directory suggests an additional arcade-game playback layer inside the Xbox 360 port. This is a hypothesis based on the data layout, not a confirmation of an emulated processor or of the rendering method. Some assets have an undocumented format; their extensions alone are not enough to interpret them.

## Requirements and limits of the findings

| Area | State | Next verification |
| --- | --- | --- |
| Extraction of the copy | Done | Keep the manifest and SHA-256 |
| Native C++ on Windows 11 | Technical candidate, compatibility unknown | ReXGlue: load the XEX, flow analysis, C++ generation |
| Two pads, local versus | Requirement recorded, not implemented | Two independent input slots, player 2 joining, menu and fight; identify the pad models |
| Resolution up to UHD | Requirement recorded, not implemented | Separate window/output size, scene rendering and UI scaling |
| Sound | Assets present, behaviour unknown | Middleware support and actual playback |
| Game speed | Risk from an external report | Check correct speed when changing resolution and synchronisation |

The Xenia compatibility report for this Title ID describes working gameplay, 1080p support and game speed depending on FPS. These are another tester's observations on a specific Xenia build, not a test of our copy or a confirmation that ReXGlue works. Source: [game report](https://github.com/xenia-project/game-compatibility/issues/2202).

## Tools

Git and Python 3.14 are available on PATH. clang, clang-cl, CMake, Ninja, rexglue and XenonRecomp were not found. The checked typical paths for LLVM, CMake, the Visual Studio installer and the Windows SDK headers do not exist either. This is not a full inventory of all drives. The PATH result was saved to `toolchain_probe.json`.

For a first attempt I propose downloading the ready-made ReXGlue release for Windows x64 into a local tools directory, with the user's consent. The release page currently shows [v0.10.0](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0). The actual release files and the CLI behaviour must be checked before assuming the package allows standalone analysis.

For the later build the documentation names Visual Studio 2022 with C++ components, Clang 20 or newer, clang-cl/MSBuild support, CMake 3.25+ and Ninja. The Windows SDK and C++ libraries should come from the Visual Studio installer. Source: [Getting Started](https://github.com/rexglue/rexglue-sdk/wiki/Getting-Started). The scope and size of the installation must be checked before it starts. At this stage no need to supply a BIOS was found.

No tools were installed or downloaded. An attempt to download public source headers through Python failed because the command environment had no network access; the documentation was read through the web tool.

## Proposed second stage — requires consent

A 30-minute limit for downloading and checking the ready-made ReXGlue, loading the XEX and attempting to generate C++. Save the logs, the number of functions found, unresolved calls and missing imports. If the CLI requires building the SDK, an additional installation, or the task exceeds the limit, stop and present a concrete scope. This stage does not guarantee that the game runs. Full adaptation of graphics, sound, two pads and UHD will be a separate stage after the results are assessed.

## Reproducibility

`python tools/inspect_package.py` reproduces the extraction and `analysis/package_inventory.json`. The script refuses to overwrite a file with different content and rejects paths leaving the output directory.

`python tools/inspect_xex.py` reproduces `analysis/xex_headers.json`. Both scripts use only the Python standard library. The program fails on inconsistent bounds or an unsupported layout. STFS support is deliberately limited to the layout of this package.

The formats were read from [stfs_xbox.h](https://github.com/xenia-project/xenia/blob/master/src/xenia/vfs/devices/stfs_xbox.h), [stfs_container_device.cc](https://github.com/xenia-project/xenia/blob/master/src/xenia/vfs/devices/stfs_container_device.cc) and [xex2_info.h](https://github.com/xenia-project/xenia/blob/master/src/xenia/kernel/util/xex2_info.h). Recompilation information: [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), [ReXGlue](https://github.com/rexglue/rexglue-sdk) and [CLI](https://github.com/rexglue/rexglue-sdk/wiki/rexglue-CLI-Commands).
