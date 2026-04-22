/* ===========================================================================
 * electronOS Desktop — Wallpaper
 * ===========================================================================
 * Generates a gradient wallpaper (deep blue/purple, macOS-inspired).
 * =========================================================================== */

#ifndef ELECTRONOS_DESKTOP_WALLPAPER_H
#define ELECTRONOS_DESKTOP_WALLPAPER_H

#include <SDL2/SDL.h>

/* Create wallpaper texture (caller owns the texture) */
SDL_Texture *wallpaper_create(SDL_Renderer *renderer, int w, int h);

/* Render wallpaper fullscreen */
void wallpaper_render(SDL_Renderer *renderer, SDL_Texture *wallpaper);

#endif /* ELECTRONOS_DESKTOP_WALLPAPER_H */
