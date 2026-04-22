/* ===========================================================================
 * electronOS Desktop — Terminal App
 * ===========================================================================
 * PTY-based terminal emulator that runs electronos-shell.
 * Parses basic VT100/ANSI escape sequences for colors and cursor movement.
 * =========================================================================== */

#ifndef ELECTRONOS_DESKTOP_APP_TERMINAL_H
#define ELECTRONOS_DESKTOP_APP_TERMINAL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include "window.h"

/* Terminal cell attributes */
typedef struct {
    char     ch;
    uint8_t  fg;        /* Color index (0-15) */
    uint8_t  bg;        /* Color index (0-15, 0=default) */
    bool     bold;
} term_cell_t;

/* Terminal context */
typedef struct {
    int          pty_fd;        /* PTY master fd */
    pid_t        child_pid;     /* Shell process */

    /* Character grid */
    term_cell_t *cells;
    int          rows;
    int          cols;
    int          cursor_row;
    int          cursor_col;

    /* Current attributes */
    uint8_t      cur_fg;
    uint8_t      cur_bg;
    bool         cur_bold;

    /* VT100 parser state */
    int          parse_state;   /* 0=normal, 1=esc, 2=csi */
    char         csi_buf[64];
    int          csi_len;

    /* Scroll */
    int          scroll_top;
    int          scroll_bottom;

    /* Rendering */
    TTF_Font    *font;
    int          cell_w;
    int          cell_h;
    bool         cursor_visible;
    Uint32       cursor_blink_timer;

    bool         alive;         /* false when shell exits */
} terminal_ctx_t;

/* Create a terminal app context. Returns app_callbacks for window use. */
app_callbacks_t app_terminal_create(TTF_Font *mono_font);

#endif /* ELECTRONOS_DESKTOP_APP_TERMINAL_H */
