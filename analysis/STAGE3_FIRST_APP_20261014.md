# Stage 3 — the first Windows application

14 October 2026. The user's consent covered installing the tools and a first build within a 60-minute limit. The user specified: the whole interface in English.

## Delivered result

The `Windows` folder contains a native start application `Virtua Fighter 2.exe`, the engine `vf2.exe` and three runtime libraries. The launcher has an English interface, a choice of 720p, 1080p, 1440p and UHD 3840 × 2160, a Fullscreen option and a count of Xbox controllers through Windows XInput. In windowed mode the window size is fitted to the desktop; the resolution reported to the game is passed separately. The game window is titled "Virtua Fighter 2". The assets remain in the neighbouring `extracted`.

This is a test version, not a fully verified release. The original package remains unchanged. Neither Java nor a BIOS was needed to build and run.

## Installation and build

- Visual Studio Build Tools 2022 17.14.42: C++ x64/x86 tools, CMake/Ninja and Windows SDK 22621. Installation finished; `vswhere` reported a complete installation without a restart. The installer log showed about 7.18 GB of disk usage change for the Build Tools operation.
- LLVM/Clang 20.1.8. SHA-256 of the downloaded installer: `3197846a2b19063687dd56e93e34cd941e3548d907f23a6131571321bdf9fe7b`, matching the official release metadata. The Microsoft installer had a valid signature.
- ReXGlue 0.10.0, C++23 application code, RelWithDebInfo configuration, Direct3D 12.
- The last full build and link finished with exit code 0. Commands: `native/build.cmd`, the GUI separately: `native/build.cmd launcher`.

## Function analysis

After the first run, the runtime pointed to additional unregistered indirect call targets. They were recorded in `native/vf2_analysis.toml` with references to the relevant logs. The Xenos module was added to the CMake configuration and the runtime.

A diagnostic dump `decoded-image.bin` was taken before guest code ran, through the optional `VF2_DUMP_IMAGE` hook. It does not change the game image. 199 additional entry candidates were found in the .rdata and initial .data sections. The first broad list revealed a problem: 14 entries were system imports and 139 pointed inside functions that were already translated. They were removed from the list of standalone entries based on import metadata and comparison with the baseline generation. 46 candidates were kept in `vf2_pointer_entries.toml`.

Final result: **13,249 function definitions**, zero `REX_FATAL` and zero `unresolved` text in all `vf2_recomp.*.cpp`. This is a check of the generated sources and the tool's validation, not proof that all execution paths are correct. Earlier intermediate files with errors are not the final result.

## Tests and user reports

| Area | Evidence / state |
| --- | --- |
| Native EXE start | Confirmed by process and log |
| Graphics | Xenos running on an NVIDIA RTX PRO 6000 through Direct3D 12 |
| Title screen | Visually confirmed through computer use |
| Two Xbox pads | The English launcher showed "2 connected" |
| Local Versus | The user confirmed two players working correctly in an earlier test |
| Local save | The user saw the runtime save-creation dialog |
| Arcade | The user reported a crash/restart; a test of the fixed version without replacements is in progress |
| Achievement | Log `runtime-final.log` confirms unlocking ID 6 |
| Resolutions | GUI choice implemented; the final launcher explicitly passes video_mode_width/height |
| Native UHD scene rendering | Not measured |
| Sound, rumble, longer stability | Not fully verified |

The restart a few seconds after the achievement coincided with my replacing the EXE and ending the test processes. It was not established that the achievement caused the crash. The user was informed. An uninterrupted Arcade retest on the fixed build was requested; from then on the game EXE was not replaced.

The logs still contain messages about the unimplemented `NtQueryInformationFile(XFileXctdCompressionInformation)`. So far they have not prevented startup or the confirmed Versus. No Windows report was obtained that would clearly explain the reported Arcade restart.

## Settings and limitations

ReXGlue defaults to `license_mask=0`. For the user-supplied copy the launcher passes `license_mask=1`; an earlier run with zero triggered the Xbox store stub. The language is explicitly set to English (`user_language=1`).

An SDK check showed that without explicit video_mode parameters the window size can take precedence over the `resolution` preset. The final launcher explicitly passes both video_mode parameters, so the chosen resolution is independent of fitting the window to the monitor. This configuration still needs verification of the actual size of the game's scene render buffer.

The current diagnostic process had the auxiliary keyboard enabled through `--mnk_mode`. The normal launcher selects XInput without enabling this option.

## Update after the user's uninterrupted test

The user completed two Arcade fights in the fixed build. They reported a hang after choosing Achievements from the main menu instead. The log at 11:22:00 local time shows `__imp__XamShowAchievementsUI STUB`: the SDK backend does not have this UI function. This is a separate, confirmed missing implementation. The unlock state exists in `Windows/userdata/achievements/5841129C.toml`; the new logs confirm IDs 1 and 2, an earlier log confirms ID 6.

New scope from the user: replace the blocking Achievements opening with a list of local unlocks, and adjust the scaling of runtime dialogs and pad navigation. Since this is bigger UI work, it requires another agreed time-boxed stage. This implementation was not started without asking about the time scope.

## Traces for continuation

- `stage3_summary.json`: package file identities and test state.
- `final_codegen_check.json`: check of the final generation.
- `build-final-title.log`: successful final build of the game EXE.
- `runtime-final-filtered.log`: uninterrupted current test of the fixed version.
- `pointer_candidates.json`, `interior_pointer_exclusions.json`: origin and exclusions of the candidates.
- `baseline_generated`: control translation without the broad candidates.
- `Windows/logs`, `Windows/userdata`: logs and user data of the normal launcher.

Implementation sources: [ReXGlue](https://github.com/rexglue/rexglue-sdk/tree/v0.10.0), [XInput backend](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/input/input_system.cpp), [video mode](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/kernel/xboxkrnl/xboxkrnl_video.cpp), [licence-mask runtime](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/kernel/xam/xam_content.cpp).

REA does not support the source STFS or XEX in this session. The code-analysis results come from ReXGlue and local scripts; UI observations from computer use and direct user replies. They were not given fictitious REA Evidence IDs.
