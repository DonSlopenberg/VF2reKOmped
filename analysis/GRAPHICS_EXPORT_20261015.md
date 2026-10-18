# Expanded offline graphics export

Output root: `data/textures/original/`. Paths below `data/game/` are preserved.
Archives become same-name directories, retaining nested archive names.

Exported families:

- 12 FARC archives decoded offline through the already bundled LZX routine.
- 29 original DDS atlases decoded losslessly into RGBA PNG; original DDS retained.
- 162 individual sprite crops from 48-byte records; bounds and normalized UV
  coordinates independently checked before extraction.
- 13 original loose PNGs copied with original paths and byte hashes.
- 110 original Model 2 tile-bank sheets in 18 load sets, with exact 0–15 index
  sheets, palettes, source addresses and VRAM layouts.
- 45 pointer-referenced uncompressed Model 2 luminance textures (encoding1),
  base mip levels. 1481 encoding0 blocks remain unresolved and are explicitly
  listed in their manifest; the 3D family is not a complete extraction.
- Previously exported 11 complete arena panoramas retained.

ROM-based image extraction uses retained, decoded original ROM banks. Attempts
to regenerate the entire data-ROM bank directly through SDK LZX exposed a
multi-segment alignment edge case; the archive-only FARC pipeline is complete,
while ROM exporters explicitly use the existing full decoded bank. No game was
launched during this stage, no software was installed, and no images were upscaled.

Tool entry point: `tools/export_graphics_assets.py`, with fresh --output directory.
Meaningful verification is recorded in `graphics-export-verification.json`:
every exported PNG reads successfully; exact sprite crop pixels match source
atlases; tile sheet hashes/dimensions and original index ranges are checked;
the fresh complete export regenerates 519 identical PNG files including previews;
all 37 original game files and the installed engine retain original hashes.

Replacement loading remains separate and unimplemented. No binaries, graphics
settings, controller code or rendering behavior were deployed by this stage.

A standalone compressed-3D codec prototype is retained as
`analysis/model2_codec_harness.cpp` and `.exe`. It calls the existing generated
codec without launching the game. A decoded guest-memory dump is retained as
`analysis/model2-decoded-test.raw`; the final pixel buffer and output layout
are not yet verified, so this probe is not published as an editable image.
The approved 30-minute stage ends here; further codec work needs renewed approval.


DDS cleanup: removed 29 verified PNG atlas duplicates from the delivered export
and 29 from the verification copy. Original DDS files remain unchanged. The
exporter and sprite verification now read DDS directly, without PNG conversion.
