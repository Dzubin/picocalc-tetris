/*
 *  PicoCalc Tetris
 *  ===============
 *
 *  A from-scratch Tetris game for the ClockworkPi PicoCalc, written to match
 *  the original Game Boy Tetris (1989) as closely as practical, based on what
 *  I learned reverse-engineering that ROM in Ghidra.
 *  Author: Thomas Dzubin.
 *
 *  This is an original implementation. No code, data tables, graphics or
 *  music from the Game Boy ROM are copied here. Only concepts confirmed by
 *  decompiling it were carried over, each noted below. Anything not noted is
 *  this program's own invention.
 *
 *      - The board is 10 columns x 18 rows (the real Game Boy field size,
 *        unlike the 10x20 field most other Tetris versions use).
 *      - Scoring is the classic 40 / 100 / 300 / 1200 (x current level+1)
 *        table for a single / double / triple / tetris line clear, found in
 *        the ROM's decompiled code.
 *      - The piece randomizer draws a random piece and, if it would repeat
 *        the previous one, re-rolls up to two more times before giving up and
 *        allowing the repeat. The real ROM seeds this from the Game Boy's
 *        free-running DIV register. There is no DIV register here, so the
 *        generator is stirred with the Pico's free-running microsecond timer
 *        instead (see rng_stir).
 *      - No wall kicks: a rotation that does not fit is simply rejected,
 *        matching the original's simple 4-state rotation (often called the
 *        "classic" rotation system, as opposed to modern SRS with kicks).
 *
 *  Background music: "Korobeiniki" ("The Peddlers"), the Russian folk song
 *  Tetris made famous, is an 1800s folk melody, public domain in its own
 *  right and independent of Nintendo's arrangement of it. MELODY[] in the
 *  config header is my own transcription of that tune, played by a small
 *  non-blocking sequencer (see "Background music" below), not a copy of the
 *  ROM's music data. M mutes it.
 *
 *  Deliberately not reproduced, because it would move from "inspired by" to
 *  "a copy of" the commercial game and wasn't needed for the mechanics:
 *      - Exact frame-perfect gravity speeds and key repeat timing. These are
 *        approximated to feel right, not measured from the ROM.
 *      - The original's monochrome look. Piece colours are my own choice.
 *
 *  Built on the Raspberry Pi Pico SDK and the drivers from
 *  "picocalc-text-starter" by Blair Leduc (drivers/lcd.c, drivers/audio.c,
 *  drivers/southbridge.c). The keyboard FIFO is polled directly with
 *  sb_read_keyboard() (see key_event) to get raw press and release events,
 *  so picocalc_init() is not called.
 *
 *  ------------------------------------------------------------------------
 *  Controls
 *  ------------------------------------------------------------------------
 *    Splash screen ......... any key starts a new game; Q asks "quit?" first
 *    LEFT / RIGHT .......... move the falling piece (auto-repeats if held)
 *    DOWN .................. soft drop (fall faster while held)
 *    SPACE ................. rotate clockwise
 *    UP .................... rotate counter-clockwise (opposite of SPACE)
 *    ENTER or P ............ pause / resume
 *    M ..................... mute / unmute the background music
 *    Q ..................... pause and ask "quit?": Y exits the program (to
 *                            the PicoCalc UF2 Loader menu, or closes the
 *                            window on a PC), any other key resumes
 *    ESC ................... quit to the splash screen
 *    ~  (SHIFT + backtick) . reboot into BOOTSEL (USB drive) mode
 *    Game over screen ...... any key returns to the splash screen
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"      /* reset_usb_boot() - BOOTSEL reboot ('~') */
#include "hardware/watchdog.h" /* watchdog_hw, watchdog_reboot() - exit to the UF2 Loader */

#include "lcd.h"          /* WIDTH, HEIGHT, GLYPH_HEIGHT, RGB(), lcd_*   */
#include "audio.h"        /* audio_*()                                  */
#include "southbridge.h"  /* sb_init(), sb_read_keyboard()               */
#include "keyboard.h"     /* KEY_* codes, KEY_STATE_* values             */
#include "tetris_config.h"

/* audio.c references this even though the blocking song player is never
 * called, so define it to keep the link clean. */
volatile bool user_interrupt = false;

/* ===================================================================== */
/*  Shared types                                                          */
/* ===================================================================== */

typedef struct { int8_t x, y; } cell_t;

/* ===================================================================== */
/*  Function prototypes                                                   */
/* ===================================================================== */

/* random pieces */
static void     rng_stir(void);
static uint32_t rng_next(void);
static int      pick_next_piece(int prev_type);

/* piece geometry */
static void piece_cells(int type, int rot, int x, int y, cell_t out[4]);
static bool piece_fits(int type, int rot, int x, int y);

/* board / piece state */
static void spawn_piece(void);
static bool try_move(int dx, int dy);
static bool try_rotate(int dir);
static void lock_piece(void);
static int  gravity_ms_for_level(int level);
static void clear_completed_lines(void);

/* drawing: board + falling piece */
static void fill(uint16_t colour, int x, int y, int w, int h);
static void put_row(int row, const char *str, uint16_t fg);
static void put_at(int col, int row, const char *str, uint16_t fg);
static void put_in_board(int row, const char *str, uint16_t fg);
static void draw_cell(int col, int row, uint16_t colour);
static void draw_board_frame(void);
static void draw_board_full(void);
static void redraw_falling(void);
static void flash_rows(const int rows[4], int count);

/* side panel */
static void draw_panel_labels(void);
static void update_panel_values(void);
static void draw_next_preview(void);

/* sound effects */
static void play_sfx(const note_t *steps, int count);
static void snd_lock(void);
static void snd_line_clear(int n);
static void snd_level_up(void);
static void snd_game_over(void);

/* background music (see MELODY[]) */
static void music_start(void);
static void music_stop(void);
static void music_update(void);
static void music_force_resync(void);
static void music_toggle_mute(void);

/* keyboard */
static inline uint16_t key_event(void);
static inline uint8_t  ev_state(uint16_t e);
static inline uint8_t  ev_code(uint16_t e);
static void go_bootsel(void);
static void exit_to_loader(void);
static void drain_keys(void);
static uint8_t wait_any_key(void);
static bool confirm_quit(bool over_board);

/* screens */
static void splash_screen(void);
static void game_over_screen(uint32_t final_score);
static void draw_pause_overlay(void);
static void draw_quit_prompt(bool over_board);
static void resume_play(void);
static void play_game(void);

/* ===================================================================== */
/*  main                                                                  */
/* ===================================================================== */
int main(void)
{
    stdio_init_all();

    sb_init();                /* keyboard / south-bridge I2C (no bg poll) */
    lcd_init();                /* ST7365P LCD */
    lcd_enable_cursor(false);  /* we don't want the blinking text cursor */
    audio_init();              /* PIO PWM audio on GP26 / GP27 */

    rng_stir();

    for (;;) {
        splash_screen();
        if (wait_any_key() == 'q' && confirm_quit(false))
            exit_to_loader();           /* does not return */
        play_game();
    }
}

/* ===================================================================== */
/*  Game state                                                            */
/* ===================================================================== */
static uint8_t board[BOARD_H][BOARD_W];   /* 0 = empty, else type + 1 */

static int  cur_type, cur_rot, cur_x, cur_y;
static int  next_type;

/* Cells of the falling piece as last drawn. A y of -1 marks a slot with
 * nothing drawn, so draw_cell skips it. */
static cell_t prev_cells[4] = { {0,-1},{0,-1},{0,-1},{0,-1} };

static uint32_t score;
static int      level, lines_total;
static bool     paused;
static bool     game_over;

static bool     left_held, right_held, down_held;
static uint32_t left_repeat_at, right_repeat_at;
static uint32_t last_gravity_ms;

/* What the side panel currently shows, so only changed values are redrawn. */
static uint32_t panel_score_shown = 0xFFFFFFFFu;
static int      panel_level_shown = -1, panel_lines_shown = -1;
static int      panel_next_shown  = -1;

/* ===================================================================== */
/*  Random pieces                                                         */
/* ===================================================================== */
static uint32_t rng_state;

/* Mixes the free-running microsecond timer into the generator's state.
 * main() calls it at power-up and play_game() again at the start of every
 * game, when the time depends on when the player pressed a key. The timing
 * at power-up alone is almost the same every time. */
static void rng_stir(void)
{
    rng_state ^= time_us_32() ^ RNG_STIR_SALT;
    if (rng_state == 0)
        rng_state = 1;
}

/* xorshift32: fast, deterministic and fine for a game randomizer */
static uint32_t rng_next(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/*
 *  Picks the next piece type, the way the real Game Boy Tetris does as
 *  recovered from the decompiled ROM: draw a random piece, and if it is the
 *  same as the previous one, re-roll, up to two more tries. If it is still a
 *  repeat after that, allow it.
 */
static int pick_next_piece(int prev_type)
{
    int candidate = (int)(rng_next() % NUM_PIECES);
    for (int tries = 0; tries < 2 && candidate == prev_type; tries++)
        candidate = (int)(rng_next() % NUM_PIECES);
    return candidate;
}

/* ===================================================================== */
/*  Piece geometry                                                        */
/* ===================================================================== */
static void piece_cells(int type, int rot, int x, int y, cell_t out[4])
{
    for (int i = 0; i < 4; i++) {
        out[i].x = (int8_t)(x + SHAPES[type][rot][i][0]);
        out[i].y = (int8_t)(y + SHAPES[type][rot][i][1]);
    }
}

/* A piece never moves up and every shape offset is 0 or more, so a cell's
 * y is never negative and only the bottom edge needs checking. */
static bool piece_fits(int type, int rot, int x, int y)
{
    cell_t c[4];
    piece_cells(type, rot, x, y, c);
    for (int i = 0; i < 4; i++) {
        if (c[i].x < 0 || c[i].x >= BOARD_W || c[i].y >= BOARD_H)
            return false;
        if (board[c[i].y][c[i].x] != 0)
            return false;
    }
    return true;
}

/* ===================================================================== */
/*  Board / piece state machine                                           */
/* ===================================================================== */
static void spawn_piece(void)
{
    cur_type = next_type;
    next_type = pick_next_piece(cur_type);
    cur_rot = 0;
    cur_x = (BOARD_W - 4) / 2;
    cur_y = 0;

    prev_cells[0].y = prev_cells[1].y = prev_cells[2].y = prev_cells[3].y = -1;

    if (!piece_fits(cur_type, cur_rot, cur_x, cur_y))
        game_over = true;

    draw_next_preview();
}

static bool try_move(int dx, int dy)
{
    if (!piece_fits(cur_type, cur_rot, cur_x + dx, cur_y + dy))
        return false;
    cur_x += dx;
    cur_y += dy;
    return true;
}

static bool try_rotate(int dir)
{
    int new_rot = (cur_rot + dir + 4) % 4;
    if (!piece_fits(cur_type, new_rot, cur_x, cur_y))
        return false;             /* classic rotation: no wall kicks */
    cur_rot = new_rot;
    return true;
}

static void lock_piece(void)
{
    cell_t c[4];
    piece_cells(cur_type, cur_rot, cur_x, cur_y, c);
    for (int i = 0; i < 4; i++)
        board[c[i].y][c[i].x] = (uint8_t)(cur_type + 1);
    snd_lock();
}

static int gravity_ms_for_level(int lvl)
{
    if (lvl > MAX_LEVEL) lvl = MAX_LEVEL;
    return GRAVITY_MS[lvl];
}

static void clear_completed_lines(void)
{
    int full_rows[BOARD_H];
    int count = 0;

    for (int r = 0; r < BOARD_H; r++) {
        bool full = true;
        for (int c = 0; c < BOARD_W; c++)
            if (board[r][c] == 0) { full = false; break; }
        if (full)
            full_rows[count++] = r;
    }

    if (count == 0)
        return;

    flash_rows(full_rows, count);
    snd_line_clear(count);

    /* collapse: rebuild the board, skipping the cleared rows */
    uint8_t newboard[BOARD_H][BOARD_W];
    memset(newboard, 0, sizeof newboard);
    int write_row = BOARD_H - 1;
    for (int r = BOARD_H - 1; r >= 0; r--) {
        bool cleared = false;
        for (int i = 0; i < count; i++)
            if (full_rows[i] == r) { cleared = true; break; }
        if (cleared)
            continue;
        memcpy(newboard[write_row], board[r], BOARD_W);
        write_row--;
    }
    memcpy(board, newboard, sizeof board);

    static const uint16_t LINE_SCORE[4] = {
        SCORE_SINGLE, SCORE_DOUBLE, SCORE_TRIPLE, SCORE_TETRIS
    };
    score += (uint32_t)LINE_SCORE[count - 1] * (uint32_t)(level + 1);

    int old_level = level;
    lines_total += count;
    level = lines_total / LINES_PER_LEVEL;
    if (level != old_level)
        snd_level_up();

    draw_board_full();
}

/* ===================================================================== */
/*  Drawing helpers                                                       */
/* ===================================================================== */
static void fill(uint16_t colour, int x, int y, int w, int h)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > WIDTH)  w = WIDTH  - x;
    if (y + h > HEIGHT) h = HEIGHT - y;
    if (w > 0 && h > 0)
        lcd_solid_rectangle(colour, (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);
}

/* clear a text row and print a string centred on it */
static void put_row(int row, const char *str, uint16_t fg)
{
    lcd_solid_rectangle(COL_BG, 0, (uint16_t)(row * GLYPH_HEIGHT), WIDTH, GLYPH_HEIGHT);
    int len = (int)strlen(str);
    int col = (TCOLS - len) / 2;
    if (col < 0) col = 0;
    lcd_set_foreground(fg);
    lcd_putstr((uint8_t)col, (uint8_t)row, str);
}

/* print left-aligned at a given text column/row, clearing just that span */
static void put_at(int col, int row, const char *str, uint16_t fg)
{
    lcd_solid_rectangle(COL_BG, (uint16_t)(col * 8), (uint16_t)(row * GLYPH_HEIGHT),
                         (uint16_t)((TCOLS - col) * 8), GLYPH_HEIGHT);
    lcd_set_foreground(fg);
    lcd_putstr((uint8_t)col, (uint8_t)row, str);
}

/* Prints a string centred across the board's width on a text row. Unlike
 * put_row and put_at it clears nothing, so it can be used for text drawn
 * over a box in the middle of the board. */
static void put_in_board(int row, const char *str, uint16_t fg)
{
    int len = (int)strlen(str);
    int col = BOARD_COL + (BOARD_COLS - len) / 2;
    if (col < BOARD_COL)
        col = BOARD_COL;
    lcd_set_foreground(fg);
    lcd_putstr((uint8_t)col, (uint8_t)row, str);
}

/* one board cell; colour COL_BG clears it back to background, leaving the
 * 1px COL_GRID border (drawn once by draw_board_frame) visible */
static void draw_cell(int col, int row, uint16_t colour)
{
    if (row < 0)
        return;
    fill(colour, BOARD_X + col * CELL + 1, BOARD_Y + row * CELL + 1, CELL - 2, CELL - 2);
}

/* Draws the empty board once: a white frame just outside it so the player can
 * see the movement limits, then a solid grid-colour rectangle with every cell
 * punched out to background, which leaves a 1px grid line between cells. */
static void draw_board_frame(void)
{
    fill(COL_WHITE, BOARD_X - BOARD_BORDER, BOARD_Y - BOARD_BORDER,
         BOARD_PX_W + 2 * BOARD_BORDER, BOARD_PX_H + 2 * BOARD_BORDER);

    fill(COL_GRID, BOARD_X, BOARD_Y, BOARD_PX_W, BOARD_PX_H);
    for (int r = 0; r < BOARD_H; r++)
        for (int c = 0; c < BOARD_W; c++)
            draw_cell(c, r, COL_BG);
}

/* redraw every locked cell (used after a line clear collapses rows) */
static void draw_board_full(void)
{
    for (int r = 0; r < BOARD_H; r++)
        for (int c = 0; c < BOARD_W; c++)
            draw_cell(c, r, board[r][c] ? PIECE_COLOUR[board[r][c] - 1] : COL_BG);
}

/* Redraws the falling piece: erases its previous cells, then draws its
 * current ones. Called whenever it moves, rotates, falls or is spawned. */
static void redraw_falling(void)
{
    cell_t cur[4];
    piece_cells(cur_type, cur_rot, cur_x, cur_y, cur);

    for (int i = 0; i < 4; i++)
        draw_cell(prev_cells[i].x, prev_cells[i].y, COL_BG);
    for (int i = 0; i < 4; i++)
        draw_cell(cur[i].x, cur[i].y, PIECE_COLOUR[cur_type]);

    memcpy(prev_cells, cur, sizeof cur);
}

/* Flashes the completed rows a few times before clear_completed_lines()
 * collapses them. rows[] holds `count` row indices in any order. */
static void flash_rows(const int rows[4], int count)
{
    for (int f = 0; f < CLEAR_FLASH_COUNT; f++) {
        for (int i = 0; i < count; i++)
            fill(COL_WHITE, BOARD_X, BOARD_Y + rows[i] * CELL, BOARD_PX_W, CELL);
        sleep_ms(CLEAR_FLASH_MS);
        for (int i = 0; i < count; i++)
            fill(COL_GRID, BOARD_X, BOARD_Y + rows[i] * CELL, BOARD_PX_W, CELL);
        sleep_ms(CLEAR_FLASH_MS);
    }
}

/* ===================================================================== */
/*  Side panel                                                            */
/* ===================================================================== */
static void draw_panel_labels(void)
{
    static const char *const keys[] = {
        "ARROWS MOVE", "SPACE/UP ROTATE", "ENTER PAUSE",
        "M MUTE", "ESC QUIT", "Q EXIT",
    };

    fill(COL_BG, PANEL_X, 0, WIDTH - PANEL_X, HEIGHT);
    put_at(PANEL_COL, PANEL_ROW_TITLE, "TETRIS", COL_CYAN);
    put_at(PANEL_COL, PANEL_ROW_SCORE, "SCORE", COL_LABEL);
    put_at(PANEL_COL, PANEL_ROW_LEVEL, "LEVEL", COL_LABEL);
    put_at(PANEL_COL, PANEL_ROW_LINES, "LINES", COL_LABEL);
    put_at(PANEL_COL, PANEL_ROW_NEXT, "NEXT", COL_LABEL);
    for (int i = 0; i < NELEMS(keys); i++)
        put_at(PANEL_COL, PANEL_ROW_KEYS + i, keys[i], COL_LABEL);

    panel_score_shown = 0xFFFFFFFFu;
    panel_level_shown = panel_lines_shown = panel_next_shown = -1;
}

static void update_panel_values(void)
{
    char buf[12];
    if (score != panel_score_shown) {
        snprintf(buf, sizeof buf, "%lu", (unsigned long)score);
        put_at(PANEL_COL, PANEL_ROW_SCORE + 1, buf, COL_WHITE);
        panel_score_shown = score;
    }
    if (level != panel_level_shown) {
        snprintf(buf, sizeof buf, "%d", level);
        put_at(PANEL_COL, PANEL_ROW_LEVEL + 1, buf, COL_WHITE);
        panel_level_shown = level;
    }
    if (lines_total != panel_lines_shown) {
        /* progress toward the next level, e.g. "3/10": a clear, fixed-size
         * objective rather than an ever-growing cumulative total */
        snprintf(buf, sizeof buf, "%d/%d",
                 lines_total % LINES_PER_LEVEL, LINES_PER_LEVEL);
        put_at(PANEL_COL, PANEL_ROW_LINES + 1, buf, COL_WHITE);
        panel_lines_shown = lines_total;
    }
}

static void draw_next_preview(void)
{
    if (next_type == panel_next_shown)
        return;
    panel_next_shown = next_type;

    int px = PANEL_X, py = PANEL_ROW_PREVIEW * GLYPH_HEIGHT;
    fill(COL_BG, px, py, 4 * PREV_CELL, 4 * PREV_CELL);
    for (int i = 0; i < 4; i++) {
        int cx = SHAPES[next_type][0][i][0];
        int cy = SHAPES[next_type][0][i][1];
        fill(PIECE_COLOUR[next_type],
             px + cx * PREV_CELL + 1, py + cy * PREV_CELL + 1,
             PREV_CELL - 2, PREV_CELL - 2);
    }
}

/* ===================================================================== */
/*  Sound effects (short and original, not from the ROM)                  */
/* ===================================================================== */

/* Plays the steps of a sound effect one after another, waiting for each. */
static void play_sfx(const note_t *steps, int count)
{
    for (int i = 0; i < count; i++)
        audio_play_sound_blocking(steps[i].freq, steps[i].freq, steps[i].dur_ms);
}

/* Each effect blocks while it plays and leaves the audio channel silent
 * afterwards, so the ones used during a game finish by telling the music to
 * start its current note again. */
static void snd_lock(void)
{
    play_sfx(SFX_LOCK, NELEMS(SFX_LOCK));
    music_force_resync();
}

static void snd_line_clear(int n)
{
    if (n >= 4)
        play_sfx(SFX_TETRIS, NELEMS(SFX_TETRIS));
    else
        play_sfx(SFX_LINE_CLEAR, NELEMS(SFX_LINE_CLEAR));
    music_force_resync();
}

static void snd_level_up(void)
{
    play_sfx(SFX_LEVEL_UP, NELEMS(SFX_LEVEL_UP));
    music_force_resync();
}

static void snd_game_over(void)
{
    play_sfx(SFX_GAME_OVER, NELEMS(SFX_GAME_OVER));
}

/* ===================================================================== */
/*  Background music: "Korobeiniki"                                       */
/*                                                                        */
/*  MELODY[] (in the config header) is a transcription of the traditional */
/*  Russian folk song, made from scratch with my own choice of octave and */
/*  note lengths. A small sequencer driven by the wall clock                */
/*  (music_update) plays it on a loop through the same PWM channel the    */
/*  sound effects use. A blocking sound effect silences that channel when */
/*  it finishes, so each one ends with music_force_resync(), which makes  */
/*  the next music_update() call start again whichever note the timeline  */
/*  says should be playing, instead of leaving the channel silent.        */
/* ===================================================================== */
static bool     music_on = true;
static uint32_t music_start_ms;
static uint32_t music_total_ms;
static int      music_note_idx = -1;

static void music_start(void)
{
    if (music_total_ms == 0)
        for (int i = 0; i < NELEMS(MELODY); i++)
            music_total_ms += MELODY[i].dur_ms;

    music_start_ms = to_ms_since_boot(get_absolute_time());
    music_note_idx = -1;
}

static void music_stop(void)
{
    audio_stop();
    music_note_idx = -1;
}

static void music_force_resync(void)
{
    music_note_idx = -1;
}

static void music_toggle_mute(void)
{
    music_on = !music_on;
    if (!music_on)
        audio_stop();
    music_note_idx = -1;   /* start the right note again either way */
}

/* Driven by the wall clock: works out which note should be playing now from
 * the elapsed time modulo the tune's total length, and starts it only when
 * that differs from the last note set. */
static void music_update(void)
{
    if (!music_on)
        return;

    uint32_t now     = to_ms_since_boot(get_absolute_time());
    uint32_t elapsed = (now - music_start_ms) % music_total_ms;

    uint32_t acc = 0;
    int idx = 0;
    for (; idx < NELEMS(MELODY) - 1; idx++) {
        if (elapsed < acc + MELODY[idx].dur_ms)
            break;
        acc += MELODY[idx].dur_ms;
    }

    if (idx != music_note_idx) {
        music_note_idx = idx;
        if (MELODY[idx].freq)
            audio_play_sound(MELODY[idx].freq, MELODY[idx].freq);
        else
            audio_stop();
    }
}

/* ===================================================================== */
/*  Keyboard (raw south-bridge FIFO polling)                              */
/* ===================================================================== */
static inline uint16_t key_event(void)      { return sb_read_keyboard(); }
static inline uint8_t  ev_state(uint16_t e) { return (e >> 8) & 0xFF; }
static inline uint8_t  ev_code(uint16_t e)  { return e & 0xFF; }

static inline bool is_mod_key(uint8_t c)
{
    return c == KEY_MOD_SHL || c == KEY_MOD_SHR || c == KEY_MOD_CTRL ||
           c == KEY_MOD_ALT || c == KEY_MOD_SYM;
}

static void go_bootsel(void)
{
    audio_stop();
    put_row(BOOTSEL_MSG_ROW, "REBOOTING TO BOOTSEL...", COL_YELLOW);
    sleep_ms(BOOTSEL_MSG_MS);
    reset_usb_boot(0, 0);
    while (1)
        tight_loop_contents();
}

/* Leaves the program for the PicoCalc UF2 Loader's menu: shows a short
 * message, asks the loader for its menu (see LOADER_COMMAND_MAGIC in the
 * config header) and reboots with the watchdog. Both PicoCalc chips take the
 * same path. The PC builds have no loader, and their stand-in
 * watchdog_reboot() closes the program instead. */
static void exit_to_loader(void)
{
    audio_stop();
    lcd_clear_screen();
    put_row(EXIT_MSG_ROW, "EXITING...", COL_YELLOW);
    sleep_ms(EXIT_MSG_MS);

    watchdog_hw->scratch[LOADER_SCRATCH_MODE] = LOADER_BOOT_MODE_SD;
    watchdog_hw->scratch[LOADER_SCRATCH_ARGUMENT] = 0;
    watchdog_hw->scratch[LOADER_SCRATCH_MAGIC] = LOADER_COMMAND_MAGIC;
    watchdog_reboot(0, 0, LOADER_REBOOT_DELAY_MS);

    while (1)
        tight_loop_contents();      /* the reboot comes in a few milliseconds */
}

static void drain_keys(void)
{
    while (key_event() != 0)
        tight_loop_contents();
}

/* Drains the keyboard, then waits for a fresh key press (a bare modifier
 * does not count, and ~ goes to BOOTSEL) and returns its code, lowercased
 * if it is a letter, so a caller can test for a particular key or just
 * ignore the result. */
static uint8_t wait_any_key(void)
{
    drain_keys();
    for (;;) {
        uint16_t e = key_event();
        if (e && ev_state(e) == KEY_STATE_PRESSED) {
            uint8_t c = ev_code(e);
            if (is_mod_key(c))
                continue;
            if (c == '~')
                go_bootsel();       /* does not return */
            c = (c >= 'A' && c <= 'Z') ? (uint8_t)(c + 0x20) : c;
            drain_keys();
            return c;
        }
        sleep_ms(KEY_WAIT_POLL_MS);
    }
}

/* Silences the music, shows the quit prompt (in a box over the board during
 * a game, on a cleared screen otherwise) and waits for a key. True only for Y
 * in either case; any other key means no. Waiting drains the keyboard, so
 * during a game the caller has to put the screen and timers back (see
 * resume_play) if the answer is no. */
static bool confirm_quit(bool over_board)
{
    audio_stop();
    draw_quit_prompt(over_board);
    return wait_any_key() == 'y';
}

/* ===================================================================== */
/*  Screens                                                               */
/* ===================================================================== */
static void splash_screen(void)
{
    lcd_clear_screen();
    put_row(SPLASH_ROW_TITLE,       "PICOCALC TETRIS",                       COL_CYAN);
    put_row(SPLASH_ROW_VERSION,     VERSION,                                 COL_CYAN);
    put_row(SPLASH_ROW_AUTHOR,      "BY THOMAS DZUBIN",                      COL_LABEL);
    put_row(SPLASH_ROW_ORIGIN,      "Learned from decompiling the real",     COL_LABEL);
    put_row(SPLASH_ROW_ORIGIN + 1,  "Game Boy Tetris ROM in Ghidra",         COL_LABEL);
    put_row(SPLASH_ROW_ORIGIN + 2,  "(an original clean-room implementation)", COL_LABEL);
    put_row(SPLASH_ROW_MUSIC,       "Music: \"Korobeiniki\", trad. Russian",  COL_LABEL);
    put_row(SPLASH_ROW_MUSIC + 1,   "folk song (1800s, public domain)",      COL_LABEL);
    put_row(SPLASH_ROW_START,       "PRESS ANY KEY TO START",                COL_WHITE);
    put_row(SPLASH_ROW_KEYS,        "ARROWS move/drop, SPACE/UP rotate,",    COL_LABEL);
    put_row(SPLASH_ROW_KEYS + 1,    "ENTER pause, M mute, ESC quit, Q exit", COL_LABEL);
}

/* The pause box: "PAUSED" on the middle row of the overlay. The text goes
 * through put_in_board, which clears nothing around it. */
static void draw_pause_overlay(void)
{
    fill(COL_OVERLAY, OVERLAY_BOX_X, OVERLAY_BOX_Y, OVERLAY_BOX_W, OVERLAY_BOX_H);
    put_in_board(OVERLAY_ROW_TOP + 1, "PAUSED", COL_YELLOW);
}

/* The quit prompt: three lines in the same box as the pause overlay when the
 * board is on screen, or on a cleared screen (the splash screen) when not. */
static void draw_quit_prompt(bool over_board)
{
    if (over_board) {
        fill(COL_OVERLAY, OVERLAY_BOX_X, OVERLAY_BOX_Y, OVERLAY_BOX_W, OVERLAY_BOX_H);
        put_in_board(OVERLAY_ROW_TOP,     "QUIT GAME?",         COL_YELLOW);
        put_in_board(OVERLAY_ROW_TOP + 1, "Y = YES",            COL_WHITE);
        put_in_board(OVERLAY_ROW_TOP + 2, "ANY OTHER KEY = NO", COL_WHITE);
    } else {
        lcd_clear_screen();
        put_row(OVERLAY_ROW_TOP,     "QUIT GAME?",         COL_YELLOW);
        put_row(OVERLAY_ROW_TOP + 1, "Y = YES",            COL_WHITE);
        put_row(OVERLAY_ROW_TOP + 2, "ANY OTHER KEY = NO", COL_WHITE);
    }
}

static void game_over_screen(uint32_t final_score)
{
    char buf[24];
    snd_game_over();
    put_row(GAMEOVER_ROW_TITLE, "GAME OVER", COL_RED);
    snprintf(buf, sizeof buf, "SCORE %lu", (unsigned long)final_score);
    put_row(GAMEOVER_ROW_SCORE, buf, COL_WHITE);
    put_row(GAMEOVER_ROW_PROMPT, "PRESS ANY KEY", COL_LABEL);
    wait_any_key();
}

/* ===================================================================== */
/*  Main gameplay loop                                                    */
/* ===================================================================== */

/* Carries on with play after the pause box or the quit prompt covered the
 * board. It redraws the board and the falling piece, restarts the fall timer
 * so the piece does not jump a row, and forgets any held keys: key events
 * are ignored while paused and the quit prompt drains the keyboard, so a key
 * release during either is lost and the piece would keep moving by itself. */
static void resume_play(void)
{
    left_held = right_held = down_held = false;
    paused = false;
    draw_board_full();
    redraw_falling();
    last_gravity_ms = to_ms_since_boot(get_absolute_time());
    music_force_resync();
}

static void play_game(void)
{
    rng_stir();         /* the time of the player's key press is unpredictable */

    memset(board, 0, sizeof board);
    score = 0;
    level = 0;
    lines_total = 0;
    paused = false;
    game_over = false;
    left_held = right_held = down_held = false;

    lcd_clear_screen();
    draw_board_frame();
    draw_panel_labels();

    next_type = (int)(rng_next() % NUM_PIECES);   /* seed the "next" slot */
    spawn_piece();
    redraw_falling();
    update_panel_values();

    last_gravity_ms = to_ms_since_boot(get_absolute_time());
    music_start();

    for (;;) {
        uint16_t e = key_event();
        while (e) {
            uint8_t code = ev_code(e);
            uint8_t st   = ev_state(e);
            uint8_t c    = (code >= 'A' && code <= 'Z') ? (uint8_t)(code + 0x20) : code;

            if (c == '~' && st == KEY_STATE_PRESSED)
                go_bootsel();               /* does not return */

            if (code == KEY_ESC && st == KEY_STATE_PRESSED) {
                drain_keys();
                music_stop();
                return;                     /* back to the splash screen */
            }

            if (c == 'q' && st == KEY_STATE_PRESSED) {
                if (confirm_quit(true))
                    exit_to_loader();       /* does not return */

                /* Declined, so play resumes, whether or not it was paused
                 * before. */
                resume_play();
                e = key_event();
                continue;
            }

            if (c == 'm' && st == KEY_STATE_PRESSED)
                music_toggle_mute();

            if ((code == KEY_ENTER || code == KEY_RETURN || c == 'p') &&
                st == KEY_STATE_PRESSED) {
                if (paused) {
                    resume_play();
                } else {
                    paused = true;
                    audio_stop();
                    draw_pause_overlay();
                }
            }

            if (!paused) {
                if (code == KEY_LEFT) {
                    if (st == KEY_STATE_PRESSED) {
                        left_held = true;
                        if (try_move(-1, 0)) redraw_falling();
                        left_repeat_at = to_ms_since_boot(get_absolute_time()) + DAS_DELAY_MS;
                    } else if (st == KEY_STATE_RELEASED) {
                        left_held = false;
                    }
                } else if (code == KEY_RIGHT) {
                    if (st == KEY_STATE_PRESSED) {
                        right_held = true;
                        if (try_move(1, 0)) redraw_falling();
                        right_repeat_at = to_ms_since_boot(get_absolute_time()) + DAS_DELAY_MS;
                    } else if (st == KEY_STATE_RELEASED) {
                        right_held = false;
                    }
                } else if (code == KEY_DOWN) {
                    down_held = (st == KEY_STATE_PRESSED || st == KEY_STATE_HOLD);
                } else if (code == KEY_SPACE && st == KEY_STATE_PRESSED) {
                    if (try_rotate(+1)) redraw_falling();
                } else if (code == KEY_UP && st == KEY_STATE_PRESSED) {
                    if (try_rotate(-1)) redraw_falling();
                }
            }

            e = key_event();
        }

        if (game_over) {
            music_stop();
            game_over_screen(score);
            return;
        }

        if (!paused) {
            uint32_t now = to_ms_since_boot(get_absolute_time());

            music_update();

            if (left_held && now >= left_repeat_at) {
                if (try_move(-1, 0)) redraw_falling();
                left_repeat_at = now + DAS_REPEAT_MS;
            }
            if (right_held && now >= right_repeat_at) {
                if (try_move(1, 0)) redraw_falling();
                right_repeat_at = now + DAS_REPEAT_MS;
            }

            int interval = down_held ? SOFT_DROP_MS : gravity_ms_for_level(level);
            if (now - last_gravity_ms >= (uint32_t)interval) {
                last_gravity_ms = now;
                if (try_move(0, 1)) {
                    redraw_falling();
                } else {
                    lock_piece();
                    clear_completed_lines();
                    spawn_piece();

                    /* The sounds and the line clear flash block for a while,
                     * so the new piece gets a full fall interval from now. */
                    last_gravity_ms = to_ms_since_boot(get_absolute_time());
                    if (!game_over) redraw_falling();
                }
            }

            update_panel_values();
        }

        sleep_ms(IDLE_SLEEP_MS);
    }
}
