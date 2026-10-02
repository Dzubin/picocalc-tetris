/*
 *  PicoCalc Tetris: compile-time configuration
 *  ===========================================
 *  Every #define, tunable and constant data table lives here, so tetris.c
 *  holds only code (see tetris.c for the design notes and the game itself).
 *  Author: Thomas Dzubin.
 */
#pragma once

#include <stdint.h>

#include "lcd.h"          /* RGB(), GLYPH_HEIGHT */
#include "audio.h"        /* PITCH_* note frequencies */

/* Shown on the splash screen. CMakeLists.txt reads it from this line to set
 * the program version, so keep it in the form  #define VERSION "V0.01". */
#define VERSION "V0.01"

/* ===================================================================== */
/*  Board geometry and screen layout                                      */
/* ===================================================================== */

/* 10 columns x 18 rows, the playfield size of the original Game Boy Tetris
 * (most other versions, the NES one included, use 20 rows). */
#define BOARD_W 10
#define BOARD_H 18

/* On-screen size of one board cell. The LCD is 320x320, so the board is
 * BOARD_W*CELL x BOARD_H*CELL = 160 x 288 pixels, with a 16 pixel margin
 * above and below it and the rest of the width left for the side panel. */
#define CELL        16
#define BOARD_X     8
#define BOARD_Y     16
#define BOARD_PX_W  (BOARD_W * CELL)
#define BOARD_PX_H  (BOARD_H * CELL)

/* Thickness in pixels of the white frame drawn just outside the board, so
 * the edges of the play area are easy to see. */
#define BOARD_BORDER 3

/* Text grid: 40 columns x 32 rows of the built-in 8x10 font. */
#define TCOLS 40

/* The board's first text column and its width in text columns. */
#define BOARD_COL   (BOARD_X / 8)
#define BOARD_COLS  (BOARD_PX_W / 8)

/* Side panel right of the board: score, level, lines, a preview of the next
 * piece and a key legend. The PANEL_ROW_* values are text rows. A score,
 * level or lines label has its value on the row below it. */
#define PANEL_X            (BOARD_X + BOARD_PX_W + 16)
#define PANEL_COL          (PANEL_X / 8)
#define PANEL_ROW_TITLE    1
#define PANEL_ROW_SCORE    3
#define PANEL_ROW_LEVEL    6
#define PANEL_ROW_LINES    9
#define PANEL_ROW_NEXT     12
#define PANEL_ROW_PREVIEW  14
#define PANEL_ROW_KEYS     25
#define PREV_CELL          14   /* pixel size of a cell in the next-piece preview */

/* Text rows of the splash and game over screens. Where a text block has
 * several lines, the constant is the first row. */
#define SPLASH_ROW_TITLE     6
#define SPLASH_ROW_VERSION   7
#define SPLASH_ROW_AUTHOR    10
#define SPLASH_ROW_ORIGIN    12   /* three lines */
#define SPLASH_ROW_MUSIC     16   /* two lines */
#define SPLASH_ROW_START     20
#define SPLASH_ROW_KEYS      22   /* two lines */
#define GAMEOVER_ROW_TITLE   13
#define GAMEOVER_ROW_SCORE   15
#define GAMEOVER_ROW_PROMPT  18

/* ===================================================================== */
/*  Timing                                                                */
/* ===================================================================== */

/* Delayed auto-shift for held LEFT/RIGHT/DOWN: the initial delay before the
 * key starts repeating, then the repeat interval. The values are a feel
 * approximation, not a measurement of the original ROM's frame counts. */
#define DAS_DELAY_MS   220
#define DAS_REPEAT_MS  60

/* Soft drop (DOWN held) fall interval, overriding the level's gravity. */
#define SOFT_DROP_MS   50

/* Line clear flash animation: the number of flashes and the on and off time
 * of each, before the rows collapse. */
#define CLEAR_FLASH_COUNT 3
#define CLEAR_FLASH_MS    80

/* Main loop idle sleep, so polling the keyboard over I2C doesn't use the
 * CPU for nothing. */
#define IDLE_SLEEP_MS 4

/* Poll interval (ms) while waiting for a key press. */
#define KEY_WAIT_POLL_MS 3

/* ===================================================================== */
/*  Pause box, quit prompt and leaving the game                           */
/* ===================================================================== */

/* The pause box and the quit prompt are drawn over the middle of the board:
 * three rows of text in a box half a text row taller above and below,
 * centred vertically on the board. */
#define OVERLAY_ROW_TOP  ((BOARD_Y + BOARD_PX_H / 2) / GLYPH_HEIGHT - 2)
#define OVERLAY_BOX_X    BOARD_X
#define OVERLAY_BOX_Y    (OVERLAY_ROW_TOP * GLYPH_HEIGHT - GLYPH_HEIGHT / 2)
#define OVERLAY_BOX_W    BOARD_PX_W
#define OVERLAY_BOX_H    (4 * GLYPH_HEIGHT)

/* Text row and display time (ms) of the message shown before rebooting to
 * BOOTSEL, and of the "EXITING..." message shown before leaving the game. */
#define BOOTSEL_MSG_ROW  15
#define BOOTSEL_MSG_MS   300
#define EXIT_MSG_ROW     15
#define EXIT_MSG_MS      300

/* Leaving for the PicoCalc UF2 Loader (pelrun/uf2loader), PicoCalc builds
 * only. The loader has no call an application can use, but its own menu
 * passes commands to its start-up code through the chip's watchdog scratch
 * registers, which survive a watchdog reboot: scratch 0 holds a magic number,
 * 1 the boot mode and 2 an argument. Asking for boot mode "SD" and then
 * rebooting makes the loader show its menu again. If the program was flashed
 * straight onto the chip with no loader, nothing reads the request and the
 * game simply restarts. */
#define LOADER_COMMAND_MAGIC     0xE98CC638u  /* PICOCALC_BL_MAGIC in the loader's proginfo.h */
#define LOADER_BOOT_MODE_SD      1            /* BOOT_SD: load the menu from the SD card */
#define LOADER_SCRATCH_MAGIC     0
#define LOADER_SCRATCH_MODE      1
#define LOADER_SCRATCH_ARGUMENT  2
/* The watchdog reboot happens this many ms after it is requested. */
#define LOADER_REBOOT_DELAY_MS   10

/* ===================================================================== */
/*  Gameplay tuning                                                       */
/* ===================================================================== */

/* Classic single/double/triple/tetris scores, multiplied by (level+1). This
 * table matches the real Game Boy Tetris ROM, confirmed by decompiling it. */
#define SCORE_SINGLE 40
#define SCORE_DOUBLE 100
#define SCORE_TRIPLE 300
#define SCORE_TETRIS 1200

/* The level advances every LINES_PER_LEVEL lines cleared (classic formula). */
#define LINES_PER_LEVEL 10

/* The highest level GRAVITY_MS has its own entry for; any level beyond it
 * reuses the last (fastest) entry. */
#define MAX_LEVEL 19

/* Mixed into the random number generator together with the timer, so its
 * state is never just the bare timer value. */
#define RNG_STIR_SALT 0x9E3779B9u

/* ===================================================================== */
/*  Colours (RGB565 through the driver's RGB() macro)                     */
/* ===================================================================== */

#define COL_BG       RGB(  0,   0,   0)
#define COL_WHITE    RGB(235, 235, 235)
/* Labels and key legends use a fully saturated colour: a dim grey is hard to
 * read on the LCD. */
#define COL_LABEL    RGB(255, 160,  40)
#define COL_GRID     RGB( 40,  40,  40)
/* Background of the boxes drawn over the board (pause and quit prompt). */
#define COL_OVERLAY  RGB( 20,  20,  20)
#define COL_YELLOW   RGB(255, 225,  70)
#define COL_CYAN     RGB(120, 225, 255)
#define COL_RED      RGB(255,  90,  90)
#define COL_GREEN    RGB( 90, 235, 130)

/* One colour per tetromino type, in PIECE_* order. */
#define COL_PIECE_I  RGB( 80, 220, 235)
#define COL_PIECE_O  RGB(255, 220,  60)
#define COL_PIECE_T  RGB(190, 110, 235)
#define COL_PIECE_S  RGB( 90, 220, 100)
#define COL_PIECE_Z  RGB(230,  80,  80)
#define COL_PIECE_J  RGB( 90, 130, 235)
#define COL_PIECE_L  RGB(235, 150,  60)

/* Element count of a fixed-size array. */
#define NELEMS(a) ((int)(sizeof (a) / sizeof (a)[0]))

/* ===================================================================== */
/*  Piece types and constant data tables                                  */
/* ===================================================================== */

enum { PIECE_I, PIECE_O, PIECE_T, PIECE_S, PIECE_Z, PIECE_J, PIECE_L, NUM_PIECES };

/* One note of the music or one step of a sound effect. A freq of 0 is a rest. */
typedef struct { uint16_t freq; uint16_t dur_ms; } note_t;

/* Each piece has 4 rotation states, each a list of 4 cell offsets (x, y)
 * inside a 4x4 box. These are the standard "classic" tetromino rotations
 * that countless Tetris programs use, not data taken from the ROM. O has no
 * visible rotation, and I, S and Z have only 2 distinct states, repeated.
 * Every offset is 0 or more, which the board code relies on. */
static const int8_t SHAPES[NUM_PIECES][4][4][2] = {
    [PIECE_I] = {
        { {0,1},{1,1},{2,1},{3,1} },
        { {2,0},{2,1},{2,2},{2,3} },
        { {0,1},{1,1},{2,1},{3,1} },
        { {2,0},{2,1},{2,2},{2,3} },
    },
    [PIECE_O] = {
        { {1,1},{2,1},{1,2},{2,2} },
        { {1,1},{2,1},{1,2},{2,2} },
        { {1,1},{2,1},{1,2},{2,2} },
        { {1,1},{2,1},{1,2},{2,2} },
    },
    [PIECE_T] = {
        { {1,0},{0,1},{1,1},{2,1} },
        { {1,0},{1,1},{2,1},{1,2} },
        { {0,1},{1,1},{2,1},{1,2} },
        { {1,0},{0,1},{1,1},{1,2} },
    },
    [PIECE_S] = {
        { {1,0},{2,0},{0,1},{1,1} },
        { {1,0},{1,1},{2,1},{2,2} },
        { {1,0},{2,0},{0,1},{1,1} },
        { {1,0},{1,1},{2,1},{2,2} },
    },
    [PIECE_Z] = {
        { {0,0},{1,0},{1,1},{2,1} },
        { {2,0},{1,1},{2,1},{1,2} },
        { {0,0},{1,0},{1,1},{2,1} },
        { {2,0},{1,1},{2,1},{1,2} },
    },
    [PIECE_J] = {
        { {0,0},{0,1},{1,1},{2,1} },
        { {1,0},{2,0},{1,1},{1,2} },
        { {0,1},{1,1},{2,1},{2,2} },
        { {1,0},{1,1},{0,2},{1,2} },
    },
    [PIECE_L] = {
        { {2,0},{0,1},{1,1},{2,1} },
        { {1,0},{1,1},{1,2},{2,2} },
        { {0,1},{1,1},{2,1},{0,2} },
        { {0,0},{1,0},{1,1},{1,2} },
    },
};

static const uint16_t PIECE_COLOUR[NUM_PIECES] = {
    COL_PIECE_I, COL_PIECE_O, COL_PIECE_T, COL_PIECE_S,
    COL_PIECE_Z, COL_PIECE_J, COL_PIECE_L,
};

/* Gravity: milliseconds per row of fall, indexed by level (clamped to
 * MAX_LEVEL). The shape of the curve (fast start, easing off, then a long
 * fast tail) is inspired by the general feel of classic Tetris, not measured
 * from the ROM's own frame count table. */
static const uint16_t GRAVITY_MS[MAX_LEVEL + 1] = {
    800, 720, 630, 550, 470, 380, 300, 220, 130, 100,
     90,  80,  80,  70,  70,  70,  60,  60,  60,  50,
};

/* Background music: "Korobeiniki", the traditional Russian folk song (1800s,
 * public domain), transcribed from scratch with my own choice of octave and
 * note lengths. It plays on a loop. */
static const note_t MELODY[] = {
    /* phrase 1 */
    { PITCH_E5, 300 }, { PITCH_B4, 150 }, { PITCH_C5, 150 }, { PITCH_D5, 300 },
    { PITCH_C5, 150 }, { PITCH_B4, 150 }, { PITCH_A4, 300 }, { PITCH_A4, 150 },
    { PITCH_C5, 150 }, { PITCH_E5, 300 }, { PITCH_D5, 150 }, { PITCH_C5, 150 },
    { PITCH_B4, 450 }, { PITCH_C5, 150 }, { PITCH_D5, 300 }, { PITCH_E5, 300 },
    { PITCH_C5, 300 }, { PITCH_A4, 300 }, { PITCH_A4, 300 }, { 0,        300 },
    /* phrase 2 */
    { PITCH_D5, 450 }, { PITCH_F5, 150 }, { PITCH_A5, 300 }, { PITCH_G5, 150 },
    { PITCH_F5, 150 }, { PITCH_E5, 450 }, { PITCH_C5, 150 }, { PITCH_E5, 300 },
    { PITCH_D5, 150 }, { PITCH_C5, 150 }, { PITCH_B4, 450 }, { PITCH_C5, 150 },
    { PITCH_D5, 300 }, { PITCH_E5, 300 }, { PITCH_C5, 300 }, { PITCH_A4, 300 },
    { PITCH_A4, 300 }, { 0,        300 },
};

/* Short sound effects, played one step after another (my own, not from the
 * ROM). Frequencies are in Hz and durations in ms. */
static const note_t SFX_LOCK[]       = { { 150, 35 } };
static const note_t SFX_LINE_CLEAR[] = { { 520, 60 }, { 780, 80 } };
static const note_t SFX_TETRIS[]     = { { 660, 70 }, { 880, 70 }, { 990, 70 }, { 1320, 70 } };
static const note_t SFX_LEVEL_UP[]   = { { 523, 60 }, { 659, 60 }, { 784, 100 } };
static const note_t SFX_GAME_OVER[]  = { { 392, 150 }, { 330, 150 }, { 262, 300 } };
