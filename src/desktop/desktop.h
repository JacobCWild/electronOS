/* ===========================================================================
 * electronOS Desktop — Desktop Compositor
 * ===========================================================================
 * Main desktop manager: ties together wallpaper, glass, panel, dock, and
 * window management into a cohesive Ubuntu-layout + Liquid Glass desktop.
 * =========================================================================== */

#ifndef ELECTRONOS_DESKTOP_DESKTOP_H
#define ELECTRONOS_DESKTOP_DESKTOP_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include "window.h"

/* Desktop state */
typedef struct {
    SDL_Renderer    *renderer;
    int              screen_w;
    int              screen_h;

    /* Wallpaper */
    SDL_Texture     *wallpaper;

    /* Fonts */
    TTF_Font        *font_title;    /* 18pt bold */
    TTF_Font        *font_label;    /* 13pt regular */
    TTF_Font        *font_mono;     /* 14pt monospace */
    TTF_Font        *font_small;    /* 11pt for hints */
    TTF_Font        *font_clock;    /* 13pt for panel clock */
    TTF_Font        *font_icon;     /* 24pt for dock icons (emoji fallback) */

    /* Windows (linked list, front = top of z-order) */
    desktop_window_t *windows;
    desktop_window_t *focused;
    int               next_window_id;

    /* Track which apps are running */
    bool              app_running[APP_COUNT];

    /* Activities overlay */
    bool              show_activities;

    /* Test mode */
    bool              test_mode;
} desktop_t;

/* Initialize the desktop. Must be called after SDL2 + TTF init. */
int  desktop_init(desktop_t *desktop, SDL_Renderer *renderer,
                  int w, int h, bool test_mode);

/* Handle an SDL event */
void desktop_handle_event(desktop_t *desktop, const SDL_Event *event);

/* Update desktop state (poll PTYs, animations, etc.) */
void desktop_update(desktop_t *desktop);

/* Render the full desktop */
void desktop_render(desktop_t *desktop);

/* Launch an application */
void desktop_launch_app(desktop_t *desktop, app_type_t app);

/* Close a window by ID */
void desktop_close_window(desktop_t *desktop, int window_id);

/* Destroy the desktop and all resources */
void desktop_destroy(desktop_t *desktop);

#endif /* ELECTRONOS_DESKTOP_DESKTOP_H */
