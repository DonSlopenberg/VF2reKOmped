# VF2 panoramic arena renderer — 15 October 2026

## Delivered behavior

The default build extends the original background renderer across the selected viewport. It reconstructs the complete ROM panorama instead of repeating or mirroring the camera-dependent 4:3 cache. Existing character rendering, gameplay camera, palette shaders, priority layers and background filtering remain in use. HUD and static screens retain their authored horizontal clipping. Smooth scrolling remains available through the existing setting.

The built executable is installed at `data/engine/vf2.exe`; the root launcher is unchanged. Original game assets are unchanged. The previous executable and native sources are backed up in `backups/before-panorama-renderer-20261015-093718/`.

## Original implementation and correction

- `sub_82145A90` composes the four tile banks and separates background layer 1 from HUD layer 0. `sub_82143DF0` and `sub_82143F20` populate the existing GPU atlas. Layer modes arrange the background banks either vertically or horizontally.
- `sub_821888F0` and `sub_821889E8` provide the engine's signed horizontal and vertical scroll. These remain the source of camera synchronization: no estimated yaw multiplier or replacement gameplay camera is introduced.
- Full background descriptors reside in the decoded ROM data. A typical panorama comprises eight 256-pixel strips. Runtime cache matching checks visible tiles against the complete map and rejects ambiguous matches. Eleven distinct panorama maps were identified.
- The renderer fills the original two background banks with a contiguous window from the complete map. Horizontal sampling wraps across the complete panorama; the original vertical period and alignment are preserved. The window width derives from the actual original/corrected projection coefficients and the selected aspect ratio. Background quads bypass only the original 512-pixel CPU rejection limit; GPU clipping handles the expanded target.
- Palette and character-texture decoding, filtering and foreground/HUD compositing still use the original routines. Unrecognized/static screens fall back to the original bitmap path.
- GPU upload ordering matters: the remapped map stays alive until queued atlas draws consume it. CPU state is restored after the pass, and GPU state is restored before a fallback frame. Immediate GPU restoration caused the missing right-hand bank and has been removed.
- The display's logical canvas now follows the selected aspect ratio, fixing the separate 4:3 output problem. HUD/logo quads retain the original 0–496 horizontal clip, preventing stale offscreen tiles from becoming visible.

## Modified implementation files

- `native/src/vf2_panorama.cpp` — complete-map identification, original atlas extension, wrapping, clipping and compositing hooks.
- `native/src/vf2_panorama.h`, `native/src/vf2_panorama_math.h` — renderer interface and coordinate calculations.
- `native/src/vf2_background_scroll.cpp`, `native/src/vf2_arena_scroll.inc` — integrate the new path with the original scroll hooks.
- `native/src/vf2_video.cpp` — aspect-dependent projection, logical display canvas and final viewport.
- `native/CMakeLists.txt` — enable the renderer in the default build; legacy experimental renderer remains excluded.
- `native/src/vf2_app.h`, `native/src/vf2_test_input.cpp` — opt-in GPU readback and scripted controller input, inactive during normal launches.

Reproducible checks: `native/tests/panorama_math_test.cpp`, `analysis/test_panorama_math.cmd`, `analysis/verify_panorama_mapping.py`, `tools/test_panorama.ps1`, `tools/collect_panorama_captures.py`, `tools/verify_panorama_results.py`.

## Build and validation

`cmd /c native\build.cmd` succeeded using the existing toolchain, with no new installations.

| Check | Result |
| --- | --- |
| Coordinate/wrap math | 6,777,737 checks passed, including negative fractional scroll, full horizontal wraps, vertical periods and aspect-dependent extents. |
| Retained runtime tilemaps | 1,005,768 offline checks across 92 captures and 11 ROM panorama maps. |
| 4:3 GPU output | 1440×1080 confirmed by direct GPU readback; 80 frames checked for full-height black gaps/borders. Smoothing disabled for this run. |
| UHD 16:9 GPU output | 3840×2160 confirmed by direct GPU readback; 80 frames checked for full-height black gaps/borders. Smoothing enabled. |
| Panorama wrap coverage | Both aspect-ratio runs traversed all 256 source-angle bins, crossing the panorama boundary repeatedly. |
| Final normal-camera regression | 130 seconds at 1920×1080, surviving Arcade, KO/replay, game over and return to intro. Final captures show a single HUD, continuous arena background and clean logo clipping. |
| Data integrity | All 37 original game files checked against the existing asset inventory. |

The 4:3/UHD sweep runs precede the final HUD-only overscan clipping adjustment. That adjustment was checked in the final 1080p normal-camera regression; it does not change panorama sampling. The final executable hashes and machine-readable results are recorded in `analysis/panorama-render-verification.json`.

Evidence is retained under `analysis/panorama-test-4x3-wrap/`, `analysis/panorama-test-uhd-wrap/`, and `analysis/panorama-test-final-hud-clip/`. Direct GPU captures avoid confusing desktop window size with actual render dimensions.

## Coverage limits

Full panorama-angle coverage uses an explicit diagnostic source-angle sweep; it is not a forced full rotation of the gameplay camera. Normal camera motion was tested separately. Live tests cover the introductory arena and Lau; remaining panorama maps were checked offline. Every quality-menu combination has not been exercised live. Automated black-column checks detect gross gaps, while seam and HUD inspection also relied on selected GPU captures; they are not a proof that every possible frame is artifact-free.

## Rollback

With the game closed, restore `backups/before-panorama-renderer-20261015-093718/vf2.exe` to `data/engine/vf2.exe`. The backup also contains the pre-change native sources.
