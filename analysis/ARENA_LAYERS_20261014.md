# Arena background and logo-skip stage — 14 October, 17:27

Stage limit: 30 minutes. Backup of the version confirmed free of UI errors: `backups/before-arena-layer-fix-20261014-1727`.

Measuring a single hardware layer (`VF2_LAYER_PROBE=1`) showed clouds/horizon in the intro without captions; character select contains the PLAYER SELECT graphics in the same layer, so the layer number is not enough to limit the mirroring. The function `sub_82145A90` renders both hardware layers into both final bitmaps, depending on the order before/after 3D. The earlier assignment of the final textures to the HUD alone or to the background alone was wrong.

Test variant (`VF2_ARENA_TRIAL=1`): both final bitmaps have a 684×384 buffer and a logical projection of -93.333…589.333. The HUD stays in its original 0…496 area. The mirrored pieces are created before composition, for hardware layer 1 only in scrolling mode; each piece is clipped to 0…496 before mirroring. The whole final bitmaps are not duplicated. Smoothing is prepared only for the layer 1 offset in the original scroll builders, keeping the fractional offset before the quads are emitted.

The shipping `Windows/vf2.exe` currently remains the version without experimental changes. Measurement variants run as `Windows/vf2_probe.exe`. UHD parameters: guest 1920×1080 / GPU 2×2.

## Separate horizon render targets — stage from 18:04

New hypothesis after the report of a static centre and sliding sides: the mirroring should apply to the finished horizon image, after the original shader runs and before composition with the HUD. This is not yet a confirmed cause of the earlier error.

The trial implementation creates two additional 496×384 render targets, keeping the original 0…496 projection. In `sub_82144050` it redirects hardware layer 1 in scrolling mode 1/2 to a separate target for the correct before/after-3D order. Before rendering layer 0 it restores the HUD target and clears it with alpha 0. After `sub_82145A90` finishes, the descriptors are always restored. During composition `sub_82144548` / `sub_821445B0` draws the horizon alone with the mirrored sides, then a single HUD.

Smoothing applies only to the source 1 offsets in the scroll builders. The wrap period matches the mode: in mode 1 vertical 1024/horizontal 512, in mode 2 vertical 512/horizontal 1024. The previous variant used 512 for both axes.

Build: `build-isolated-horizon.log`; UHD trial run: `runtime-isolated-horizon.log`. The main `Windows/vf2.exe` is still not replaced; the trial runs as `vf2_probe.exe` with `VF2_ARENA_TRIAL=1`.

## Rollback of the failed experiments

The user confirmed that the separate-target variant did not fix the intro background either. The horizon and smooth-scrolling fixes remain UNFINISHED. The main version was not replaced by the experiment. The measurement processes were closed and the kept Windows/vf2.exe was started (SHA256 AE9507632352EDC46AC161DBAE9FD87D9A701F5280C1066DB55078B24CB6BA2D). CMake again builds only main.cpp and vf2_video.cpp. The trial files remain for analysis but are not part of the main build. Logo skipping is neither implemented nor verified.
