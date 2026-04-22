/* ===========================================================================
 * electronOS Desktop — Wallpaper Implementation
 * ===========================================================================
 * Generates a gradient wallpaper inspired by macOS Sonoma/Sequoia:
 * deep blue at top-left transitioning to purple/magenta at bottom-right,
 * with subtle radial highlights for depth.
 * =========================================================================== */

#include "wallpaper.h"
#include <math.h>
#include <stdlib.h>

/* Color stops for the gradient */
typedef struct { float r, g, b; } color3f_t;

static color3f_t lerp_color(color3f_t a, color3f_t b, float t) {
    return (color3f_t){
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
    };
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

SDL_Texture *wallpaper_create(SDL_Renderer *renderer, int w, int h) {
    SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!tex) return NULL;

    void *pixels;
    int pitch;
    if (SDL_LockTexture(tex, NULL, &pixels, &pitch) != 0) {
        SDL_DestroyTexture(tex);
        return NULL;
    }

    /* Gradient colors */
    color3f_t top_left     = { 0.05f, 0.05f, 0.20f };  /* Deep navy */
    color3f_t top_right    = { 0.12f, 0.04f, 0.25f };  /* Dark purple */
    color3f_t bottom_left  = { 0.08f, 0.10f, 0.30f };  /* Blue */
    color3f_t bottom_right = { 0.22f, 0.06f, 0.28f };  /* Magenta-purple */

    /* Radial highlight center (slightly off-center, upper area) */
    float cx = w * 0.4f;
    float cy = h * 0.35f;
    float max_r = sqrtf((float)(w * w + h * h)) * 0.5f;

    Uint32 *row;
    for (int y = 0; y < h; y++) {
        row = (Uint32 *)((Uint8 *)pixels + y * pitch);
        float fy = (float)y / (float)(h - 1);

        for (int x = 0; x < w; x++) {
            float fx = (float)x / (float)(w - 1);

            /* Bilinear gradient */
            color3f_t top = lerp_color(top_left, top_right, fx);
            color3f_t bot = lerp_color(bottom_left, bottom_right, fx);
            color3f_t base = lerp_color(top, bot, fy);

            /* Radial highlight (warm glow) */
            float dx = (float)x - cx;
            float dy = (float)y - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float glow = 1.0f - clampf(dist / max_r, 0.0f, 1.0f);
            glow = glow * glow * glow * 0.15f;  /* Subtle cubic falloff */

            color3f_t highlight = { 0.3f, 0.15f, 0.4f };
            base.r = clampf(base.r + highlight.r * glow, 0.0f, 1.0f);
            base.g = clampf(base.g + highlight.g * glow, 0.0f, 1.0f);
            base.b = clampf(base.b + highlight.b * glow, 0.0f, 1.0f);

            /* Second radial highlight (lower right, blue) */
            float cx2 = w * 0.75f, cy2 = h * 0.7f;
            float dx2 = (float)x - cx2;
            float dy2 = (float)y - cy2;
            float dist2 = sqrtf(dx2 * dx2 + dy2 * dy2);
            float glow2 = 1.0f - clampf(dist2 / (max_r * 0.6f), 0.0f, 1.0f);
            glow2 = glow2 * glow2 * glow2 * 0.1f;

            color3f_t hl2 = { 0.1f, 0.2f, 0.45f };
            base.r = clampf(base.r + hl2.r * glow2, 0.0f, 1.0f);
            base.g = clampf(base.g + hl2.g * glow2, 0.0f, 1.0f);
            base.b = clampf(base.b + hl2.b * glow2, 0.0f, 1.0f);

            Uint8 r = (Uint8)(base.r * 255.0f);
            Uint8 g = (Uint8)(base.g * 255.0f);
            Uint8 b = (Uint8)(base.b * 255.0f);
            row[x] = (0xFFu << 24) | ((Uint32)r << 16) |
                     ((Uint32)g << 8) | (Uint32)b;
        }
    }

    SDL_UnlockTexture(tex);
    return tex;
}

void wallpaper_render(SDL_Renderer *renderer, SDL_Texture *wallpaper) {
    if (wallpaper) {
        SDL_RenderCopy(renderer, wallpaper, NULL, NULL);
    }
}
