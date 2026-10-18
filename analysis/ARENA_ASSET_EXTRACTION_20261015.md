# Original arena bitmap extraction

Delivered 11 unique PNG panoramas corresponding to 16 consecutive arena
records at program ROM offset 0x2502C, stride 32 bytes. Each record points to
the character bank list (+8), palette list (+12) and panorama descriptor (+16).
Source offsets and SHA256 hashes are retained in the exported manifest.

Data-ROM virtual base: 0x02000000. Character bank list entries contain a source
pointer and Model 2 destination, ending with a zero source. Each source begins
with a little-endian tile count followed by 32 bytes per 8x8 4bpp tile. Pixels
are read from 16-bit little-endian words in nibble order 12,8,4,0.

Palette list entries contain destination, color count and little-endian RGB555
colors, ending with a zero destination. Bit15 selects full brightness; otherwise
the original runtime halves the RGB result. Palette destination 0x01800000
corresponds to color index zero.

Panorama descriptors hold row count and a vertical parameter, followed by 18
strip pointers. Each strip contains 32 tiles per row. Repeated pointers encode
the horizontal wrap; only the minimal repeating period is exported. Most
panoramas contain eight unique 256-pixel strips. Tile word bits0..13 select the
character, bits7..14 select the 16-color palette. Pen0 is transparent.

Verification: all exported PNG pixels/dimensions match their manifest hashes.
For the day-cloud panorama, all 2576 decoded source tiles and all 496 source
palette colors exactly match an earlier native runtime sample. No new game
launch is required to regenerate the PNGs. See arena-extraction-verification.json.

Earlier camera-dependent snapshots were moved under analysis and are not the
delivered editable originals. No replacement loader is shipped in this stage.
The installed engine remains SHA256
8A47684D992114BAA002B0CA15CBD78D8E428B97CC8DDE6D3E52AAD244A2F51B.
