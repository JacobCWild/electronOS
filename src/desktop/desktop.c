/* ===========================================================================
 * electronOS Desktop — Desktop Compositor Implementation
 * ===========================================================================
 * Ties together wallpaper, glass effect, top panel, left dock, and window
 * management into a cohesive Ubuntu-layout + Apple Liquid Glass desktop.
 * =========================================================================== */

#include "desktop.h"
#include "wallpaper.h"
#include "glass.h"
#include "panel.h"
#include "dock.h"
#include "window.h"
#include "app_terminal.h"
#include "app_settings.h"

#include <unistd.h>
#include "app_editor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Font paths (try multiple locations) -------------------------------- */
static const char *FONT_PATHS[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    NULL,
};

static const char *MONO_FONT_PATHS[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
    "/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf",
    "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
    NULL,
};

static TTF_Font *try_load_font(const char **paths, int size) {
    for (int i = 0; paths[i]; i++) {
        TTF_Font *f = TTF_OpenFont(paths[i], size);
        if (f) return f;
    }
    return NULL;
}

/* ---- Window z-order helpers --------------------------------------------- */

static void bring_to_front(desktop_t *d, desktop_window_t *win) {
    if (!win || d->windows == win) return;

    /* Remove from list */
    desktop_window_t **pp = &d->windows;
    while (*pp && *pp != win) pp = &(*pp)->next;
    if (*pp) *pp = win->next;

    /* Insert at front */
    win->next = d->windows;
    d->windows = win;
}

static desktop_window_t *find_window_at(desktop_t *d, int x, int y) {
    desktop_window_t *w = d->windows;
    while (w) {
        if (!w->minimized && window_contains_point(w, x, y)) return w;
        w = w->next;
    }
    return NULL;
}

/* ---- Window placement --------------------------------------------------- */

static void next_window_pos(desktop_t *d, int *x, int *y) {
    int base_x = DOCK_WIDTH + 40;
    int base_y = PANEL_HEIGHT + 30;
    int offset = (d->next_window_id % 8) * 28;
    *x = base_x + offset;
    *y = base_y + offset;
}

/* ---- API ---------------------------------------------------------------- */

int desktop_init(desktop_t *d, SDL_Renderer *renderer, int w, int h,
                 bool test_mode) {
    memset(d, 0, sizeof(*d));
    d->renderer = renderer;
    d->screen_w = w;
    d->screen_h = h;
    d->test_mode = test_mode;
    d->next_window_id = 1;

    /* Load fonts */
    d->font_title = try_load_font(FONT_PATHS, 18);
    d->font_label = try_load_font(FONT_PATHS, 13);
    d->font_mono  = try_load_font(MONO_FONT_PATHS, 14);
    d->font_small = try_load_font(FONT_PATHS, 11);
    d->font_clock = try_load_font(FONT_PATHS, 13);
    d->font_icon  = try_load_font(FONT_PATHS, 15);

    if (!d->font_label || !d->font_mono) {
        fprintf(stderr, "electronos-desktop: failed to load fonts\n");
        return -1;
    }

    /* Create wallpaper */
    d->wallpaper = wallpaper_create(renderer, w, h);
    if (!d->wallpaper) {
        fprintf(stderr, "electronos-desktop: failed to create wallpaper\n");
        return -1;
    }

    /* Initialize glass system */
    if (glass_init(renderer, d->wallpaper, w, h) != 0) {
        fprintf(stderr, "electronos-desktop: glass init failed\n");
        /* Non-fatal: glass panels will just be solid */
    }

    /* Initialize panel and dock */
    panel_init(d->font_label, d->font_clock ? d->font_clock : d->font_label);
    dock_init(d->font_icon ? d->font_icon : d->font_label);

    /* Enable text input for editor */
    SDL_StartTextInput();

    return 0;
}

void desktop_handle_event(desktop_t *d, const SDL_Event *event) {
    /* Panel events */
    panel_click_t pc = panel_handle_event(event, d->screen_w);
    if (pc == PANEL_CLICK_ACTIVITIES) {
        d->show_activities = !d->show_activities;
        return;
    }

    /* Dock events */
    dock_click_t dc = dock_handle_event(event, d->screen_h, PANEL_HEIGHT);
    if (dc.clicked) {
        desktop_launch_app(d, dc.app);
        return;
    }

    /* Mouse events on windows */
    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;

        /* Skip if in panel or dock area (already handled) */
        if (my <= PANEL_HEIGHT || mx < DOCK_WIDTH) return;

        desktop_window_t *hit = find_window_at(d, mx, my);
        if (hit) {
            /* Focus this window */
            if (d->focused) d->focused->focused = false;
            d->focused = hit;
            hit->focused = true;
            bring_to_front(d, hit);

            /* Check close button */
            if (window_close_hit(hit, mx, my)) {
                desktop_close_window(d, hit->id);
                return;
            }

            /* Check maximize button */
            if (window_maximize_hit(hit, mx, my)) {
                window_toggle_maximize(hit, d->screen_w, d->screen_h,
                                       PANEL_HEIGHT, DOCK_WIDTH);
                return;
            }

            /* Window handles its own drag/resize */
            window_handle_event(hit, event, d->screen_w, d->screen_h);

            /* Forward to app */
            if (hit->app.handle_event) {
                SDL_Rect cr = window_content_rect(hit);
                hit->app.handle_event(hit->app.ctx, event, cr);
            }
            return;
        }

        /* Click on desktop background — unfocus */
        if (d->focused) {
            d->focused->focused = false;
            d->focused = NULL;
        }
        return;
    }

    /* Mouse motion and button up — forward to windows */
    if (event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONUP) {
        /* Check dock hover */
        dock_handle_event(event, d->screen_h, PANEL_HEIGHT);

        /* Forward to all windows for drag/resize */
        desktop_window_t *w = d->windows;
        while (w) {
            if (w->dragging || w->resizing) {
                window_handle_event(w, event, d->screen_w, d->screen_h);
                return;
            }
            w = w->next;
        }

        /* Forward mouse to focused window's app */
        if (d->focused && !d->focused->minimized && d->focused->app.handle_event) {
            SDL_Rect cr = window_content_rect(d->focused);
            d->focused->app.handle_event(d->focused->app.ctx, event, cr);
        }
        return;
    }

    /* Keyboard events — forward to focused window */
    if ((event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) &&
        d->focused && !d->focused->minimized) {
        if (d->focused->app.handle_key) {
            d->focused->app.handle_key(d->focused->app.ctx, event);
        }
        return;
    }

    /* Text input events — forward to focused window */
    if (event->type == SDL_TEXTINPUT && d->focused && !d->focused->minimized) {
        /* Terminal handles text input specially */
        if (d->focused->app_type == APP_TERMINAL) {
            terminal_ctx_t *t = (terminal_ctx_t *)d->focused->app.ctx;
            if (t && t->alive && t->pty_fd >= 0) {
                ssize_t wr = write(t->pty_fd, event->text.text,
                                   strlen(event->text.text));
                (void)wr;
            }
        } else if (d->focused->app.handle_event) {
            SDL_Rect cr = window_content_rect(d->focused);
            d->focused->app.handle_event(d->focused->app.ctx, event, cr);
        }
        return;
    }
}

void desktop_update(desktop_t *d) {
    desktop_window_t *w = d->windows;
    while (w) {
        if (w->app.update) {
            w->app.update(w->app.ctx);
        }
        w = w->next;
    }
}

void desktop_render(desktop_t *d) {
    SDL_Renderer *r = d->renderer;

    /* 1. Wallpaper */
    wallpaper_render(r, d->wallpaper);

    /* 2. Windows (render back-to-front: last in list = bottom) */
    /* First, build a reversed list for rendering */
    desktop_window_t *render_order[256];
    int count = 0;
    desktop_window_t *w = d->windows;
    while (w && count < 256) {
        render_order[count++] = w;
        w = w->next;
    }
    /* Render from back (last) to front (first) */
    for (int i = count - 1; i >= 0; i--) {
        window_render(r, render_order[i], d->font_label);
    }

    /* 3. Panel (on top of everything) */
    panel_render(r, d->screen_w);

    /* 4. Dock (on top of everything) */
    dock_render(r, d->screen_h, PANEL_HEIGHT, d->app_running);

    /* 5. Activities overlay */
    if (d->show_activities) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
        SDL_Rect overlay = { DOCK_WIDTH, PANEL_HEIGHT,
                            d->screen_w - DOCK_WIDTH,
                            d->screen_h - PANEL_HEIGHT };
        SDL_RenderFillRect(r, &overlay);

        /* Title */
        if (d->font_title) {
            SDL_Color white = { 230, 230, 235, 255 };
            SDL_Surface *surf = TTF_RenderText_Blended(d->font_title,
                "Activities Overview", white);
            if (surf) {
                SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
                if (tex) {
                    SDL_Rect dst = {
                        DOCK_WIDTH + (d->screen_w - DOCK_WIDTH - surf->w) / 2,
                        PANEL_HEIGHT + 40,
                        surf->w, surf->h
                    };
                    SDL_RenderCopy(r, tex, NULL, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        /* Show window thumbnails */
        int tx = DOCK_WIDTH + 60;
        int ty = PANEL_HEIGHT + 90;
        w = d->windows;
        while (w) {
            if (!w->minimized) {
                /* Thumbnail frame */
                SDL_Rect thumb = { tx, ty, 200, 140 };
                SDL_SetRenderDrawColor(r, 50, 50, 60, 200);
                SDL_RenderFillRect(r, &thumb);
                SDL_SetRenderDrawColor(r, 100, 100, 110, 200);
                SDL_RenderDrawRect(r, &thumb);

                if (d->font_small) {
                    SDL_Color tc = { 200, 200, 210, 255 };
                    SDL_Surface *ts = TTF_RenderText_Blended(d->font_small,
                        w->title, tc);
                    if (ts) {
                        SDL_Texture *tt = SDL_CreateTextureFromSurface(r, ts);
                        if (tt) {
                            SDL_Rect td = { tx + 8, ty + 8, ts->w, ts->h };
                            SDL_RenderCopy(r, tt, NULL, &td);
                            SDL_DestroyTexture(tt);
                        }
                        SDL_FreeSurface(ts);
                    }
                }

                tx += 220;
                if (tx + 200 > d->screen_w - 60) {
                    tx = DOCK_WIDTH + 60;
                    ty += 160;
                }
            }
            w = w->next;
        }
    }
}

void desktop_launch_app(desktop_t *d, app_type_t app) {
    int wx, wy;
    next_window_pos(d, &wx, &wy);

    app_callbacks_t callbacks = { NULL, NULL, NULL, NULL, NULL, NULL };
    const char *title = "";
    int ww = 720, wh = 480;

    switch (app) {
    case APP_TERMINAL:
        title = "Terminal";
        ww = 750;
        wh = 500;
        callbacks = app_terminal_create(d->font_mono);
        break;
    case APP_SETTINGS:
        title = "Settings";
        ww = 680;
        wh = 460;
        callbacks = app_settings_create(d->font_title, d->font_label,
                                        d->font_label);
        break;
    case APP_EDITOR:
        title = "Text Editor";
        ww = 760;
        wh = 520;
        callbacks = app_editor_create(d->font_mono, d->font_label);
        break;
    default:
        return;
    }

    if (!callbacks.ctx) {
        fprintf(stderr, "electronos-desktop: failed to create %s app\n", title);
        return;
    }

    desktop_window_t *win = window_create(title, app, wx, wy, ww, wh,
                                          callbacks);
    if (!win) {
        if (callbacks.destroy) callbacks.destroy(callbacks.ctx);
        return;
    }

    /* Unfocus current window */
    if (d->focused) d->focused->focused = false;

    /* Add to front of list */
    win->next = d->windows;
    d->windows = win;
    d->focused = win;
    d->app_running[app] = true;
    d->next_window_id++;
}

void desktop_close_window(desktop_t *d, int window_id) {
    desktop_window_t **pp = &d->windows;
    while (*pp) {
        if ((*pp)->id == window_id) {
            desktop_window_t *w = *pp;
            *pp = w->next;

            /* Update app_running */
            app_type_t atype = w->app_type;
            bool still_running = false;
            desktop_window_t *check = d->windows;
            while (check) {
                if (check->app_type == atype) {
                    still_running = true;
                    break;
                }
                check = check->next;
            }
            d->app_running[atype] = still_running;

            /* Update focus */
            if (d->focused == w) {
                d->focused = d->windows;
                if (d->focused) d->focused->focused = true;
            }

            window_destroy(w);
            return;
        }
        pp = &(*pp)->next;
    }
}

void desktop_destroy(desktop_t *d) {
    /* Destroy all windows */
    while (d->windows) {
        desktop_window_t *w = d->windows;
        d->windows = w->next;
        window_destroy(w);
    }

    /* Destroy subsystems */
    panel_destroy();
    dock_destroy();
    glass_destroy();

    if (d->wallpaper) SDL_DestroyTexture(d->wallpaper);

    /* Destroy fonts */
    if (d->font_title) TTF_CloseFont(d->font_title);
    if (d->font_label) TTF_CloseFont(d->font_label);
    if (d->font_mono)  TTF_CloseFont(d->font_mono);
    if (d->font_small) TTF_CloseFont(d->font_small);
    if (d->font_clock) TTF_CloseFont(d->font_clock);
    if (d->font_icon)  TTF_CloseFont(d->font_icon);

    SDL_StopTextInput();
}
