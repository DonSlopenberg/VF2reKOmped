# Demo and gameplay panorama fixes

## Installed behavior

The background renderer now retains a validated arena mapping through camera tilts, filters full panorama phase instead of streaming-cache coordinates, and wraps horizontally only. Cylindrical horizontal projection is clipped before its tangent poles and tessellated into adjoining strips. The original camera scroll supplies the center ray; gameplay camera, arena geometry, HUD and foreground rendering are unchanged.

F6 now has an actual **Background FOV** slider at the top of the existing Settings window: **40–140 degrees**, default **120**. It changes distant scenery only and takes effect immediately. Reset restores 120; the SDK's existing **Save to config** action persists the value. Original settings categories remain available below the slider.

Each arena's modal opaque top-row sky color is sampled from its live character/palette data. A hidden offscreen atlas tile carries that exact source pixel into a full background fill through the original palette shader. This avoids the color mismatch caused by using an RGB clear alone and retains the existing quality/color processing. Nothing is written into the original asset files.

Backup: `backups/before-demo-panorama-20261015-150950/` contains the preceding executable, CMake configuration and native sources.

## Findings

The captured baseline contained **303 fallback frames in 5,991 scrolling-layer frames**. Scripted vertical camera movements moved all identifiable opaque panorama pixels outside the original visible 4:3 region, making the old matcher return to its fallback renderer. The fix validates retained mapping against the tile cache during those intervals rather than carrying an unvalidated arena into menus. Ordinary visible-frame matching remains strict.

Repeated cloud tiles could also admit multiple exact candidates; selection previously depended on hash iteration order. Continuous camera motion now prefers the coherent previous phase. Smoothing uses full panorama coordinates so an atlas rebase cannot masquerade as camera motion. Hard camera cuts remain hard cuts.

The previous vertical modulo operation repeated the skyline when the camera tilted outside a short panorama strip. Vertical bounds now clip the actual strip and expose the arena-specific sky fill. Only horizontal source coordinates repeat. Quad construction uses the original Xenos quad-list order TL, BL, BR, TR.

## Modified files

- `native/src/vf2_panorama.cpp`: coherent matching and retained-cache validation, full-phase smoothing, cylindrical strip projection, vertical clipping, sampled sky fill and opt-in trace data.
- `native/src/vf2_panorama_math.h`: cylinder projection and inverse calculations.
- `native/src/vf2_settings.h`: new slider integrated into the existing F6 window. The SDK's generic double-value control is an InputDouble; the new control is explicitly a SliderFloat.
- `native/src/vf2_app.h`: F6 dialog integration and opt-in settings render test.
- `native/tests/panorama_math_test.cpp`: projection monotonicity/inverse, FOV endpoints and streaming-rebase continuity checks.
- `tools/test_panorama.ps1`: selectable background FOV and settings-overlay test.
- `tools/verify_demo_panorama.py`: offline regression report.

## Build and tests

`cmd /c native\build.cmd` succeeded with the existing toolchain; no installations. Installed SHA-256: `e5dd5669a42ee944ad0621d2b9e87a3f3202ff5eb7bfe5dd258f4b2405d0d4ff`.

| Test | Result |
| --- | --- |
| Math tests | **7,028,616 checks passed**, including all FOV limits, monotonic source mapping, inverse projection, negative phase, wraps and cache rebases. |
| 135-second mapping regression | 6,780 bitmap frames; **zero scrolling-layer fallbacks** after the matching correction. |
| Final shader-fill build | 125 seconds at 1920×1080, intro, Arcade, KO/replay, continue/game over and return to intro; **5,390 scrolling frames, zero fallbacks**, process alive, 100 GPU captures. |
| 4:3, FOV 140 | 1440×1080 GPU readback, 65 seconds, process alive, zero fallbacks. |
| UHD, FOV 40 | **3840×2160 GPU readback**, 65 seconds, process alive, zero fallbacks; F6 slider draw confirmed in the runtime log. |
| Arena sky colors | Opaque top-row samples checked for all **11 distinct ROM panorama maps**. Live palette samples for the two tested arenas matched the ROM exports. |

Selected final captures were inspected for HUD duplication, background folding and sky-edge discontinuity, including `panorama-test-demo-swatch-final/captures/38.png` (upward KO camera) and `90.png` (Shun intro). The former black/color-mismatched sky band is absent in the inspected final KO capture.

Machine-readable details and executable hash comparison are in `analysis/demo-panorama-verification.json`. Tests are isolated from the user's save data.

## Coverage limits

Live tests cover the Shun introductory arena and the Lau Arcade arena, with original scripted camera moves and replay cuts. They do not exhaust every arena or quality-menu combination. All 11 distinct panorama maps were checked offline. The FOV-extreme 4:3/UHD runs precede the final palette-swatch fill adjustment, which was checked in the final 1080p run. Frame counts establish elimination of the measured fallback cause, not proof that every possible frame is artifact-free.
