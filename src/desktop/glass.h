/* ===========================================================================
 * electronOS Desktop — Liquid Glass Effect
 * ===========================================================================
 * Provides Apple-style frosted glass rendering by blurring the wallpaper
 * behind translucent UI elements (panels, dock, window titlebars).
 * =========================================================================== */

#ifndef ELECTRONOS_DESKTOP_GLASS_H
#define ELECTRONOS_DESKTOP_GLASS_H

#include <SDL2/SDL.h>

/* Glass visual styles */
typedef enum {
    GLASS_PANEL,        /* Top panel / dock — subtle, low opacity    */
    GLASS_WINDOW,       /* Window titlebar — slightly more opaque    */
    GLASS_WINDOW_ACTIVE,/* Active window titlebar — brighter         */
    GLASS_DOCK,         /* Dock background                           */
    GLASS_MENU,         /* Context menus / popups                    */
} glass_style_t;

/* Initialize the glass system. Call once after creating the wallpaper.
 * Takes the wallpaper texture and creates a blurred copy for glass bg. */
int  glass_init(SDL_Renderer *renderer, SDL_Texture *wallpaper,
                int screen_w, int screen_h);

/* Render a glass panel at the given rectangle */
void glass_render(SDL_Renderer *renderer, const SDL_Rect *area,
                  glass_style_t style);

/* Render a glass panel with rounded corners (radius in pixels) */
void glass_render_rounded(SDL_Renderer *renderer, const SDL_Rect *area,
                          glass_style_t style, int radius);

/* Clean up glass resources */
void glass_destroy(void);

#endif /* ELECTRONOS_DESKTOP_GLASS_H */
