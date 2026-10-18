# Graphics assets

See `original/ASSETS.md` for the expanded graphics export, preserving original
paths below `original/media/`. This includes UI/font atlases, individual sprites,
all listed Model 2 tile banks and the decoded subset of 3D texture masks.

## Arena backgrounds: exported

`original/arenas/` contains 11 unique original arena panorama PNGs decoded
offline from the original ROM banks, covering 16 arena entries. See its
`README.md`, `index.png` and `manifest.json` for editing guidance and source mapping.

The exporter is `tools/extract_arena_bitmaps.py`. It reads source tile banks,
RGB555 palettes and complete panorama strip tables. It does not launch the game.
Original resolution and alpha are retained. Edited copies belong in
`replacements/arenas/`; loading replacements is a separate unfinished stage.

## Original package format findings

`original/system/` contains the 13 PNG images already stored separately in the
original game (achievement icons and the game image). They are editable now.
These are not the arena backgrounds.

All five files under `data/game/media/rom/vf2_rom/`, plus the `.farc` files
under `media/rom/`, start with bytes `0F F5 12 ED`. This identifies an Xbox XMem LZX TD
compression wrapper. Decoded ROM copies are retained in `analysis/bitmap-captures/`.
The `.farc` extension alone does not establish their inner layout.
`media/x360/SYSTEM.farc` is an exception: its observed header is `FArc`,
an archive signature without the same outer XMem wrapper.

Reference identifying this signature:
https://github.com/LittleBigBug/QuickBMS/blob/master/readme.md

The names `rom_tex.bin` and `rom_data.bin` suggest texture and game-data stores;
the arena tile banks, palettes and panorama maps have now been located in
the decoded data ROM. Their byte offsets are recorded in the arena manifest.
Native rendering evidence shows indexed/tiled Model 2 graphics assembled into
512/1024-pixel background planes. A useful background editor workflow must
reconstruct those planes with their palettes rather than merely export an atlas.

`replacements/` is reserved only. Copying PNG files there currently does nothing.
The future loader must define image naming, dimensions, alpha, scroll origin,
16:9 coverage and fallback behavior before upscaled backgrounds can be used.
No replacement files are loaded by this build.

`asset_inventory.csv` and `.json` record each original file's path, header,
size and SHA-256 hash. Regenerate them with `python tools/inventory_assets.py`
from the game root. This tool copies existing PNGs only; it performs no decoding.

No extraction software was installed for this stage.
