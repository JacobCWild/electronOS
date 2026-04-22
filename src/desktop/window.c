/* ===========================================================================
 * electronOS Desktop — Window Management Implementation
 * ===========================================================================
 * Handles window creation, rendering (glass titlebar with traffic-light
 * buttons), dragging, resizing, minimize, maximize, and close.
 * =========================================================================== */

#include "window.h"
#include "glass.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- Traffic-light button colors ---------------------------------------- */
#define BTN_CLOSE_R    235
#define BTN_CLOSE_G     65
#define BTN_CLOSE_B     55
#define BTN_MINIMIZE_R 245
#define BTN_MINIMIZE_G 190
#define BTN_MINIMIZE_B  50
#define BTN_MAXIMIZE_R  60
#define BTN_MAXIMIZE_G 195
#define BTN_MAXIMIZE_B  70
#define BTN_INACTIVE_R  80
#define BTN_INACTIVE_G  80
#define BTN_INACTIVE_B  80

static int s_next_id = 1;

/* ---- Helpers ------------------------------------------------------------ */

static void fill_circle(SDL_Renderer *renderer, int cx, int cy, int r) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                        const char *text, int x, int y, SDL_Color color) {
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

/* ---- Button hit-test rects --------------------------------------------- */

static SDL_Rect close_btn_rect(const desktop_window_t *win) {
    return (SDL_Rect){
        win->rect.x + WINDOW_BTN_MARGIN,
        win->rect.y + (TITLEBAR_HEIGHT - WINDOW_BTN_SIZE) / 2,
        WINDOW_BTN_SIZE, WINDOW_BTN_SIZE
    };
}

static SDL_Rect minimize_btn_rect(const desktop_window_t *win) {
    return (SDL_Rect){
        win->rect.x + WINDOW_BTN_MARGIN + WINDOW_BTN_SIZE + 6,
        win->rect.y + (TITLEBAR_HEIGHT - WINDOW_BTN_SIZE) / 2,
        WINDOW_BTN_SIZE, WINDOW_BTN_SIZE
    };
}

static SDL_Rect maximize_btn_rect(const desktop_window_t *win) {
    return (SDL_Rect){
        win->rect.x + WINDOW_BTN_MARGIN + (WINDOW_BTN_SIZE + 6) * 2,
        win->rect.y + (TITLEBAR_HEIGHT - WINDOW_BTN_SIZE) / 2,
        WINDOW_BTN_SIZE, WINDOW_BTN_SIZE
    };
}

static bool point_in_circle(int px, int py, int cx, int cy, int r) {
    int dx = px - cx;
    int dy = py - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

/* ---- API ---------------------------------------------------------------- */

desktop_window_t *window_create(const char *title, app_type_t type,
                                int x, int y, int w, int h,
                                app_callbacks_t app) {
    desktop_window_t *win = calloc(1, sizeof(desktop_window_t));
    if (!win) return NULL;

    win->id = s_next_id++;
    strncpy(win->title, title, sizeof(win->title) - 1);
    win->app_type = type;
    win->rect = (SDL_Rect){ x, y, w, h };
    win->restore_rect = win->rect;
    win->app = app;
    win->focused = true;
    return win;
}

void window_render(SDL_Renderer *renderer, desktop_window_t *win,
                   TTF_Font *font) {
    if (win->minimized) return;

    glass_style_t style = win->focused ? GLASS_WINDOW_ACTIVE : GLASS_WINDOW;

    /* Titlebar glass background */
    SDL_Rect titlebar = {
        win->rect.x, win->rect.y, win->rect.w, TITLEBAR_HEIGHT
    };
    glass_render(renderer, &titlebar, style);

    /* Content area background (dark) */
    SDL_Rect content = window_content_rect(win);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 28, 28, 32, 245);
    SDL_RenderFillRect(renderer, &content);

    /* Window border */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                           win->focused ? 40 : 20);
    SDL_RenderDrawRect(renderer, &win->rect);

    /* Traffic-light buttons */
    SDL_Rect cb = close_btn_rect(win);
    SDL_Rect mb = minimize_btn_rect(win);
    SDL_Rect xb = maximize_btn_rect(win);

    int cr = WINDOW_BTN_SIZE / 2;

    if (win->focused) {
        SDL_SetRenderDrawColor(renderer, BTN_CLOSE_R, BTN_CLOSE_G,
                               BTN_CLOSE_B, 255);
        fill_circle(renderer, cb.x + cr, cb.y + cr, cr);

        SDL_SetRenderDrawColor(renderer, BTN_MINIMIZE_R, BTN_MINIMIZE_G,
                               BTN_MINIMIZE_B, 255);
        fill_circle(renderer, mb.x + cr, mb.y + cr, cr);

        SDL_SetRenderDrawColor(renderer, BTN_MAXIMIZE_R, BTN_MAXIMIZE_G,
                               BTN_MAXIMIZE_B, 255);
        fill_circle(renderer, xb.x + cr, xb.y + cr, cr);
    } else {
        SDL_SetRenderDrawColor(renderer, BTN_INACTIVE_R, BTN_INACTIVE_G,
                               BTN_INACTIVE_B, 255);
        fill_circle(renderer, cb.x + cr, cb.y + cr, cr);
        fill_circle(renderer, mb.x + cr, mb.y + cr, cr);
        fill_circle(renderer, xb.x + cr, xb.y + cr, cr);
    }

    /* Title text (centered in titlebar, after buttons) */
    if (font) {
        int tw, th;
        TTF_SizeText(font, win->title, &tw, &th);
        int title_x = win->rect.x + (win->rect.w - tw) / 2;
        int title_y = win->rect.y + (TITLEBAR_HEIGHT - th) / 2;
        SDL_Color title_color = { 210, 210, 215, 255 };
        render_text(renderer, font, win->title, title_x, title_y,
                    title_color);
    }

    /* Render app content */
    if (win->app.render) {
        win->app.render(win->app.ctx, renderer, content);
    }
}

bool window_handle_event(desktop_window_t *win, const SDL_Event *event,
                         int screen_w, int screen_h) {
    if (win->minimized) return false;

    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;

        /* Check titlebar buttons */
        int cr = WINDOW_BTN_SIZE / 2;
        SDL_Rect cb = close_btn_rect(win);
        if (point_in_circle(mx, my, cb.x + cr, cb.y + cr, cr + 2)) {
            return true;  /* Close handled by desktop */
        }

        SDL_Rect mb = minimize_btn_rect(win);
        if (point_in_circle(mx, my, mb.x + cr, mb.y + cr, cr + 2)) {
            win->minimized = true;
            return true;
        }

        SDL_Rect xb = maximize_btn_rect(win);
        if (point_in_circle(mx, my, xb.x + cr, xb.y + cr, cr + 2)) {
            return true;  /* Maximize handled by desktop */
        }

        /* Check titlebar drag */
        if (my >= win->rect.y && my < win->rect.y + TITLEBAR_HEIGHT &&
            mx >= win->rect.x && mx < win->rect.x + win->rect.w) {
            win->dragging = true;
            win->drag_offset_x = mx - win->rect.x;
            win->drag_offset_y = my - win->rect.y;
            return true;
        }

        /* Check resize edges */
        if (window_contains_point(win, mx, my)) {
            int edge = 0;
            if (mx < win->rect.x + RESIZE_BORDER) edge |= 1;
            if (mx > win->rect.x + win->rect.w - RESIZE_BORDER) edge |= 2;
            if (my > win->rect.y + win->rect.h - RESIZE_BORDER) edge |= 8;

            if (edge) {
                win->resizing = true;
                win->resize_edge = edge;
                return true;
            }
        }
    }

    if (event->type == SDL_MOUSEBUTTONUP) {
        if (win->dragging || win->resizing) {
            win->dragging = false;
            win->resizing = false;
            return true;
        }
    }

    if (event->type == SDL_MOUSEMOTION) {
        int mx = event->motion.x;
        int my = event->motion.y;

        if (win->dragging) {
            win->rect.x = mx - win->drag_offset_x;
            win->rect.y = my - win->drag_offset_y;

            /* Clamp to screen */
            if (win->rect.y < 0) win->rect.y = 0;
            if (win->rect.x + win->rect.w < 80)
                win->rect.x = 80 - win->rect.w;
            if (win->rect.x > screen_w - 40)
                win->rect.x = screen_w - 40;
            if (win->rect.y + win->rect.h > screen_h)
                win->rect.y = screen_h - win->rect.h;
            return true;
        }

        if (win->resizing) {
            if (win->resize_edge & 2) {
                int new_w = mx - win->rect.x;
                if (new_w >= WINDOW_MIN_W) win->rect.w = new_w;
            }
            if (win->resize_edge & 1) {
                int dx = mx - win->rect.x;
                if (win->rect.w - dx >= WINDOW_MIN_W) {
                    win->rect.x += dx;
                    win->rect.w -= dx;
                }
            }
            if (win->resize_edge & 8) {
                int new_h = my - win->rect.y;
                if (new_h >= WINDOW_MIN_H) win->rect.h = new_h;
            }
            return true;
        }
    }

    return false;
}

SDL_Rect window_content_rect(const desktop_window_t *win) {
    return (SDL_Rect){
        win->rect.x + WINDOW_BORDER,
        win->rect.y + TITLEBAR_HEIGHT,
        win->rect.w - 2 * WINDOW_BORDER,
        win->rect.h - TITLEBAR_HEIGHT - WINDOW_BORDER,
    };
}

bool window_contains_point(const desktop_window_t *win, int x, int y) {
    return x >= win->rect.x && x < win->rect.x + win->rect.w &&
           y >= win->rect.y && y < win->rect.y + win->rect.h;
}

bool window_close_hit(const desktop_window_t *win, int x, int y) {
    SDL_Rect cb = close_btn_rect(win);
    int cr = WINDOW_BTN_SIZE / 2;
    return point_in_circle(x, y, cb.x + cr, cb.y + cr, cr + 2);
}

bool window_minimize_hit(const desktop_window_t *win, int x, int y) {
    SDL_Rect mb = minimize_btn_rect(win);
    int cr = WINDOW_BTN_SIZE / 2;
    return point_in_circle(x, y, mb.x + cr, mb.y + cr, cr + 2);
}

bool window_maximize_hit(const desktop_window_t *win, int x, int y) {
    SDL_Rect xb = maximize_btn_rect(win);
    int cr = WINDOW_BTN_SIZE / 2;
    return point_in_circle(x, y, xb.x + cr, xb.y + cr, cr + 2);
}

void window_toggle_maximize(desktop_window_t *win, int screen_w, int screen_h,
                            int panel_h, int dock_w) {
    if (win->maximized) {
        win->rect = win->restore_rect;
        win->maximized = false;
    } else {
        win->restore_rect = win->rect;
        win->rect.x = dock_w;
        win->rect.y = panel_h;
        win->rect.w = screen_w - dock_w;
        win->rect.h = screen_h - panel_h;
        win->maximized = true;
    }
}

void window_destroy(desktop_window_t *win) {
    if (!win) return;
    if (win->app.destroy) {
        win->app.destroy(win->app.ctx);
    }
    free(win);
}
