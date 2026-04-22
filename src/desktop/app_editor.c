/* ===========================================================================
 * electronOS Desktop — Text Editor App Implementation
 * ===========================================================================
 * Simple text editor with a toolbar (New, Open, Save), line numbers,
 * cursor navigation, and basic editing. Styled with the liquid glass theme.
 * =========================================================================== */

#include "app_editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- Layout constants --------------------------------------------------- */
#define TOOLBAR_H       32
#define GUTTER_W        48
#define LINE_PAD        4
#define BTN_W           60
#define BTN_H           24
#define BTN_GAP         6

/* ---- Helpers ------------------------------------------------------------ */

static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                        const char *text, int x, int y, SDL_Color color) {
    if (!font || !text || !*text) return;
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = { x, y, surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

/* ---- Line management ---------------------------------------------------- */

static void line_init(editor_line_t *line) {
    line->cap = 128;
    line->text = calloc((size_t)line->cap, 1);
    line->len = 0;
}

static void line_ensure_cap(editor_line_t *line, int needed) {
    if (needed < line->cap) return;
    int new_cap = line->cap * 2;
    if (new_cap < needed + 1) new_cap = needed + 1;
    char *new_text = realloc(line->text, (size_t)new_cap);
    if (new_text) {
        line->text = new_text;
        line->cap = new_cap;
    }
}

static void line_insert_char(editor_line_t *line, int col, char ch) {
    if (col < 0) col = 0;
    if (col > line->len) col = line->len;
    line_ensure_cap(line, line->len + 1);
    memmove(&line->text[col + 1], &line->text[col],
            (size_t)(line->len - col));
    line->text[col] = ch;
    line->len++;
    line->text[line->len] = '\0';
}

static void line_delete_char(editor_line_t *line, int col) {
    if (col < 0 || col >= line->len) return;
    memmove(&line->text[col], &line->text[col + 1],
            (size_t)(line->len - col - 1));
    line->len--;
    line->text[line->len] = '\0';
}

static void line_free(editor_line_t *line) {
    free(line->text);
    line->text = NULL;
    line->len = 0;
    line->cap = 0;
}

/* ---- Editor internals --------------------------------------------------- */

static void ensure_lines_cap(editor_ctx_t *e, int needed) {
    if (needed <= e->lines_cap) return;
    int new_cap = e->lines_cap * 2;
    if (new_cap < needed) new_cap = needed;
    editor_line_t *new_lines = realloc(e->lines,
                                       (size_t)new_cap * sizeof(editor_line_t));
    if (new_lines) {
        e->lines = new_lines;
        e->lines_cap = new_cap;
    }
}

static void insert_line(editor_ctx_t *e, int at) {
    ensure_lines_cap(e, e->num_lines + 1);
    if (at < e->num_lines) {
        memmove(&e->lines[at + 1], &e->lines[at],
                (size_t)(e->num_lines - at) * sizeof(editor_line_t));
    }
    line_init(&e->lines[at]);
    e->num_lines++;
}

static void delete_line(editor_ctx_t *e, int at) {
    if (at < 0 || at >= e->num_lines || e->num_lines <= 1) return;
    line_free(&e->lines[at]);
    if (at < e->num_lines - 1) {
        memmove(&e->lines[at], &e->lines[at + 1],
                (size_t)(e->num_lines - at - 1) * sizeof(editor_line_t));
    }
    e->num_lines--;
}

static void editor_new_file(editor_ctx_t *e) {
    for (int i = 0; i < e->num_lines; i++) {
        line_free(&e->lines[i]);
    }
    e->num_lines = 1;
    line_init(&e->lines[0]);
    e->cursor_row = 0;
    e->cursor_col = 0;
    e->scroll_row = 0;
    e->modified = false;
    e->has_file = false;
    e->filepath[0] = '\0';
}

static void editor_load_file(editor_ctx_t *e, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    /* Clear existing content */
    for (int i = 0; i < e->num_lines; i++) {
        line_free(&e->lines[i]);
    }
    e->num_lines = 0;

    char buf[EDITOR_MAX_LINE_LEN];
    while (fgets(buf, sizeof(buf), f)) {
        ensure_lines_cap(e, e->num_lines + 1);
        line_init(&e->lines[e->num_lines]);

        /* Strip trailing newline */
        int len = (int)strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
        if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';

        line_ensure_cap(&e->lines[e->num_lines], len);
        memcpy(e->lines[e->num_lines].text, buf, (size_t)len);
        e->lines[e->num_lines].len = len;
        e->lines[e->num_lines].text[len] = '\0';
        e->num_lines++;

        if (e->num_lines >= EDITOR_MAX_LINES) break;
    }

    fclose(f);

    if (e->num_lines == 0) {
        ensure_lines_cap(e, 1);
        line_init(&e->lines[0]);
        e->num_lines = 1;
    }

    strncpy(e->filepath, path, sizeof(e->filepath) - 1);
    e->has_file = true;
    e->modified = false;
    e->cursor_row = 0;
    e->cursor_col = 0;
    e->scroll_row = 0;
}

static void editor_save_file(editor_ctx_t *e) {
    if (!e->filepath[0]) return;

    FILE *f = fopen(e->filepath, "w");
    if (!f) return;

    for (int i = 0; i < e->num_lines; i++) {
        fputs(e->lines[i].text, f);
        fputc('\n', f);
    }

    fclose(f);
    e->modified = false;
    e->has_file = true;
}

/* ---- Callbacks ---------------------------------------------------------- */

static void editor_render_cb(void *ctx, SDL_Renderer *renderer,
                             SDL_Rect area) {
    editor_ctx_t *e = (editor_ctx_t *)ctx;
    if (!e) return;

    SDL_Color bg_color = { 26, 26, 32, 255 };
    SDL_Color toolbar_bg = { 38, 38, 46, 255 };
    SDL_Color gutter_bg = { 32, 32, 40, 255 };
    SDL_Color gutter_text = { 100, 100, 115, 255 };
    SDL_Color text_color = { 215, 215, 220, 255 };
    SDL_Color cursor_color = { 80, 140, 230, 200 };
    SDL_Color btn_bg = { 55, 55, 65, 255 };
    SDL_Color btn_text = { 200, 200, 210, 255 };
    SDL_Color modified_color = { 230, 170, 60, 255 };

    /* Toolbar */
    SDL_Rect toolbar = { area.x, area.y, area.w, TOOLBAR_H };
    SDL_SetRenderDrawColor(renderer, toolbar_bg.r, toolbar_bg.g,
                           toolbar_bg.b, 255);
    SDL_RenderFillRect(renderer, &toolbar);

    /* Toolbar buttons */
    int bx = area.x + BTN_GAP;
    int by = area.y + (TOOLBAR_H - BTN_H) / 2;

    e->btn_new = (SDL_Rect){ bx, by, BTN_W, BTN_H };
    SDL_SetRenderDrawColor(renderer, btn_bg.r, btn_bg.g, btn_bg.b, 255);
    SDL_RenderFillRect(renderer, &e->btn_new);
    render_text(renderer, e->font_ui, "New", bx + 14, by + 4, btn_text);
    bx += BTN_W + BTN_GAP;

    e->btn_open = (SDL_Rect){ bx, by, BTN_W, BTN_H };
    SDL_SetRenderDrawColor(renderer, btn_bg.r, btn_bg.g, btn_bg.b, 255);
    SDL_RenderFillRect(renderer, &e->btn_open);
    render_text(renderer, e->font_ui, "Open", bx + 10, by + 4, btn_text);
    bx += BTN_W + BTN_GAP;

    e->btn_save = (SDL_Rect){ bx, by, BTN_W, BTN_H };
    SDL_SetRenderDrawColor(renderer, btn_bg.r, btn_bg.g, btn_bg.b, 255);
    SDL_RenderFillRect(renderer, &e->btn_save);
    render_text(renderer, e->font_ui, "Save", bx + 10, by + 4, btn_text);
    bx += BTN_W + BTN_GAP + 10;

    /* File name / modified indicator */
    if (e->has_file) {
        const char *name = strrchr(e->filepath, '/');
        name = name ? name + 1 : e->filepath;
        render_text(renderer, e->font_ui, name, bx, by + 4,
                    e->modified ? modified_color : btn_text);
    } else {
        render_text(renderer, e->font_ui, "Untitled",
                    bx, by + 4, btn_text);
    }

    /* Toolbar bottom border */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 60, 60, 70, 200);
    SDL_RenderDrawLine(renderer, area.x, area.y + TOOLBAR_H,
                       area.x + area.w, area.y + TOOLBAR_H);

    /* Editor area */
    SDL_Rect edit_area = {
        area.x, area.y + TOOLBAR_H,
        area.w, area.h - TOOLBAR_H
    };

    /* Background */
    SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g,
                           bg_color.b, 255);
    SDL_RenderFillRect(renderer, &edit_area);

    /* Gutter */
    SDL_Rect gutter = { edit_area.x, edit_area.y, GUTTER_W, edit_area.h };
    SDL_SetRenderDrawColor(renderer, gutter_bg.r, gutter_bg.g,
                           gutter_bg.b, 255);
    SDL_RenderFillRect(renderer, &gutter);

    /* Gutter right border */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 55, 55, 65, 200);
    SDL_RenderDrawLine(renderer, edit_area.x + GUTTER_W, edit_area.y,
                       edit_area.x + GUTTER_W, edit_area.y + edit_area.h);

    /* Text area dimensions */
    int text_x = edit_area.x + GUTTER_W + LINE_PAD;
    int text_y = edit_area.y + LINE_PAD;
    int vis_rows = (edit_area.h - LINE_PAD * 2) / e->cell_h;

    /* Render visible lines */
    char line_num_buf[16];
    for (int i = 0; i < vis_rows; i++) {
        int row = e->scroll_row + i;
        if (row >= e->num_lines) break;

        int py = text_y + i * e->cell_h;

        /* Current line highlight */
        if (row == e->cursor_row) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 8);
            SDL_Rect hl = { edit_area.x + GUTTER_W, py,
                           edit_area.w - GUTTER_W, e->cell_h };
            SDL_RenderFillRect(renderer, &hl);
        }

        /* Line number */
        snprintf(line_num_buf, sizeof(line_num_buf), "%3d", row + 1);
        render_text(renderer, e->font_mono, line_num_buf,
                    edit_area.x + 4, py, gutter_text);

        /* Line text */
        if (e->lines[row].len > 0) {
            /* Render visible portion */
            const char *line_text = e->lines[row].text;
            int start_col = e->scroll_col;
            if (start_col < e->lines[row].len) {
                render_text(renderer, e->font_mono,
                            &line_text[start_col], text_x, py, text_color);
            }
        }
    }

    /* Cursor */
    Uint32 now = SDL_GetTicks();
    if (now - e->cursor_blink_timer > 530) {
        e->cursor_visible = !e->cursor_visible;
        e->cursor_blink_timer = now;
    }

    if (e->cursor_visible) {
        int vis_row = e->cursor_row - e->scroll_row;
        int vis_col = e->cursor_col - e->scroll_col;
        if (vis_row >= 0 && vis_row < vis_rows && vis_col >= 0) {
            int cx = text_x + vis_col * e->cell_w;
            int cy = text_y + vis_row * e->cell_h;
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, cursor_color.r, cursor_color.g,
                                   cursor_color.b, cursor_color.a);
            SDL_Rect crect = { cx, cy, 2, e->cell_h };
            SDL_RenderFillRect(renderer, &crect);
        }
    }

    /* Save dialog overlay */
    if (e->show_save_dialog) {
        /* Dim background */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
        SDL_RenderFillRect(renderer, &area);

        /* Dialog box */
        int dw = 400, dh = 120;
        SDL_Rect dialog = {
            area.x + (area.w - dw) / 2,
            area.y + (area.h - dh) / 2,
            dw, dh
        };
        SDL_SetRenderDrawColor(renderer, 45, 45, 55, 240);
        SDL_RenderFillRect(renderer, &dialog);
        SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
        SDL_RenderDrawRect(renderer, &dialog);

        render_text(renderer, e->font_ui, "Save as:",
                    dialog.x + 16, dialog.y + 12, btn_text);

        /* Input field */
        SDL_Rect input = { dialog.x + 16, dialog.y + 40,
                          dw - 32, 28 };
        SDL_SetRenderDrawColor(renderer, 30, 30, 38, 255);
        SDL_RenderFillRect(renderer, &input);
        SDL_SetRenderDrawColor(renderer, 80, 140, 230, 200);
        SDL_RenderDrawRect(renderer, &input);

        if (e->save_input_len > 0) {
            render_text(renderer, e->font_mono, e->save_input,
                        input.x + 4, input.y + 4, text_color);
        }

        render_text(renderer, e->font_ui, "Press Enter to save, Esc to cancel",
                    dialog.x + 16, dialog.y + 80, gutter_text);
    }
}

/* (mouse event handling is merged into editor_handle_combined_event below) */

static void editor_handle_key_cb(void *ctx, const SDL_Event *event) {
    editor_ctx_t *e = (editor_ctx_t *)ctx;
    if (!e || event->type != SDL_KEYDOWN) return;

    const SDL_Keysym *key = &event->key.keysym;

    /* Save dialog input */
    if (e->show_save_dialog) {
        if (key->sym == SDLK_ESCAPE) {
            e->show_save_dialog = false;
        } else if (key->sym == SDLK_RETURN) {
            if (e->save_input_len > 0) {
                snprintf(e->filepath, sizeof(e->filepath), "%s", e->save_input);
                editor_save_file(e);
                e->show_save_dialog = false;
            }
        } else if (key->sym == SDLK_BACKSPACE) {
            if (e->save_input_len > 0) {
                e->save_input[--e->save_input_len] = '\0';
            }
        }
        return;
    }

    /* Ctrl shortcuts */
    if (key->mod & KMOD_CTRL) {
        if (key->sym == SDLK_s) {
            if (e->has_file) {
                editor_save_file(e);
            } else {
                e->show_save_dialog = true;
                e->save_input_len = 0;
                e->save_input[0] = '\0';
            }
            return;
        }
        if (key->sym == SDLK_n) {
            editor_new_file(e);
            return;
        }
        return;
    }

    /* Navigation */
    if (key->sym == SDLK_UP) {
        if (e->cursor_row > 0) {
            e->cursor_row--;
            if (e->cursor_col > e->lines[e->cursor_row].len)
                e->cursor_col = e->lines[e->cursor_row].len;
        }
    } else if (key->sym == SDLK_DOWN) {
        if (e->cursor_row < e->num_lines - 1) {
            e->cursor_row++;
            if (e->cursor_col > e->lines[e->cursor_row].len)
                e->cursor_col = e->lines[e->cursor_row].len;
        }
    } else if (key->sym == SDLK_LEFT) {
        if (e->cursor_col > 0) {
            e->cursor_col--;
        } else if (e->cursor_row > 0) {
            e->cursor_row--;
            e->cursor_col = e->lines[e->cursor_row].len;
        }
    } else if (key->sym == SDLK_RIGHT) {
        if (e->cursor_col < e->lines[e->cursor_row].len) {
            e->cursor_col++;
        } else if (e->cursor_row < e->num_lines - 1) {
            e->cursor_row++;
            e->cursor_col = 0;
        }
    } else if (key->sym == SDLK_HOME) {
        e->cursor_col = 0;
    } else if (key->sym == SDLK_END) {
        e->cursor_col = e->lines[e->cursor_row].len;
    } else if (key->sym == SDLK_PAGEUP) {
        e->cursor_row -= 20;
        if (e->cursor_row < 0) e->cursor_row = 0;
        if (e->cursor_col > e->lines[e->cursor_row].len)
            e->cursor_col = e->lines[e->cursor_row].len;
    } else if (key->sym == SDLK_PAGEDOWN) {
        e->cursor_row += 20;
        if (e->cursor_row >= e->num_lines) e->cursor_row = e->num_lines - 1;
        if (e->cursor_col > e->lines[e->cursor_row].len)
            e->cursor_col = e->lines[e->cursor_row].len;
    } else if (key->sym == SDLK_RETURN) {
        /* Split line at cursor */
        editor_line_t *cur = &e->lines[e->cursor_row];
        insert_line(e, e->cursor_row + 1);
        editor_line_t *new_line = &e->lines[e->cursor_row + 1];

        /* Move text after cursor to new line */
        int remaining = cur->len - e->cursor_col;
        if (remaining > 0) {
            line_ensure_cap(new_line, remaining);
            memcpy(new_line->text, &cur->text[e->cursor_col],
                   (size_t)remaining);
            new_line->len = remaining;
            new_line->text[remaining] = '\0';
            cur->len = e->cursor_col;
            cur->text[cur->len] = '\0';
        }

        e->cursor_row++;
        e->cursor_col = 0;
        e->modified = true;
    } else if (key->sym == SDLK_BACKSPACE) {
        if (e->cursor_col > 0) {
            line_delete_char(&e->lines[e->cursor_row], e->cursor_col - 1);
            e->cursor_col--;
            e->modified = true;
        } else if (e->cursor_row > 0) {
            /* Merge with previous line */
            int prev_len = e->lines[e->cursor_row - 1].len;
            editor_line_t *prev = &e->lines[e->cursor_row - 1];
            editor_line_t *cur = &e->lines[e->cursor_row];

            line_ensure_cap(prev, prev->len + cur->len);
            memcpy(&prev->text[prev->len], cur->text, (size_t)cur->len);
            prev->len += cur->len;
            prev->text[prev->len] = '\0';

            delete_line(e, e->cursor_row);
            e->cursor_row--;
            e->cursor_col = prev_len;
            e->modified = true;
        }
    } else if (key->sym == SDLK_DELETE) {
        editor_line_t *cur = &e->lines[e->cursor_row];
        if (e->cursor_col < cur->len) {
            line_delete_char(cur, e->cursor_col);
            e->modified = true;
        } else if (e->cursor_row < e->num_lines - 1) {
            /* Merge next line into current */
            editor_line_t *next = &e->lines[e->cursor_row + 1];
            line_ensure_cap(cur, cur->len + next->len);
            memcpy(&cur->text[cur->len], next->text, (size_t)next->len);
            cur->len += next->len;
            cur->text[cur->len] = '\0';
            delete_line(e, e->cursor_row + 1);
            e->modified = true;
        }
    } else if (key->sym == SDLK_TAB) {
        for (int i = 0; i < EDITOR_TAB_SIZE; i++) {
            line_insert_char(&e->lines[e->cursor_row], e->cursor_col, ' ');
            e->cursor_col++;
        }
        e->modified = true;
    }

    /* Ensure cursor visible (scroll) */
    int vis_rows_approx = 30; /* rough estimate, actual computed at render */
    if (e->cursor_row < e->scroll_row) {
        e->scroll_row = e->cursor_row;
    }
    if (e->cursor_row >= e->scroll_row + vis_rows_approx) {
        e->scroll_row = e->cursor_row - vis_rows_approx + 1;
    }

    e->cursor_visible = true;
    e->cursor_blink_timer = SDL_GetTicks();
}

static void editor_update_cb(void *ctx) {
    (void)ctx;
}

static void editor_destroy_cb(void *ctx) {
    editor_ctx_t *e = (editor_ctx_t *)ctx;
    if (!e) return;
    for (int i = 0; i < e->num_lines; i++) {
        line_free(&e->lines[i]);
    }
    free(e->lines);
    free(e);
}

/* ---- SDL_TEXTINPUT handler for editor ----------------------------------- */
/* This is called from the desktop event loop for text input events */

static void editor_text_input(editor_ctx_t *e, const char *text) {
    if (e->show_save_dialog) {
        /* Append to save input */
        int len = (int)strlen(text);
        for (int i = 0; i < len && e->save_input_len < 1022; i++) {
            e->save_input[e->save_input_len++] = text[i];
        }
        e->save_input[e->save_input_len] = '\0';
        return;
    }

    /* Insert characters at cursor */
    int len = (int)strlen(text);
    for (int i = 0; i < len; i++) {
        char ch = text[i];
        if (ch >= 32 && ch < 127) {
            line_insert_char(&e->lines[e->cursor_row], e->cursor_col, ch);
            e->cursor_col++;
            e->modified = true;
        }
    }
    e->cursor_visible = true;
    e->cursor_blink_timer = SDL_GetTicks();
}

/* Combined event handler: mouse clicks + text input */
static void editor_handle_combined_event(void *ctx, const SDL_Event *event,
                                         SDL_Rect area) {
    editor_ctx_t *e = (editor_ctx_t *)ctx;
    if (!e) return;

    if (event->type == SDL_TEXTINPUT) {
        editor_text_input(e, event->text.text);
        return;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;

        /* Toolbar button clicks */
        if (mx >= e->btn_new.x && mx < e->btn_new.x + e->btn_new.w &&
            my >= e->btn_new.y && my < e->btn_new.y + e->btn_new.h) {
            editor_new_file(e);
            return;
        }
        if (mx >= e->btn_open.x && mx < e->btn_open.x + e->btn_open.w &&
            my >= e->btn_open.y && my < e->btn_open.y + e->btn_open.h) {
            editor_load_file(e, "/etc/hostname");
            return;
        }
        if (mx >= e->btn_save.x && mx < e->btn_save.x + e->btn_save.w &&
            my >= e->btn_save.y && my < e->btn_save.y + e->btn_save.h) {
            if (e->has_file) {
                editor_save_file(e);
            } else {
                e->show_save_dialog = true;
                e->save_input_len = 0;
                e->save_input[0] = '\0';
            }
            return;
        }

        /* Click in text area to position cursor */
        int text_x = area.x + GUTTER_W + LINE_PAD;
        int text_y = area.y + TOOLBAR_H + LINE_PAD;
        if (mx >= text_x && my >= text_y) {
            int row = (my - text_y) / e->cell_h + e->scroll_row;
            int col = (mx - text_x) / e->cell_w + e->scroll_col;
            if (row >= e->num_lines) row = e->num_lines - 1;
            if (row < 0) row = 0;
            if (col < 0) col = 0;
            if (col > e->lines[row].len) col = e->lines[row].len;
            e->cursor_row = row;
            e->cursor_col = col;
            e->cursor_visible = true;
            e->cursor_blink_timer = SDL_GetTicks();
        }
    }
}

/* ---- Public API --------------------------------------------------------- */

app_callbacks_t app_editor_create(TTF_Font *mono_font, TTF_Font *ui_font) {
    app_callbacks_t cb = { NULL, NULL, NULL, NULL, NULL, NULL };

    editor_ctx_t *e = calloc(1, sizeof(editor_ctx_t));
    if (!e) return cb;

    e->font_mono = mono_font;
    e->font_ui = ui_font;
    e->cursor_visible = true;
    e->cursor_blink_timer = SDL_GetTicks();

    TTF_SizeText(mono_font, "M", &e->cell_w, &e->cell_h);

    /* Initial document */
    e->lines_cap = 256;
    e->lines = calloc((size_t)e->lines_cap, sizeof(editor_line_t));
    e->num_lines = 1;
    line_init(&e->lines[0]);

    cb.ctx = e;
    cb.render = editor_render_cb;
    cb.handle_event = editor_handle_combined_event;
    cb.handle_key = editor_handle_key_cb;
    cb.update = editor_update_cb;
    cb.destroy = editor_destroy_cb;
    return cb;
}
