/* ===========================================================================
 * electronOS Desktop — Dock
 * ===========================================================================
 * Ubuntu-style left dock with application icons.
 * Rendered with liquid glass effect.
 * =========================================================================== */

#ifndef ELECTRONOS_DESKTOP_DOCK_H
#define ELECTRONOS_DESKTOP_DOCK_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include "window.h"

#define DOCK_WIDTH      56
#define DOCK_ICON_SIZE  40
#define DOCK_PADDING    8

/* Dock click result */
typedef struct {
    bool      clicked;
    app_type_t app;
} dock_click_t;

/* Initialize the dock */
int  dock_init(TTF_Font *font);

/* Render the dock on the left side */
void dock_render(SDL_Renderer *renderer, int screen_h, int panel_h,
                 const bool *app_running);

/* Handle mouse event. Returns which app icon was clicked. */
dock_click_t dock_handle_event(const SDL_Event *event, int screen_h,
                               int panel_h);

/* Destroy dock resources */
void dock_destroy(void);

#endif /* ELECTRONOS_DESKTOP_DOCK_H */
