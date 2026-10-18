# Isolated background scrolling restoration

User confirmed the rollback works and requested smooth background scrolling.
The confirmed engine is preserved at `backups/before-smooth-scroll-20261014/`.

Default builds now use `vf2_background_scroll.cpp`, independently of the
experimental arena renderer. Original scroll builders retain their draw calls;
only hardware layer 1 scroll coordinates in layouts 1/2 are filtered. The
filter uses the correct 512/1024 wrap periods, one update per bitmap frame,
a 12 ms time constant, and resets on large jumps, frame gaps and layout changes.
No allocation, mirroring, extra geometry or final composition hooks are enabled.

Validation: native build passed. `analysis/test_scroll_filter.cpp` passed
wrap continuity, repeated priority pass, monotonic movement, scene jumps,
layout changes, pauses and bounded lag checks. Runtime UHD log confirms
3840x2160 selection and 2x2 drawing. It records active fractional filtering:
input247 -> 246.49075166109878. Visually checked intro, intact character
selection and the Akira/Lau first arena. The background retains its original
width. This does not claim the unfinished widescreen bitmap work is fixed.

Runtime log: `data/logs/smooth-scroll-uhd.log`.
