/* ===========================================================================
 * electronOS Desktop — Top Panel
 * ===========================================================================
 * Ubuntu-style top panel with Activities button, clock, and system tray.
 * Rendered with liquid glass effect.
 * =========================================================================== */

#ifndef ELECTRONOS_DESKTOP_PANEL_H
#define ELECTRONOS_DESKTOP_PANEL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#define PANEL_HEIGHT    28

/* Panel click results */
typedef enum {
    PANEL_CLICK_NONE,
    PANEL_CLICK_ACTIVITIES,
    PANEL_CLICK_POWER,
} panel_click_t;

/* Initialize the panel */
int  panel_init(TTF_Font *font_label, TTF_Font *font_clock);

/* Render the panel at the top of the screen */
void panel_render(SDL_Renderer *renderer, int screen_w);

/* Handle mouse event. Returns what was clicked. */
panel_click_t panel_handle_event(const SDL_Event *event, int screen_w);

/* Destroy panel resources */
void panel_destroy(void);

#endif /* ELECTRONOS_DESKTOP_PANEL_H */
