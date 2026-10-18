# Demo Lab checkpoint — first 30-minute stage

## Status: requested rendering fix is NOT complete

The standalone `VF2 Demo Lab.exe` is built in the game root. It directly enters
the original Arcade title/attract demo using the verified native mode dispatch
(`0xA6F8`, mode 2 -> `0xAB0C` -> original script initialization `0x43FD0`).
It bypasses frontend menus after required one-time resource/profile startup.
Restart Demo resets that original demo in the same process. Before / After
switches are also supported through the same-process command bridge.
The lab uses a separate test profile and logs under `analysis/demo-lab/`.
Physical controller input is suppressed only in this explicit diagnostic mode.

The installed main engine `data/engine/vf2.exe` has NOT been replaced.
Its SHA-256 and the pre-stage backup are both
`71bf0221620dfc751da0d6c94935c50102a14e177392fe586b0fb7e7b36f41c0`.
Backup: `backups/before-demo-interpolation-20261016-180954/`.

## What the measurements actually establish

- Original demo start/restart succeeded in one process; native initialization
  logs contain successive invocation numbers. The standalone EXE smoke test
  also started the engine and restarted the demo while its child stayed alive.
- `demo-lab-before` selected 1,755 rendered scroll samples without a mismatch
  between the selected native-buffer phase and the cached pose. This rejects
  the initial same-tick cache-mismatch hypothesis for this sequence.
- Merely increasing outer refresh accelerates native simulation. A prototype
  fixed simulation clock now uses elapsed time, not "every second frame";
  the latter slows gameplay when rendering misses its requested refresh.
- `demo-lab-clock-wall120` measured approximately **118.694 rendered frames/s**
  and **59.991 simulation updates/s** during a continuous demo segment. The
  physical monitor remained 144 Hz. This is a software 120 Hz render test,
  not proof of physical 120 Hz output or 120 unique interpolated poses.
- `tools/test_render_clock.cmd` passes simulation-clock tests at 30, 60, 90,
  120, 144, 240 and 360 Hz, repeated callbacks and pause boundaries.

## Rejected / unproven explanation

Runtime texture inspection shows the bitmap source at `engine+0x118C00` is
1024x1024 format 447, with separate map (128x128, format 452) and palette
(2048x2, format 470). The earlier comment identifying it as a decoded RGBA
atlas was incorrect. Linear interpolation of palette indices is inappropriate.
The candidate removes that override, and the lab can reproduce it explicitly.

However, the controlled GPU comparison at **the exact same frozen native tick
500, phase X 1845, phase Y 126.5, FOV 120, horizon 0, UHD** produced **zero
different pixels**. Therefore removing that override is NOT a demonstrated
fix for the user's visible jitter. The shader may enforce its own sampling;
the removal must not be advertised as a visually validated root-cause fix.
Evidence: `analysis/demo-lab-palette-comparison.json` and the two
`analysis/demo-lab-same-pose-*/captures/0.png` readbacks.

## Remaining work

There is no validated full-scene interpolation yet. Geometry arrives at the
renderer as already transformed vertices; the observed per-draw matrices are
mostly the common projection, not individual character/camera poses. Vertex
counts and visible polygon ordering change as the camera moves. Blending
buffers by index would connect different polygons and create distortions.

Identify stable geometry/animation identities upstream of visibility sorting,
capture paired native poses and shading, and render interpolated state with
the same time parameter as the panorama. Treat camera cuts as cuts. Inspect
the actual palette-decoding shader and test decoded-color interpolation,
instead of relying on sampler changes or adding a lag filter.

The experimental higher-rate clock is opt-in through `VF2_DEMO_RENDER_HZ` and
is NOT enabled by the normal launcher or the Demo Lab's default Run button.
Before/After currently evaluates the palette-sampler change above; it does not
claim to compare a completed full-interpolation implementation.

## Principal modified files

- `native/src/demo_launcher.cpp`, `native/src/vf2_demo.{h,cpp}`: launcher/bridge.
- `native/src/vf2_fight_host.cpp`, `native/src/vf2_fight.cpp`: opt-in native demo
  entry/restart and experimental fixed-step worker integration.
- `native/src/vf2_render_clock.h`, `native/src/vf2_interpolation.{h,cpp}`:
  clock prototype and opt-in geometry/source probes; interpolation unfinished.
- `native/src/vf2_panorama.cpp`: synchronized buffer traces, format probe and
  candidate sampler correction with explicit lab-only Before reproduction.
- `native/src/vf2_app.h`, `native/src/vf2_test_input.cpp`, `native/CMakeLists.txt`.
- `tools/test_demo_lab.ps1`, `tools/test_render_clock.cmd`,
  `native/tests/render_clock_test.cpp`.

No programs were downloaded or installed. Continuation beyond this first stage
requires the additional time approval already requested in the chat.
