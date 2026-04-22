/* ===========================================================================
 * electronOS Desktop — Text Editor App
 * ===========================================================================
 * Simple text editor with open/save/edit file capabilities.
 * =========================================================================== */

#ifndef ELECTRONOS_DESKTOP_APP_EDITOR_H
#define ELECTRONOS_DESKTOP_APP_EDITOR_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include "window.h"

#define EDITOR_MAX_LINES    65536
#define EDITOR_MAX_LINE_LEN 4096
#define EDITOR_TAB_SIZE     4

/* Editor line */
typedef struct {
    char    *text;
    int      len;
    int      cap;
} editor_line_t;

/* Editor context */
typedef struct {
    editor_line_t  *lines;
    int             num_lines;
    int             lines_cap;

    /* Cursor */
    int             cursor_row;
    int             cursor_col;

    /* Selection (future) */
    bool            has_selection;
    int             sel_start_row;
    int             sel_start_col;
    int             sel_end_row;
    int             sel_end_col;

    /* Scroll */
    int             scroll_row;
    int             scroll_col;

    /* File */
    char            filepath[1024];
    bool            modified;
    bool            has_file;

    /* Rendering */
    TTF_Font       *font_mono;
    TTF_Font       *font_ui;
    int             cell_w;
    int             cell_h;

    /* UI state */
    bool            show_save_dialog;
    char            save_input[1024];
    int             save_input_len;
    bool            cursor_visible;
    Uint32          cursor_blink_timer;

    /* Toolbar buttons */
    SDL_Rect        btn_new;
    SDL_Rect        btn_open;
    SDL_Rect        btn_save;
} editor_ctx_t;

/* Create an editor app context. Returns app_callbacks for window use. */
app_callbacks_t app_editor_create(TTF_Font *mono_font, TTF_Font *ui_font);

#endif /* ELECTRONOS_DESKTOP_APP_EDITOR_H */
