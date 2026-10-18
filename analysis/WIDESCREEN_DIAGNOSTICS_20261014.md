# Short 16:9 image diagnostics — 2026-10-14

Scope approved by the user: a backup of the current state and a short diagnosis, without implementing changes.

## Backup before changes

`backups/before-16x9-20261014-115451`

165 files, 103,363,459 bytes. It contains the `native` project with the generated code, the application and library files from `Windows`, local saves, achievements, cache and logs. The intermediate build directory `native/out` was skipped; the finished executables are kept in the `Windows` copy. The original game assets and installed tools remain in their existing directories.

Checksums of all copied files: `manifest.sha256.json` in the backup directory. The SHA-256 of both executables and three application source files were checked against the originals after creating the backup and again after the diagnosis; they still matched.

## Findings

- The launcher passes separate `video_mode_width` and `video_mode_height`; all four available resolutions are 16:9. The log of the last test, `runtime-stage4c.log`, confirms a 1920×1080 game mode. The launcher's resolution choice alone does not explain the reported 4:3 view.
- The generated function `sub_82100D28` (`native/generated/default/vf2_recomp.43.cpp`) calls `XGetVideoMode` and initialises a 1920×1080 or 1280×720 screen. This is code analysis; it is not a measurement of the final viewport during a fight.
- The ReXGlue 0.10.0 runtime enables `present_letterbox` by default. The presenter keeps the declared aspect ratio of the guest image. Turning this option off can stretch the whole image, but does not adapt the projection or the position of HUD elements. Stretching a 4:3 image to 16:9 would widen objects relative to their height by 33⅓%.
- The current F4 independently sets 75% of the width and height of the monitor's work area, including the window frame. It does not guarantee an exact 16:9 client area, especially after subtracting the taskbar and window decorations.
- The interface sprites and animations have their own archives, `media/rom/sprite` and `media/rom/auth2d`. Their transformation and the HUD anchoring have not been established yet.

## Unexplained within the short scope

It has not been established whether the 4:3 fight view is baked into the rendered game image or arises when that image is presented. No fix for the 3D projection or the HUD layout was found or tested. No new gameplay test was run.

## Scope of a future fix

1. Fit the window's client area to exact 16:9 within 75% of the monitor.
2. Measure the viewport and the declared aspect ratio during a fight, to separate the game's projection from runtime scaling.
3. Adapt the projection and the HUD/sprite layout according to the cause found; check the proportions of characters, text, icons and framing in Arcade and Versus.

No sources, executables or game settings were changed in this stage.

Upstream sources: [ReXGlue 0.10.0 presenter](https://raw.githubusercontent.com/rexglue/rexglue-sdk/v0.10.0/src/ui/presenter.cpp), [runtime video modes](https://raw.githubusercontent.com/rexglue/rexglue-sdk/v0.10.0/src/kernel/xboxkrnl/xboxkrnl_video.cpp).
