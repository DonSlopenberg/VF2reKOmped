# Virtua Fighter 2 — 16:9 stage and render resolution

Backup of the state before the changes: `backups/before-16x9-20261014-115451`.

## Confirmed in tests

- Original fight viewport: x=263, y=0, 1395×1080 in a 1920×1080 image. The proportions match the logical Model 2 screen of 496×384.
- Widening only the viewport stretched the models. Changing the orthographic matrix additionally squeezed the 2D layers; that change was reverted.
- The perspective builder `sub_821538A8` creates the 3D matrix and transposes it before returning. For a 496×384 perspective camera the first row of the matrix is multiplied by 0.7265625. The target fight viewport is 16:9, while the vertical scale and HUD coordinates stay original.
- The user confirmed correct proportions of the 3D characters and the HUD after this correction. The background still ended at the old image area.

## Implementation currently prepared

- The back bitmap (call to `sub_82144340` from return address 0x82144590) is repeated horizontally within its visible period. The HUD layer, called from 0x821445F8, is not repeated. Repetition is limited to bitmaps covering the full screen height, so the title screen is not repeated.
- The back bitmap offsets in `sub_821456B0` get a smoothing filter with an 18 ms time constant and 512-pixel wrap-around handling. The smoothing does not change pad input or game logic. The `vf2_smooth_background` cvar can be turned off in F6. The effect needs visual confirmation.
- F4 fits the window's client area to exact 16:9 and keeps the outer frame within 75% of the monitor's work area.
- The game separates the physical screen from the logical, authored canvas. The `sub_821067A0` hook updates only the physical record at 0x824A0498; the logical record at offset +32 keeps its original dimensions.

## UHD — verification state

Directly forcing a 3840×2160 console buffer produced a corrupted image. The log still showed a 1920×1080 viewport, and the screenshot showed duplicated and displaced fragments. That variant is not valid UHD.

The next variant uses GPU draw scaling, set before its render-target cache is created:

| Choice | Guest buffer | GPU draw scale | Target rendering |
|---|---|---|---|
| 1280×720 | 1280×720 | 1×1 | 1280×720 |
| 1920×1080 | 1920×1080 | 1×1 | 1920×1080 |
| 2560×1440 | 1280×720 | 2×2 | 2560×1440 |
| 3840×2160 | 1920×1080 | 2×2 | 3840×2160 |

The GPU module is loaded before `resolution_scale` and `draw_resolution_scale_x/y` are set, so these settings are already registered. The chosen mode, guest buffer, GPU scale values and the actual fight viewport parameters are logged. The change affects GPU rasterisation, not just enlarging the finished image through the window.

**Current status (14 October, 16:37):** the GPU 2× variant is installed and running on the real desktop in fullscreen. The title screen renders correctly, without the corruption of the direct 4K buffer. Log `runtime-video-uhd-scaled.log` confirms the selected 3840×2160 mode, a 1920×1080 guest buffer and registered 2×2 GPU settings. The fight itself, the tiling and the background smoothing are not yet confirmed in this variant. The whole UHD test must not be marked as finished.

The temporarily enabled `mnk_mode` keyboard mode was turned off in the settings without saving the configuration. A keyboard attempt did not open the menu. What remains is to enter Arcade with a pad and compare the fight viewport with the chosen GPU scale.

F4 tested in the same running UHD version: fullscreen → window with a 2640×1485 client (exact 16:9) → fullscreen. The log confirms a 2666×1556 outer frame fitting within 75% of the monitor, and both switches. The installed `Windows/vf2.exe` matches the build artefact; SHA256: `6C32634E516965DFB50478C3654EFF7374E120371C137D2F4C89C0C92A7371EA`.

Logs: `runtime-video-probe.log`, `runtime-video-wide-trial.log`, `runtime-video-perspective.log`, `runtime-video-uhd-tiled.log`. Build of the next variant: `build-video-uhd-smooth.log`.

## Layer assignment correction — 14 October, 17:04

The user confirmed the stability of the running UHD variant, but noticed duplication on character select and in the fight HUD. A runtime screenshot also showed extra health bars and names at the sides. The earlier assignment of caller 0x82144590 to the background alone was a wrong conclusion.

In the new build, caller 0x82144590 is completely excluded from the extra copies. The mirrored-widening attempt applies only to caller 0x821445F8, under the same full-height condition. There is no visual confirmation yet of what this second layer contains. The change does not modify the UHD settings or the perspective matrix.

Backup of the stable UHD version and sources before this correction: `backups/before-background-plane-fix-20261014-1703`. The new artefact built successfully (`build-background-plane-fix.log`). Replacing the file in Windows waits for the user to close the running game.

## Replacement after the confirmed game close — 17:13

The user closed the game. The new `Windows/vf2.exe` was replaced and its SHA256 matches the build artefact: `7F040DFEE4B76516C97FBF5E6CF0F176AA6C876A3899D2F256E1E9788E17BC96`.

The version with the layer correction runs at 3840×2160, fullscreen, XInput. Log `runtime-background-plane-fix.log` confirms a 1920×1080 buffer and a 2×2 GPU draw scale. A runtime observation showed a correct main menu. Verification of character select, the HUD and the mirrored arena background in this version is still in progress.

## Failed attempt and rollback — 17:16

A fresh fight screenshot and the user's reply confirmed that caller 0x821445F8 also contains text and HUD: swapping the caller did not separate the background. The earlier claims of an unambiguous layer assignment must be rejected. Duplicating the final bitmaps is not the right way to isolate the arena background alone.

The whole hooks `sub_82144340` (extra copies) and `sub_821456B0` (experimental filter) were removed from the source. The working UHD settings, the physical output record, the perspective correction and the viewport widening were kept. The rollback build succeeded, `build-background-rollback.log`. It waits for the game to close and for installation; after the rollback the background returns to the limited width of the original bitmap.

## Rollback installed — 17:22

After the game close was confirmed, the version without the extra bitmap copy hooks and scroll filtering was installed. SHA256 of `Windows/vf2.exe` and the build artefact: `AE9507632352EDC46AC161DBAE9FD87D9A701F5280C1066DB55078B24CB6BA2D`. It was started on the real desktop in fullscreen, UHD, XInput. Log `runtime-background-rollback.log` confirms the 3840×2160 choice, a 1920×1080 guest buffer, GPU 2×2. Widening the bitmap arena background itself remains unsolved.

The user authorised the agent to close and test the game on its own. The available UI tool sends keyboard and mouse, not physical pad buttons; this consent is not evidence that a pad test was carried out.

A runtime check after activating the window showed Shun's intro with single PUSH START BUTTON / FREE PLAY / ©SEGA 1994 captions, without extra bitmap copies. The old borders of the bitmap background are visible, as expected after rolling back its widening. The intro check is not yet a test of character select or a local fight in this particular version.
