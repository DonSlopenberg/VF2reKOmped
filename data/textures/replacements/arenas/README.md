# Arena background replacements

Upscaled copies of the PNGs in `data/textures/original/arenas/`, with the same
file names. Enable them in F6 → VF2 → Display → "Upscaled arena backgrounds".

- Any integer or fractional scale works as long as the aspect ratio matches
  the original (e.g. 2048x280 → 4096x560); max 8192 px per side.
- The image replaces the whole distant panorama and wraps horizontally.
- Loaded on the first fight in each arena; a missing or mismatched file falls
  back to the original (see data/logs/game.log, "VF2 replacement").
- The per-line water wave of the Akira/Jacky/Dural arenas is a render effect;
  it is applied to replacement images too (cvar vf2_background_replacement_wave).
