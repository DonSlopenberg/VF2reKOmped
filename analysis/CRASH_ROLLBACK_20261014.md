# Installed build rollback

The root-layout launcher incorrectly enabled `--vf2_arena_mirror_horizon`
and shipped the experimental source-space background renderer. The user
reported crashes during the intro/first arena and persistent bitmap faults.
The captured game.log contains normal window-close records, not a crash
exception identifying the root cause. Do not claim the exact fault is proven.

Preserved the affected engine, launcher, runtime log and source configuration
under `backups/before-crash-rollback-20261014/`.

Restored `data/engine/vf2.exe` byte-for-byte from the earlier stable build:
SHA256 AE9507632352EDC46AC161DBAE9FD87D9A701F5280C1066DB55078B24CB6BA2D.
Rebuilt the root launcher without the experiment flag. Default CMake sources
exclude vf2_arena.cpp; VF2_EXPERIMENTAL_ARENA is explicitly OFF by default.
The diagnostic source is retained for future work, not installed.

New directory layout, game assets, local saves, resolution selection, F4,
achievement UI and widescreen 3D/HUD remain preserved. Arena bitmap width is
back to the original limitation. Background work remains incomplete.

Started the exact restored engine with the new data paths and UHD selection;
runtime evidence is in `data/logs/rollback-uhd.log`.

Short UHD smoke test: visually observed the original SEGA intro and subsequently the first arena (Wolf/Lau, round 1 K.O.). Process 20860 remained responsive across repeated samples. A native overlay was visible in the arena capture. This is limited smoke-test evidence, not a full Arcade regression test. The original-width bitmap background limitation remains visible.
