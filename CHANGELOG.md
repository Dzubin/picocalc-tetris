# Changelog

All notable changes to PicoCalc Tetris.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches the `VERSION` define in `tetris.c` / `tetris_config.h`.

## [0.01] - Unreleased

First cut of the game — original, from-scratch implementation, not a port of
the decompiled Game Boy ROM (see README.md for exactly what was and wasn't
carried over from that analysis).

### Added
- Vendored `drivers/fat32.c`: `get_next_free_cluster()` now advances its search
  hint past the cluster it hands out (a local fix, not in upstream), so a long
  sequential write to the SD card no longer re-scans every cluster already given
  out and looks hung.
- Desktop build: the build logic that was repeated in `desktop/CMakeLists.txt` now
  lives in `desktop/shim_desktop.cmake` (that file now only names the program and
  its sources), and the shim files were refreshed from a shared copy. The numeric
  keypad now works in the desktop build.
- 10x18 board, 7 tetrominoes with classic (no-wall-kick) rotation.
- Gravity that speeds up by level; soft drop (DOWN held); DAS auto-repeat
  on LEFT/RIGHT.
- Line clear with a flash animation, classic 40/100/300/1200 x (level+1)
  scoring, and level-up every 10 lines.
- Next-piece preview and a score/level/lines side panel.
- Piece randomizer with the "re-roll up to twice to avoid an immediate
  repeat" behaviour confirmed in the real ROM.
- Short original sound effects for lock / line clear / level up / game over.
- Background music: an original transcription of the public-domain folk
  song "Korobeiniki", played by a small non-blocking sequencer; `M` mutes it.
- Splash and game-over screens; `~` reboots to BOOTSEL from anywhere.
- Windows build: `desktop/` holds a small SDL2 shim that re-implements the
  picocalc-text-starter driver API (LCD, keyboard, audio) so the same
  `tetris.c` runs on a PC; SDL2 is linked statically, giving one
  self-contained `picocalc-tetris-Windows.exe`.
- Linux build: the `desktop/` build (formerly `windows/`) now also builds on
  Linux and produces `picocalc-tetris-Linux`. SDL2 is linked dynamically there and the
  build copies `libSDL2-2.0.so.0` next to the executable (found through an
  `$ORIGIN` run-path); needs the SDL2 development package (e.g. `libsdl2-dev`).
- `Q` pauses the game and asks "QUIT GAME?" in a box over the board. `Y`
  exits the program: on the PicoCalc it returns to the UF2 Loader menu (the
  same hand-over through the watchdog scratch registers that
  picocalc-platformer and PicoCalc-SD-Drive use, identical on RP2040 and
  RP2350); on Windows and Linux it closes the window. Any other key resumes
  play. The `desktop/` shim gained a stand-in for the watchdog scratch
  registers so `tetris.c` still compiles there unchanged.
- On the splash screen `Q` asks the same "QUIT GAME?" question as during a
  game: `Y` exits (to the UF2 Loader menu, or closes the window on a PC),
  any other key starts a game.
- The firmware build now names every output with the chip (the `.uf2`, `.elf`, `.bin` and `.hex` all end in `-RP2040` / `-RP2350`) and copies the `.uf2` to the top-level folder as
  `picocalc-tetris-RP2040.uf2` / `picocalc-tetris-RP2350.uf2`, so the two
  chips' builds sit side by side.

### Changed
- Secondary/label text (score/level/lines labels, splash credits, the side
  panel's key legend) was a dim grey that was hard to read on the LCD after
  the first real playtest; switched to a fully-saturated amber (`COL_LABEL`
  in `tetris_config.h`).
- The board's boundary was the same dim grey as its internal grid lines,
  making the play-area edges hard to spot; the outer frame is now a white
  3px border (`BOARD_BORDER` in `tetris_config.h`) while the internal grid
  stays dim.
- The LINES readout now shows progress toward the next level as "N/10"
  (lines cleared so far this level / lines needed) instead of just the
  ever-growing cumulative total, so there's a clear, fixed objective.
- Controls remapped after group playtesting: SPACE is now rotate clockwise,
  UP is rotate counter-clockwise (opposite of SPACE), and ENTER (or P) is
  pause — SPACE briefly doubled as pause in an earlier round, that's gone
  now that it rotates. The old Z rotate-counter-clockwise key was removed
  (redundant with UP).
- Every constant now lives in `tetris_config.h`: the screen rows of the
  splash, game over and side panel, the timings and message times, the
  pause/quit box geometry, and the data tables (piece shapes and colours,
  gravity speeds, the music and the sound effects), so `tetris.c` holds only
  code. The pause box now has the same size and position as the quit prompt.
- The side panel functions and variables were renamed (`draw_panel_labels`,
  `update_panel_values`, `panel_*_shown`), and the code comments were tidied
  to a plainer style.
- The program version in the firmware binary now comes from the `VERSION` line
  in `tetris_config.h` (it said 1.0 while the splash screen said V0.01).
- The comments and docs no longer point at another project's folder.

### Fixed
- Pausing: a LEFT, RIGHT or DOWN key released while the game was paused was
  never seen, so the piece kept sliding or dropping by itself after resuming,
  and the piece also dropped a row the instant play resumed. Resuming (from
  the pause box or the quit prompt) now clears the held keys and restarts the
  fall timer.
- After a line clear the new piece could drop a row the moment it appeared
  from about level 3 up, because the flash and sounds had used up the fall
  interval. The fall timer now restarts when the new piece spawns.
- The piece generator was only seeded at power-up, when the timing is nearly
  the same every time. The timer is now mixed in again at the start of every
  game, at the moment a key is pressed on the splash screen.
- Removed two checks for a negative row that could never be true.

Tried extending the music loop with a second contrasting section; it sounded
bad in practice, so it was pulled back out — the loop is back to the original
~9.6s main theme only.
