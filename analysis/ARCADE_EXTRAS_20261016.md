# Arcade extras — 2026-10-16 (Claude)

Implemented in `native/src/vf2_arcade.cpp`; F6 → VF2 → "Arcade & Cheats".
Agreed spec: random opponents (Dural last, not in Ranking Mode, no
achievements), random arenas (0..9 only, no repeats, Dural keeps 10, not in
Ranking Mode), Tourist difficulty (constant CPU), Infinite Health P1/P2
(achievements off until restart). Options persist via `vf2.toml` cvars.

## Native facts used (decoded ROM, own i960 decoder; REA has no i960 provider)

| Fact | Evidence |
| --- | --- |
| Arcade stage counter RAM 0x500054 (1-based) | 0xCE5C/0xCF64 |
| Opponent of stage s = ROM[0x12509+(s-1)*2], arena = ROM[0x12508+s*2] | 0xCE68, 0xCF6C; table 4,2,8,6,10,5,3,1,7,0,9 / arenas 0..10 |
| Ranking Mode = 0x500088 bit 9 | RANK_INT 0x10BC0 |
| Fighter health u16 at fighter +0x1AC, round max at 0x5000A0 | AI 0x3D678; FIGHT init 0xD960 |
| var_diff 0x50D005 = ROM[0x11510 + diff*8 + working_rate]; Easy row all 0 | 0x112F8..0x113BC |
| CPU level = base[char] (0x4356C) + (task+0x72 >> 5), clamped by 0x4353C; stored at 0x3DBC4 | 0x3DB20..0x3DBC8 |
| AI block 0x3D06C..0x43ABC never reads the stage counter | full-ROM grep of 0x500054 |
| XUserWriteAchievements wrapper = sub_821A8520 (XGI 0x000B0008, {count,array}) | generated code |

## Behaviour

- Line-up: on the first progression-table read of a run (PC 0xCE68/0xCF6C) a
  permutation is written into the in-memory ROM copy and locked until a
  frontend scene, so Continue keeps it. Ranking Mode restores the original.
  Logged as "VF2 arcade line-up (opponent@arena)".
- Tourist: at 0x113B4 difficulty/rate = 0 (Easy, no var_diff growth), at
  0x1140C override base 0, at 0x3DBC4 constant CPU level
  `vf2_tourist_cpu_level` (default −32, native minimum) for every opponent.
- Cheats: each host frame the selected fighters are set to 0x5000A0.
- Achievement gate: count forced to 0 while cheats were used this session or
  a random-opponent run is active.

## Verified

- Random run: log `7@4 8@6 2@1 …`; first fight on arena 4 with HUD "WOLF" (7).
- Infinite Health P1: 94 samples, P1 never below 160/160.
- Gate: "achievements blocked (1 requested)" → kernel "skipped (count=0)".
- Tourist: constant by construction (both escalation inputs pinned); no
  gameplay-strength measurement claimed.

## Not done: native ARCADE menu rows

TaskMenuArcade (vtable 0x8204B978: init 0x82147C08, update 0x82147DD0,
left/right 0x82148040 / 0x82148130, draw 0x82148FF0) hard-codes 5 rows:
`ctr=5` loops, `cmpwi 5` checks, per-row arrays at task +452..+468 (values,
from settings struct sub_821230B8 +8..+24), +605..+609, per-row maxima at
0x8204B8DC, list widget at +612. Two extra rows require relocating these
arrays. Tourist inside the Difficulty row (wider value range + label hook)
is smaller. Proposed as a separate task.

## Fixes 2026-10-17 (user report)

- **Tourist was very hard**: the native level is a handicap. 0x3DBF0 derives
  action parameters as 0x52 − 16·level (clamped 0..255); Akira/Dural have the
  lowest base levels. −32 therefore meant maximum strength. Default is now
  +16 (native maximum handicap); higher = weaker.
- **Save indicator stuck in the main menu**: the gate passed count=0, and the
  runtime's "skipped (count=0)" path does not complete the request, so the
  game waited forever. The gate now keeps the count and substitutes ids that
  match no achievement (0xFFFFFFFF), restoring the game's buffer afterwards.
  Verified: 3 blocked writes in one run, each "id=4294967295", nothing
  unlocked, game continued to stage 3.

## Tourist: every CPU uses Lau's AI tables (2026-10-17)

The AI never reads the stage; the Arcade ramp is the opponent order with
per-character tuning. Tourist now replaces the character index register at
four i960 PCs with Lau (id 4, first Arcade opponent): 0x3D274 (AI parameter
row 0x3BD54 + char*128 + style*32, style kept), 0x3D34C (base level
0x4356C), 0x3DB68 (reduction 0x4355C), 0x3DB80 (limits 0x4353C), plus the
constant level 16 at 0x3DBC4. Move/decision tables keyed by the real
character are untouched, so each fighter keeps its own moves.

## Arcade end returns to Main Menu (2026-10-17)

ROM main mode (0x50002A): select 7 → match 9 → game over/ending 10/11 →
attract 3. The frontend stayed in gameplay scene 1 and ran the attract demo.
HostFrame now remembers a mode-9 run (scene 1, not a custom fight) and, when
attract mode 3 starts, performs the native Exit reset (sub_82183140,
sub_82110B50) and opens Main Menu scene 2/8. Cvar `vf2_arcade_end_to_menu`.
Verified with a lost run (GAME OVER) → Main Menu → a new Arcade run started.
The clear + name-entry path ends in the same mode 3, but was not played
through in a test. The odd colours were the attract demo; not investigated.

## Save system: root cause of "No storage device" (2026-10-17)

Not Codex's TaskWarning skip: TaskWarning (vtable 0x82051D94) only shows
string 747 for 180 frames and polls save status; it starts nothing. Storage
selection lives in TaskSystemData (vtable 0x8204FFDC, update 0x82192128 →
sub_8218F000 → XamShowDeviceSelectorUI), which the native Push Start flow
runs. The old startup jumped straight to Main Menu (scene 2/8), skipping it:
debug log of that build with a copy of the user's save shows no device
selector and no `save:` mount. With the Push Start flow (installed build):
XamShowDeviceSelectorUI(user 0) → success, XamContentCreateEnumerator finds
the save, `save:` → \Device\Content\1\ mounted and read, then unmounted.

## Achievement arrows and hidden Dural (2026-10-17)

- Expert Mode text is correct UTF-8 (E2 86 93 … = ↓↓↑↑←←→→). The ImGui atlas
  is baked statically, so only listed glyph ranges exist: added 0x2010-0x2027
  and 0x2190-0x2193 to the Segoe UI config; the dialog spaces consecutive
  arrows. Verified in a window capture of the overlay.
- Dural: native check at character-select confirmation (side 1 0xC8BC–0xC8E0,
  side 2 0xCA00–0xCA24): Ver.2.1 flag, side Start bit in 0x500700 and
  options+0x3001 % 17 == 0. Holding Start for 3 s (user 0 = side 1, user 1 =
  side 2, only while the select loop runs) injects A to confirm and sets
  interpreter r15 before each test, so the native branch picks ID 9.
  Verified: Arcade run started as Dural vs Lau. Cvar `vf2_dural_hold_start`.

## Release 1.2.0 (2026-10-17)

- Upscaled panorama replacements (F6 Display and launcher "Upscaled 2D
  Backgrounds" → vf2_background_replacements in vf2.toml). Sky/ground fills
  use two rows appended to the replacement texture with the average colour
  of the image's own top/bottom row; native per-line wave applied in strips.
- Hidden Dural toggle in F6 "Arcade & Cheats".
- VF2_GAME_VERSION / VF2_LAUNCHER_VERSION 1.2.0 (touch resources/*.rc after
  a version bump: the generated header is not a tracked RC dependency).
