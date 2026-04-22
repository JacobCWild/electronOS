/* ===========================================================================
 * electronOS Desktop — Terminal App Implementation
 * ===========================================================================
 * PTY-based terminal emulator that runs electronos-shell (or /bin/sh).
 * Implements a basic VT100/ANSI parser for colors, cursor movement, and
 * screen clearing. Renders a character grid with a monospace font.
 * =========================================================================== */

#include "app_terminal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <pwd.h>

#if defined(__linux__)
#include <pty.h>
#elif defined(__APPLE__)
#include <util.h>
#endif

/* ---- ANSI color palette ------------------------------------------------- */
static const SDL_Color ANSI_COLORS[16] = {
    /* Normal */
    {  30,  30,  35, 255 },  /* 0: Black (bg-ish) */
    { 210,  70,  70, 255 },  /* 1: Red */
    {  70, 200,  80, 255 },  /* 2: Green */
    { 220, 180,  50, 255 },  /* 3: Yellow */
    {  80, 140, 230, 255 },  /* 4: Blue */
    { 180,  90, 210, 255 },  /* 5: Magenta */
    {  70, 200, 200, 255 },  /* 6: Cyan */
    { 210, 210, 215, 255 },  /* 7: White */
    /* Bright */
    {  90,  90,  95, 255 },  /* 8: Bright black */
    { 240, 100, 100, 255 },  /* 9: Bright red */
    { 100, 240, 110, 255 },  /* 10: Bright green */
    { 250, 220,  80, 255 },  /* 11: Bright yellow */
    { 110, 170, 250, 255 },  /* 12: Bright blue */
    { 210, 120, 240, 255 },  /* 13: Bright magenta */
    { 100, 230, 240, 255 },  /* 14: Bright cyan */
    { 240, 240, 245, 255 },  /* 15: Bright white */
};

/* ---- Forward declarations ----------------------------------------------- */
static void terminal_render_cb(void *ctx, SDL_Renderer *renderer,
                               SDL_Rect content_area);
static void terminal_handle_event_cb(void *ctx, const SDL_Event *event,
                                     SDL_Rect content_area);
static void terminal_handle_key_cb(void *ctx, const SDL_Event *event);
static void terminal_update_cb(void *ctx);
static void terminal_destroy_cb(void *ctx);

/* ---- Grid helpers ------------------------------------------------------- */

static void grid_clear(terminal_ctx_t *t) {
    for (int i = 0; i < t->rows * t->cols; i++) {
        t->cells[i] = (term_cell_t){ ' ', 7, 0, false };
    }
    t->cursor_row = 0;
    t->cursor_col = 0;
}

static void grid_resize(terminal_ctx_t *t, int new_rows, int new_cols) {
    term_cell_t *new_cells = calloc((size_t)new_rows * (size_t)new_cols,
                                    sizeof(term_cell_t));
    if (!new_cells) return;

    /* Initialize new cells */
    for (int i = 0; i < new_rows * new_cols; i++) {
        new_cells[i] = (term_cell_t){ ' ', 7, 0, false };
    }

    /* Copy old content */
    if (t->cells) {
        int copy_rows = t->rows < new_rows ? t->rows : new_rows;
        int copy_cols = t->cols < new_cols ? t->cols : new_cols;
        for (int r = 0; r < copy_rows; r++) {
            for (int c = 0; c < copy_cols; c++) {
                new_cells[r * new_cols + c] = t->cells[r * t->cols + c];
            }
        }
        free(t->cells);
    }

    t->cells = new_cells;
    t->rows = new_rows;
    t->cols = new_cols;
    t->scroll_top = 0;
    t->scroll_bottom = new_rows - 1;

    if (t->cursor_row >= new_rows) t->cursor_row = new_rows - 1;
    if (t->cursor_col >= new_cols) t->cursor_col = new_cols - 1;
}

static void scroll_up(terminal_ctx_t *t) {
    int top = t->scroll_top;
    int bot = t->scroll_bottom;

    /* Move rows up by one in the scroll region */
    for (int r = top; r < bot; r++) {
        memcpy(&t->cells[r * t->cols],
               &t->cells[(r + 1) * t->cols],
               (size_t)t->cols * sizeof(term_cell_t));
    }

    /* Clear bottom row */
    for (int c = 0; c < t->cols; c++) {
        t->cells[bot * t->cols + c] = (term_cell_t){ ' ', 7, 0, false };
    }
}

/* ---- VT100 Parser ------------------------------------------------------- */

static void process_csi(terminal_ctx_t *t) {
    /* Parse CSI parameters: ESC [ Pn ; Pn ... final_char */
    int params[16] = {0};
    int nparam = 0;
    int i = 0;

    while (i < t->csi_len && nparam < 16) {
        if (t->csi_buf[i] >= '0' && t->csi_buf[i] <= '9') {
            params[nparam] = params[nparam] * 10 + (t->csi_buf[i] - '0');
            i++;
        } else if (t->csi_buf[i] == ';') {
            nparam++;
            i++;
        } else {
            break;
        }
    }
    if (i < t->csi_len || nparam < 16) nparam++;

    if (t->csi_len == 0) return;
    char cmd = t->csi_buf[t->csi_len - 1];

    switch (cmd) {
    case 'A': { /* Cursor up */
        int n = params[0] > 0 ? params[0] : 1;
        t->cursor_row -= n;
        if (t->cursor_row < 0) t->cursor_row = 0;
        break;
    }
    case 'B': { /* Cursor down */
        int n = params[0] > 0 ? params[0] : 1;
        t->cursor_row += n;
        if (t->cursor_row >= t->rows) t->cursor_row = t->rows - 1;
        break;
    }
    case 'C': { /* Cursor forward */
        int n = params[0] > 0 ? params[0] : 1;
        t->cursor_col += n;
        if (t->cursor_col >= t->cols) t->cursor_col = t->cols - 1;
        break;
    }
    case 'D': { /* Cursor backward */
        int n = params[0] > 0 ? params[0] : 1;
        t->cursor_col -= n;
        if (t->cursor_col < 0) t->cursor_col = 0;
        break;
    }
    case 'H':
    case 'f': { /* Cursor position */
        int row = params[0] > 0 ? params[0] - 1 : 0;
        int col = (nparam > 1 && params[1] > 0) ? params[1] - 1 : 0;
        if (row >= t->rows) row = t->rows - 1;
        if (col >= t->cols) col = t->cols - 1;
        t->cursor_row = row;
        t->cursor_col = col;
        break;
    }
    case 'J': { /* Erase display */
        int mode = params[0];
        if (mode == 0) {
            /* Clear from cursor to end */
            for (int c = t->cursor_col; c < t->cols; c++)
                t->cells[t->cursor_row * t->cols + c] =
                    (term_cell_t){ ' ', 7, 0, false };
            for (int r = t->cursor_row + 1; r < t->rows; r++)
                for (int c = 0; c < t->cols; c++)
                    t->cells[r * t->cols + c] =
                        (term_cell_t){ ' ', 7, 0, false };
        } else if (mode == 1) {
            /* Clear from start to cursor */
            for (int r = 0; r < t->cursor_row; r++)
                for (int c = 0; c < t->cols; c++)
                    t->cells[r * t->cols + c] =
                        (term_cell_t){ ' ', 7, 0, false };
            for (int c = 0; c <= t->cursor_col; c++)
                t->cells[t->cursor_row * t->cols + c] =
                    (term_cell_t){ ' ', 7, 0, false };
        } else if (mode == 2 || mode == 3) {
            grid_clear(t);
        }
        break;
    }
    case 'K': { /* Erase line */
        int mode = params[0];
        int row = t->cursor_row;
        if (mode == 0) {
            for (int c = t->cursor_col; c < t->cols; c++)
                t->cells[row * t->cols + c] =
                    (term_cell_t){ ' ', 7, 0, false };
        } else if (mode == 1) {
            for (int c = 0; c <= t->cursor_col; c++)
                t->cells[row * t->cols + c] =
                    (term_cell_t){ ' ', 7, 0, false };
        } else if (mode == 2) {
            for (int c = 0; c < t->cols; c++)
                t->cells[row * t->cols + c] =
                    (term_cell_t){ ' ', 7, 0, false };
        }
        break;
    }
    case 'G': { /* Cursor horizontal absolute */
        int col = params[0] > 0 ? params[0] - 1 : 0;
        if (col >= t->cols) col = t->cols - 1;
        t->cursor_col = col;
        break;
    }
    case 'P': { /* Delete characters */
        int n = params[0] > 0 ? params[0] : 1;
        int row = t->cursor_row;
        int col = t->cursor_col;
        for (int c = col; c < t->cols; c++) {
            int src = c + n;
            if (src < t->cols)
                t->cells[row * t->cols + c] = t->cells[row * t->cols + src];
            else
                t->cells[row * t->cols + c] =
                    (term_cell_t){ ' ', 7, 0, false };
        }
        break;
    }
    case '@': { /* Insert characters */
        int n = params[0] > 0 ? params[0] : 1;
        int row = t->cursor_row;
        for (int c = t->cols - 1; c >= t->cursor_col + n; c--) {
            t->cells[row * t->cols + c] =
                t->cells[row * t->cols + c - n];
        }
        for (int c = t->cursor_col; c < t->cursor_col + n && c < t->cols; c++) {
            t->cells[row * t->cols + c] = (term_cell_t){ ' ', 7, 0, false };
        }
        break;
    }
    case 'r': { /* Set scroll region */
        int top = params[0] > 0 ? params[0] - 1 : 0;
        int bot = (nparam > 1 && params[1] > 0) ? params[1] - 1 : t->rows - 1;
        if (top < 0) top = 0;
        if (bot >= t->rows) bot = t->rows - 1;
        if (top < bot) {
            t->scroll_top = top;
            t->scroll_bottom = bot;
        }
        t->cursor_row = 0;
        t->cursor_col = 0;
        break;
    }
    case 'm': { /* SGR — Set Graphics Rendition */
        for (int p = 0; p < nparam; p++) {
            int v = params[p];
            if (v == 0) {
                t->cur_fg = 7;
                t->cur_bg = 0;
                t->cur_bold = false;
            } else if (v == 1) {
                t->cur_bold = true;
            } else if (v == 22) {
                t->cur_bold = false;
            } else if (v >= 30 && v <= 37) {
                t->cur_fg = (uint8_t)(v - 30);
            } else if (v == 39) {
                t->cur_fg = 7;
            } else if (v >= 40 && v <= 47) {
                t->cur_bg = (uint8_t)(v - 40);
            } else if (v == 49) {
                t->cur_bg = 0;
            } else if (v >= 90 && v <= 97) {
                t->cur_fg = (uint8_t)(v - 90 + 8);
            } else if (v >= 100 && v <= 107) {
                t->cur_bg = (uint8_t)(v - 100 + 8);
            }
        }
        break;
    }
    case 'L': { /* Insert lines */
        int n = params[0] > 0 ? params[0] : 1;
        for (int k = 0; k < n; k++) {
            for (int r = t->scroll_bottom; r > t->cursor_row; r--) {
                memcpy(&t->cells[r * t->cols],
                       &t->cells[(r - 1) * t->cols],
                       (size_t)t->cols * sizeof(term_cell_t));
            }
            for (int c = 0; c < t->cols; c++) {
                t->cells[t->cursor_row * t->cols + c] =
                    (term_cell_t){ ' ', 7, 0, false };
            }
        }
        break;
    }
    case 'M': { /* Delete lines */
        int n = params[0] > 0 ? params[0] : 1;
        for (int k = 0; k < n; k++) {
            for (int r = t->cursor_row; r < t->scroll_bottom; r++) {
                memcpy(&t->cells[r * t->cols],
                       &t->cells[(r + 1) * t->cols],
                       (size_t)t->cols * sizeof(term_cell_t));
            }
            for (int c = 0; c < t->cols; c++) {
                t->cells[t->scroll_bottom * t->cols + c] =
                    (term_cell_t){ ' ', 7, 0, false };
            }
        }
        break;
    }
    case 'd': { /* Vertical position absolute */
        int row = params[0] > 0 ? params[0] - 1 : 0;
        if (row >= t->rows) row = t->rows - 1;
        t->cursor_row = row;
        break;
    }
    default:
        break;
    }
}

static void process_char(terminal_ctx_t *t, char ch) {
    switch (t->parse_state) {
    case 0: /* Normal */
        if (ch == '\033') {
            t->parse_state = 1;
        } else if (ch == '\n') {
            t->cursor_row++;
            if (t->cursor_row > t->scroll_bottom) {
                t->cursor_row = t->scroll_bottom;
                scroll_up(t);
            }
        } else if (ch == '\r') {
            t->cursor_col = 0;
        } else if (ch == '\b') {
            if (t->cursor_col > 0) t->cursor_col--;
        } else if (ch == '\t') {
            t->cursor_col = (t->cursor_col + 8) & ~7;
            if (t->cursor_col >= t->cols) t->cursor_col = t->cols - 1;
        } else if (ch == '\a') {
            /* Bell — ignore */
        } else if ((unsigned char)ch >= 32) {
            if (t->cursor_col >= t->cols) {
                t->cursor_col = 0;
                t->cursor_row++;
                if (t->cursor_row > t->scroll_bottom) {
                    t->cursor_row = t->scroll_bottom;
                    scroll_up(t);
                }
            }
            int idx = t->cursor_row * t->cols + t->cursor_col;
            t->cells[idx].ch = ch;
            t->cells[idx].fg = t->cur_fg;
            t->cells[idx].bg = t->cur_bg;
            t->cells[idx].bold = t->cur_bold;
            t->cursor_col++;
        }
        break;

    case 1: /* After ESC */
        if (ch == '[') {
            t->parse_state = 2;
            t->csi_len = 0;
        } else if (ch == ']') {
            /* OSC — skip until BEL or ST */
            t->parse_state = 3;
        } else if (ch == '(' || ch == ')') {
            /* Character set designation — skip next char */
            t->parse_state = 4;
        } else {
            t->parse_state = 0;
        }
        break;

    case 2: /* CSI accumulation */
        if (t->csi_len < (int)sizeof(t->csi_buf) - 1) {
            t->csi_buf[t->csi_len++] = ch;
        }
        /* CSI final character is 0x40-0x7E */
        if (ch >= 0x40 && ch <= 0x7E) {
            t->csi_buf[t->csi_len] = '\0';
            process_csi(t);
            t->parse_state = 0;
        }
        break;

    case 3: /* OSC — skip until BEL (\a) or ESC \ */
        if (ch == '\a' || ch == '\\') {
            t->parse_state = 0;
        }
        break;

    case 4: /* Skip one char after ESC ( or ESC ) */
        t->parse_state = 0;
        break;
    }
}

/* ---- Callbacks ---------------------------------------------------------- */

static void terminal_render_cb(void *ctx, SDL_Renderer *renderer,
                               SDL_Rect area) {
    terminal_ctx_t *t = (terminal_ctx_t *)ctx;
    if (!t || !t->font) return;

    /* Background */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 22, 22, 28, 250);
    SDL_RenderFillRect(renderer, &area);

    /* Compute visible grid size */
    int vis_cols = area.w / t->cell_w;
    int vis_rows = area.h / t->cell_h;
    if (vis_cols > t->cols) vis_cols = t->cols;
    if (vis_rows > t->rows) vis_rows = t->rows;

    /* Render cells */
    char ch_buf[2] = { 0, 0 };
    for (int r = 0; r < vis_rows; r++) {
        for (int c = 0; c < vis_cols; c++) {
            term_cell_t *cell = &t->cells[r * t->cols + c];
            int px = area.x + c * t->cell_w;
            int py = area.y + r * t->cell_h;

            /* Background color if non-default */
            if (cell->bg != 0) {
                SDL_Color bg = ANSI_COLORS[cell->bg & 0x0F];
                SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
                SDL_Rect cell_rect = { px, py, t->cell_w, t->cell_h };
                SDL_RenderFillRect(renderer, &cell_rect);
            }

            if (cell->ch > 32) {
                int fg_idx = cell->fg & 0x0F;
                if (cell->bold && fg_idx < 8) fg_idx += 8;
                SDL_Color fg = ANSI_COLORS[fg_idx];

                ch_buf[0] = cell->ch;
                SDL_Surface *surf = TTF_RenderText_Blended(t->font, ch_buf, fg);
                if (surf) {
                    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer,
                                                                     surf);
                    if (tex) {
                        SDL_Rect dst = { px, py, surf->w, surf->h };
                        SDL_RenderCopy(renderer, tex, NULL, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }
        }
    }

    /* Cursor */
    Uint32 now = SDL_GetTicks();
    if (now - t->cursor_blink_timer > 530) {
        t->cursor_visible = !t->cursor_visible;
        t->cursor_blink_timer = now;
    }

    if (t->cursor_visible && t->cursor_row < vis_rows &&
        t->cursor_col < vis_cols) {
        int cx = area.x + t->cursor_col * t->cell_w;
        int cy = area.y + t->cursor_row * t->cell_h;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 200, 200, 210, 180);
        SDL_Rect cursor_rect = { cx, cy, 2, t->cell_h };
        SDL_RenderFillRect(renderer, &cursor_rect);
    }

    /* "Shell exited" overlay */
    if (!t->alive) {
        SDL_Color dim = { 150, 150, 160, 255 };
        SDL_Surface *surf = TTF_RenderText_Blended(t->font,
                                                    "[Shell exited]", dim);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_Rect dst = {
                    area.x + (area.w - surf->w) / 2,
                    area.y + area.h - surf->h - 8,
                    surf->w, surf->h
                };
                SDL_RenderCopy(renderer, tex, NULL, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }
}

static void terminal_handle_event_cb(void *ctx, const SDL_Event *event,
                                     SDL_Rect content_area) {
    (void)ctx;
    (void)event;
    (void)content_area;
    /* Mouse events in terminal area — could be used for selection later */
}

static void terminal_handle_key_cb(void *ctx, const SDL_Event *event) {
    terminal_ctx_t *t = (terminal_ctx_t *)ctx;
    if (!t || !t->alive || t->pty_fd < 0) return;

    if (event->type != SDL_KEYDOWN) return;

    const SDL_Keysym *key = &event->key.keysym;
    char buf[8];
    int len = 0;

    /* Handle special keys */
    if (key->sym == SDLK_RETURN || key->sym == SDLK_KP_ENTER) {
        buf[0] = '\r';
        len = 1;
    } else if (key->sym == SDLK_BACKSPACE) {
        buf[0] = 0x7F;
        len = 1;
    } else if (key->sym == SDLK_TAB) {
        buf[0] = '\t';
        len = 1;
    } else if (key->sym == SDLK_ESCAPE) {
        buf[0] = 0x1B;
        len = 1;
    } else if (key->sym == SDLK_UP) {
        memcpy(buf, "\033[A", 3);
        len = 3;
    } else if (key->sym == SDLK_DOWN) {
        memcpy(buf, "\033[B", 3);
        len = 3;
    } else if (key->sym == SDLK_RIGHT) {
        memcpy(buf, "\033[C", 3);
        len = 3;
    } else if (key->sym == SDLK_LEFT) {
        memcpy(buf, "\033[D", 3);
        len = 3;
    } else if (key->sym == SDLK_HOME) {
        memcpy(buf, "\033[H", 3);
        len = 3;
    } else if (key->sym == SDLK_END) {
        memcpy(buf, "\033[F", 3);
        len = 3;
    } else if (key->sym == SDLK_DELETE) {
        memcpy(buf, "\033[3~", 4);
        len = 4;
    } else if (key->mod & KMOD_CTRL) {
        /* Ctrl+letter */
        if (key->sym >= SDLK_a && key->sym <= SDLK_z) {
            buf[0] = (char)(key->sym - SDLK_a + 1);
            len = 1;
        }
    } else {
        /* Regular character — use text input instead */
        return;
    }

    if (len > 0) {
        ssize_t wr = write(t->pty_fd, buf, (size_t)len);
        (void)wr;
    }
}

static void terminal_update_cb(void *ctx) {
    terminal_ctx_t *t = (terminal_ctx_t *)ctx;
    if (!t || t->pty_fd < 0) return;

    /* Check if child is still alive */
    if (t->child_pid > 0) {
        int status;
        pid_t ret = waitpid(t->child_pid, &status, WNOHANG);
        if (ret > 0) {
            t->alive = false;
            return;
        }
    }

    /* Non-blocking read from PTY */
    fd_set fds;
    struct timeval tv = { 0, 0 };
    FD_ZERO(&fds);
    FD_SET(t->pty_fd, &fds);

    while (select(t->pty_fd + 1, &fds, NULL, NULL, &tv) > 0) {
        char buf[4096];
        ssize_t n = read(t->pty_fd, buf, sizeof(buf));
        if (n <= 0) {
            if (n == 0 || (errno != EAGAIN && errno != EINTR)) {
                t->alive = false;
            }
            break;
        }

        for (ssize_t i = 0; i < n; i++) {
            process_char(t, buf[i]);
        }

        FD_ZERO(&fds);
        FD_SET(t->pty_fd, &fds);
        tv = (struct timeval){ 0, 0 };
    }
}

static void terminal_destroy_cb(void *ctx) {
    terminal_ctx_t *t = (terminal_ctx_t *)ctx;
    if (!t) return;

    if (t->child_pid > 0) {
        kill(t->child_pid, SIGTERM);
        waitpid(t->child_pid, NULL, 0);
    }
    if (t->pty_fd >= 0) {
        close(t->pty_fd);
    }
    free(t->cells);
    free(t);
}

/* ---- Public API --------------------------------------------------------- */

app_callbacks_t app_terminal_create(TTF_Font *mono_font) {
    app_callbacks_t cb = { NULL, NULL, NULL, NULL, NULL, NULL };

    terminal_ctx_t *t = calloc(1, sizeof(terminal_ctx_t));
    if (!t) return cb;

    t->font = mono_font;
    t->cur_fg = 7;
    t->cur_bg = 0;
    t->cur_bold = false;
    t->cursor_visible = true;
    t->cursor_blink_timer = SDL_GetTicks();

    /* Measure cell size */
    TTF_SizeText(mono_font, "M", &t->cell_w, &t->cell_h);

    /* Initial grid size (will resize when window renders) */
    grid_resize(t, 40, 100);

    /* Create PTY and fork shell */
    int master_fd;
    pid_t pid = forkpty(&master_fd, NULL, NULL, NULL);
    if (pid < 0) {
        free(t->cells);
        free(t);
        return cb;
    }

    if (pid == 0) {
        /* Child: exec shell */
        /* Try electronos-shell first, fall back to /bin/sh */
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);

        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            setenv("HOME", pw->pw_dir, 1);
            setenv("USER", pw->pw_name, 1);
            if (chdir(pw->pw_dir) != 0) {
                if (chdir("/") != 0) {
                    /* ignore */
                }
            }
        }

        execlp("electronos-shell", "electronos-shell", (char *)NULL);
        execlp("/bin/sh", "sh", (char *)NULL);
        _exit(1);
    }

    /* Parent */
    t->pty_fd = master_fd;
    t->child_pid = pid;
    t->alive = true;

    /* Set PTY to non-blocking */
    int flags = fcntl(master_fd, F_GETFL, 0);
    fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

    /* Set initial window size */
    struct winsize ws = { .ws_row = (unsigned short)t->rows,
                          .ws_col = (unsigned short)t->cols };
    ioctl(master_fd, TIOCSWINSZ, &ws);

    cb.ctx = t;
    cb.render = terminal_render_cb;
    cb.handle_event = terminal_handle_event_cb;
    cb.handle_key = terminal_handle_key_cb;
    cb.update = terminal_update_cb;
    cb.destroy = terminal_destroy_cb;
    return cb;
}
