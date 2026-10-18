# Demo Lab second stage — 2026-10-16, 18:43–19:13

## Status and safety

User approved this additional 30-minute stage. The installed engine remains
unchanged: data/engine/vf2.exe SHA256
71bf0221620dfc751da0d6c94935c50102a14e177392fe586b0fb7e7b36f41c0.
No software was downloaded or installed. All new rendering is restricted to
VF2_DEMO_COMMAND (the standalone Demo Lab), After mode, Smooth Scroll enabled.
Normal launcher behavior and installed saves are untouched.

**Spatial subtexel sampling has a demonstrated fix. Full-scene temporal
interpolation is NOT implemented; do not claim perfectly smooth 120/144 Hz.**

## Confirmed root cause and implemented extension

The native intermediate 1024x1024 render target at 0x82AB6D08 is format470
RGBA storage, but its values STILL encode character pens and palette metadata.
Shader 5B2584E683611239 emits these packed attributes. Shader
453B068FB94B6D0D and its quality/mode variants fetch them, FLOOR the packed
values, decode the palette coordinates, fetch tf3, then apply the native color
quality transform. Native index textures must remain POINT-filtered.

Changing a sampler without selecting texture unit541 was ineffective (the
active unit was544). Selecting541 and linearly filtering the packed target
changed pixels but interpolated indices, which is incorrect. That experiment
was removed, NOT shipped as a fix.

The After renderer now:
1. Uses the original palette/quality shader to decode two guarded, complete
   panorama banks at integer 1:1 positions into separate 1024x512 color targets
   (one for each native priority).
2. Resolves the actual colors, then uses the original generic texture blitter
   with LINEAR filtering for fractional camera translation and FOV/horizon.
3. Restores the original target, indexed texture binding, palette shader and
   arrays for the HUD/remaining original rendering.
4. Keeps top/bottom modal opaque palette swatches through that same original
   quality shader. Upper/lower color extensions remain separate.
5. Accounts for the generic blitter installing the UI projection; its input
   rectangle is mapped from bitmap coordinates into native UI canvas units.

This is spatial reconstruction after palette decoding, not a temporal lag filter.
Native simulation remains60 Hz in the explicit higher-rate clock experiment.

## Red/green feedback loop

Deterministic native demo pose: tick500, phaseX1845, phaseY126.5, UHD3840x2160,
FOV120, horizon0. Separate run changes phaseY by exactly0.125 source texel.

Run:
python tools/test_panorama_subtexel.py analysis/demo-decoded-before-y0 analysis/demo-decoded-before-y125

Old renderer: FAIL as expected, zero changed pixels (fraction discarded by
point sampling / packed palette decoding).

Run:
python tools/test_panorama_subtexel.py analysis/demo-color-ui-y0 analysis/demo-color-ui-y125

After: PASS, 317734 changed background pixels, ZERO changed foreground strip
pixels (including the character and HUD), max channel change43.
The test verifies matching frozen native tick, X phase, FOV and horizon.
Only complete CSV rows are used because the test process may stop mid-write.

Artifacts: analysis/demo-color-ui-comparison.json, paired captures under
analysis/demo-color-ui-y{0,125}/captures. Old and new screenshots were viewed;
3D geometry, character colors and HUD agree. Differences are confined to the
panorama sampling. Shader disassembly is in analysis/demo-shader-disassembly.

## Additional tests

- Build via native/build.cmd: passed, experimental Before/After EXEs refreshed.
- Standalone VF2 Demo Lab.exe rebuilt; Run/Restart and live Before/After remain.
- UHD demo movement at software120 and144 render targets: alive at end.
- FOV90/horizon+160 UHD: values reached renderer; lower extension uses ground
  color, not sky (capture reviewed).
- 1440x1080 4:3, FOV140/horizon-160: run completed and captures saved.
- FixedClock tests30..360 Hz, repeat callbacks, pause boundaries: PASS.
- Without GPU readbacks, measured software120 render118.30 FPS/native60.06;
  software144 render134.50 FPS/native60.02. These are observed rates, NOT proof
  of unique interpolated poses. Physical display stayed144 Hz, not120 Hz.
- Normal Demo Lab no longer automatically reads back60 UHD screenshots;
  capture overhead can disturb frame pacing. Explicit deterministic script
  captures remain available; -CaptureCount0 disables them.
- Existing native bank selection stays unchanged during repeated render frames
  with the same native tick. No evidence for a bank flip each render.

Not validated: full temporal interpolation, all arenas/quality combinations,
physical120 Hz display mode, perfect fast-demo motion. Gameplay regression tests
are still needed before enabling the color decode extension outside the lab.

## Full interpolation obstacle, now measured

Geometry is already transformed before the final variable-size draw buffer.
A narrow opt-in VF2_MESH_ID_TRACE probe at sub_82141F98 captures identities
BEFORE polygon clipping/compaction. During120 frames /12425 mesh instances:
- model/source geometry identifiers have5385 duplicate instances within frames;
- source geometry plus command slot has no duplicates, but ZERO matches across
  adjacent native frames (command positions shift).
Thus neither vertex index, model pointer alone, nor command slot is a valid
persistent object identity. Blending these would create geometric regressions.
Evidence: analysis/demo-mesh-identities-summary.json and CSV.

Next seam: native Model2 display-list object/call hierarchy before instance
matrices are flattened. Preserve persistent instance paths, capture paired
camera/model transforms and shading, then interpolate with the same render
alpha as the panorama. Preserve cuts and original60 Hz simulation. Do not add
background-only extrapolation, vertex-buffer-index blending, or low-pass lag.

## Files changed in this stage

- native/src/vf2_panorama.cpp — lab-only post-palette color decode/blit, subtexel
  probe, target/texture/shader restoration, native FOV/horizon/edge preservation.
- native/src/vf2_interpolation.cpp — opt-in pre-culling mesh identity probe only.
- native/src/demo_launcher.cpp — disable automatic GPU readback in timing runs.
- tools/test_demo_lab.ps1 — FOV/horizon controls and optional capture count.
- tools/test_panorama_subtexel.py — actual captured-output red/green regression.
- This report and analysis artifacts above.

The previous first-stage report remains useful for native clocks, demo startup,
ROM scroll capture and frame-buffer latch details; its speculative sampler
candidate is superseded by this stage's confirmed shader decoding evidence.
