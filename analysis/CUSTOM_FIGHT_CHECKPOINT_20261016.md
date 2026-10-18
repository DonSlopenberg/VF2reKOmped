# Custom Fight Launcher and panorama checkpoint

Work stopped at the user's explicit request. This is an incomplete candidate,
not an installed release. No further tests or analysis should resume without
the user's instruction.

## Saved artifacts

- Snapshot: `analysis/checkpoint-custom-fight-20261016-011056/` contains current
  native sources, CMake configuration, existing math tests, fight test scripts,
  compiled candidate and SHA-256 manifest.
- Candidate with its required runtime DLLs: `data/engine/vf2_fight_candidate.exe`.
- Latest build: `analysis/build-launcher-ui-input-check.log`.
- Original working version remains installed at `data/engine/vf2.exe`.
- Pre-change backup: `backups/before-custom-fight-20261015-164420/`.

Installed SHA-256:
`0119F8A144D1F3AF0AA2C50C25D4FDEEB281D144A34E198EB3C9D97270AEADA9`

Saved candidate SHA-256:
`D8DED4706F44B67F8A2FBB2436F4856020BD5CF2D78ACB628DDD838CC9862928`

## Implemented in the candidate

- F6 entry opens an English Custom Fight Launcher with independent P1/P2
  character and Human/CPU selection, all eleven native characters, sixteen
  native arena records, Start Fight and Restart Fight.
- Dural is native character 9, boss arena 10. Both boss and versus variants
  reuse original initialization and assets. Native alternate costume IDs were
  verified; Dural's alternate points to the same original model.
- Restart retains the configuration after a match ends. Reopening F6 allows
  a new configuration without restarting the process.
- Launches enter the original i960 mode dispatcher and reuse selection-resource,
  stage, match, round, model, collision, camera and control initialization.
- A bounded native CALL/RET bridge preserves interpreter context. Requests are
  queued across the UI/game thread boundary.
- Title-screen starts wait for original resource loading, close recognized
  frontend tasks through native callbacks and resume the original Model2 task.
- Direct starts activate the original HUD initializer, rebuild native names
  and bars, and expand final 3D/bitmap composition to the selected aspect.
- As explicitly chosen by the user, difficulty remains in the original game
  settings. No independent CPU difficulty is advertised or fabricated.
- Panorama extension samples the modal opaque color separately from the
  source texture's top and bottom rows and renders both through the original
  palette/quality shader. Edge fills are submitted once per background draw.
- Background phase guessing from repeated clouds was removed. Native scroll
  capture, completion publication and register-buffer metadata are implemented,
  but exact synchronization remains unresolved as described below.

## Observed tests

- Build completed successfully.
- Existing panorama math suite: 7,197,545 checks passed
  (`analysis/custom-fight-panorama-math.txt`). This does not prove live buffer
  synchronization or absence of flicker.
- `analysis/fight-test-verified-matrix/`: process survived 22 configurations,
  including all sixteen arena records, all eleven characters across the cases,
  Dural as P1/P2/both, Human/CPU combinations and restart. Native state checks
  matched requested character, stage and control IDs. GPU captures include
  correct Dural/Dural models and names. This was not every possible pairing.
- `analysis/fight-test-uhd-buffer-final/`: six configurations including restart
  survived; GPU readbacks are 3840 x 2160. Models, HUD and full-width background
  were inspected in preceding direct-start captures as well.
- Original menu/Arcade navigation and demo/gameplay runs survived at 1440 x 1080
  in `analysis/panorama-test-native-demo-4x3/` and
  `analysis/panorama-test-buffer-final-4x3/`. Elevated horizon testing exposed
  the lower extension; it uses ground/distant-bottom color rather than sky.
- `analysis/panorama-test-commit-4x3/`: latest completion-boundary candidate
  survived 45 seconds, but telemetry still shows the mismatch below.
- `analysis/fight-input-test/`: both synthetic XInput users received scripted
  direction/attack masks; native Human control mode 5 and bindings 0/1 were
  recorded. End-to-end gameplay response for both players remains to be
  conclusively verified. Do not infer failure solely from a post-tick input
  byte, which the original engine may already have consumed.
- F6 launcher widgets were compiled; interactive mouse/controller navigation
  of the new window has not been independently exercised.

## Remaining issues and exact continuation points

1. **Panorama timing is not finished.** In the latest commit trace, frame 300
   of arena 3 has scalar scroll -232 and captured full phase 236, while the
   raw scalar port is 280 (fine phase 232). Thus publishing only at native
   return 0x2467C did not remove the phase discrepancy. Do not add a guessed
   frame delay. Inspect original computed i960 g0/g2/g4 at 0x24664 and actual
   physical port mapping, then associate metadata with the original buffer.
2. Native update is 0x24534, completion 0x2467C, renderer register latch
   0x82188C08; read/write selectors are 0x833DD4D1/D0. Current diagnostics are
   `phase.csv.native.csv`. Original ROM evidence and decoder are under
   `analysis/custom-fight-rom/` and `analysis/custom-fight-cpu/`.
3. Existing custom panorama drawing bypasses native per-scanline water/wave
   modulation. Camera phase now handles scalar-disabled arenas, but restoration
   of that original modulation has not been implemented or verified.
4. Complete Human/Human response testing. Native control mode 5 is verified;
   host mode was measured as 3 and player mappings as 0/1. Therefore the
   conditional online-mode-2 input gate is not an established cause. Avoid
   changing frontend mode or resetting user bindings without evidence.
   Useful native telemetry: control task [0x50081C] flags and continuation,
   native input word 0x500700, active-low host ports 0x830B89E3/E4.
5. Verify new F6 widgets interactively, preserve normal Arcade after leaving a
   custom fight, and finish live timing/rotation/quality checks before replacing
   the installed executable. Audio has not been separately listened to or tested.

## Modified native files

New: `vf2_fight.h/.cpp`, `vf2_fight_host.cpp`, `vf2_i960.h/.cpp`.

Changed: `vf2_settings.h`, `vf2_video.cpp`, `vf2_panorama.h/.cpp`,
`vf2_test_input.cpp` (opt-in test support only), and `native/CMakeLists.txt`.

New test scripts: `tools/test_custom_fight.ps1`, `tools/test_fight_matrix.ps1`,
`tools/test_fight_input.ps1`.

Static reverse-engineering findings and runtime observations are separate:
the ROM disassembly explains initialization and IDs; the recorded process,
state and GPU tests establish only the configurations actually exercised.
