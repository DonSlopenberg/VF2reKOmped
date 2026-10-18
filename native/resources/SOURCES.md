# Launcher artwork

`logo_master.png` is the full-size Virtua Fighter 2 reKOmped logo supplied by
the project author. `tools/prepare_launcher_art.py` derives the other files
from it (standard library only, no redraw or upscaling):

- `logo.png`: the logo trimmed to its visible pixels and reduced to 1000 px
  wide (embedded in the launcher, drawn at its own aspect ratio);
- `vf2.ico`: the numeral 2 of the logo, 16, 24, 32, 48, 64, 128 and 256 px
  (PNG icon entries), used by both the launcher and the game.

`tools/optimize_png.py` writes the PNGs (best filter per row, zlib level 9,
transparent pixels cleared).
