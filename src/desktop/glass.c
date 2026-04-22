/* ===========================================================================
 * electronOS Desktop — Liquid Glass Effect Implementation
 * ===========================================================================
 * Creates a blurred copy of the wallpaper at init time. When rendering a
 * glass panel, it samples the blurred wallpaper behind the element and
 * overlays a semi-transparent tint with a thin luminous border.
 *
 * The blur uses a 3-pass box blur (approximates Gaussian) on the wallpaper
 * pixels. This runs once at startup so there's no per-frame cost.
 * =========================================================================== */

#include "glass.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- Module state ------------------------------------------------------- */
static SDL_Texture *s_blurred = NULL;
static int s_width  = 0;
static int s_height = 0;

/* ---- Box blur ----------------------------------------------------------- */

static void box_blur_h(Uint32 *src, Uint32 *dst, int w, int h, int radius) {
    float inv = 1.0f / (float)(radius + radius + 1);

    for (int y = 0; y < h; y++) {
        Uint32 *row_src = src + y * w;
        Uint32 *row_dst = dst + y * w;

        float r_sum = 0, g_sum = 0, b_sum = 0;

        /* Seed with left edge repeated */
        for (int i = -radius; i <= radius; i++) {
            int idx = i < 0 ? 0 : (i >= w ? w - 1 : i);
            Uint32 px = row_src[idx];
            r_sum += (float)((px >> 16) & 0xFF);
            g_sum += (float)((px >> 8) & 0xFF);
            b_sum += (float)(px & 0xFF);
        }

        for (int x = 0; x < w; x++) {
            Uint8 rr = (Uint8)(r_sum * inv);
            Uint8 gg = (Uint8)(g_sum * inv);
            Uint8 bb = (Uint8)(b_sum * inv);
            row_dst[x] = (0xFFu << 24) | ((Uint32)rr << 16) |
                         ((Uint32)gg << 8) | (Uint32)bb;

            /* Slide window */
            int add_idx = x + radius + 1;
            int rem_idx = x - radius;
            if (add_idx >= w) add_idx = w - 1;
            if (rem_idx < 0) rem_idx = 0;

            Uint32 add_px = row_src[add_idx];
            Uint32 rem_px = row_src[rem_idx];
            r_sum += (float)((add_px >> 16) & 0xFF) - (float)((rem_px >> 16) & 0xFF);
            g_sum += (float)((add_px >> 8) & 0xFF) - (float)((rem_px >> 8) & 0xFF);
            b_sum += (float)(add_px & 0xFF) - (float)(rem_px & 0xFF);
        }
    }
}

static void box_blur_v(Uint32 *src, Uint32 *dst, int w, int h, int radius) {
    float inv = 1.0f / (float)(radius + radius + 1);

    for (int x = 0; x < w; x++) {
        float r_sum = 0, g_sum = 0, b_sum = 0;

        for (int i = -radius; i <= radius; i++) {
            int idx = i < 0 ? 0 : (i >= h ? h - 1 : i);
            Uint32 px = src[idx * w + x];
            r_sum += (float)((px >> 16) & 0xFF);
            g_sum += (float)((px >> 8) & 0xFF);
            b_sum += (float)(px & 0xFF);
        }

        for (int y = 0; y < h; y++) {
            Uint8 rr = (Uint8)(r_sum * inv);
            Uint8 gg = (Uint8)(g_sum * inv);
            Uint8 bb = (Uint8)(b_sum * inv);
            dst[y * w + x] = (0xFFu << 24) | ((Uint32)rr << 16) |
                              ((Uint32)gg << 8) | (Uint32)bb;

            int add_idx = y + radius + 1;
            int rem_idx = y - radius;
            if (add_idx >= h) add_idx = h - 1;
            if (rem_idx < 0) rem_idx = 0;

            Uint32 add_px = src[add_idx * w + x];
            Uint32 rem_px = src[rem_idx * w + x];
            r_sum += (float)((add_px >> 16) & 0xFF) - (float)((rem_px >> 16) & 0xFF);
            g_sum += (float)((add_px >> 8) & 0xFF) - (float)((rem_px >> 8) & 0xFF);
            b_sum += (float)(add_px & 0xFF) - (float)(rem_px & 0xFF);
        }
    }
}

/* ---- Initialization ----------------------------------------------------- */

int glass_init(SDL_Renderer *renderer, SDL_Texture *wallpaper,
               int screen_w, int screen_h) {
    s_width = screen_w;
    s_height = screen_h;

    /* Read wallpaper pixels */
    Uint32 format;
    int ww, wh;
    SDL_QueryTexture(wallpaper, &format, NULL, &ww, &wh);
    (void)ww;
    (void)wh;

    /* Create a target texture to read pixels from wallpaper */
    SDL_Texture *target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_TARGET,
                                            screen_w, screen_h);
    if (!target) return -1;

    SDL_SetRenderTarget(renderer, target);
    SDL_RenderCopy(renderer, wallpaper, NULL, NULL);

    /* Read pixels from the render target */
    size_t pixel_count = (size_t)screen_w * (size_t)screen_h;
    Uint32 *pixels = malloc(pixel_count * sizeof(Uint32));
    Uint32 *tmp    = malloc(pixel_count * sizeof(Uint32));
    if (!pixels || !tmp) {
        free(pixels);
        free(tmp);
        SDL_DestroyTexture(target);
        SDL_SetRenderTarget(renderer, NULL);
        return -1;
    }

    SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                         pixels, screen_w * 4);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_DestroyTexture(target);

    /* Apply 3-pass box blur (radius 25) to approximate Gaussian blur */
    int blur_radius = 25;
    for (int pass = 0; pass < 3; pass++) {
        box_blur_h(pixels, tmp, screen_w, screen_h, blur_radius);
        box_blur_v(tmp, pixels, screen_w, screen_h, blur_radius);
    }

    /* Create blurred texture */
    s_blurred = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STATIC,
                                  screen_w, screen_h);
    if (s_blurred) {
        SDL_UpdateTexture(s_blurred, NULL, pixels, screen_w * 4);
    }

    free(pixels);
    free(tmp);
    return s_blurred ? 0 : -1;
}

/* ---- Style parameters --------------------------------------------------- */

typedef struct {
    Uint8 tint_r, tint_g, tint_b, tint_a;  /* Overlay tint */
    Uint8 border_r, border_g, border_b, border_a;
    Uint8 shine_a;                          /* Top-edge shine alpha */
} glass_params_t;

static glass_params_t get_params(glass_style_t style) {
    switch (style) {
    case GLASS_PANEL:
        return (glass_params_t){
            200, 200, 210, 25,      /* Very subtle white tint */
            255, 255, 255, 40,      /* Thin border */
            18,                     /* Subtle shine */
        };
    case GLASS_DOCK:
        return (glass_params_t){
            200, 200, 215, 30,
            255, 255, 255, 45,
            20,
        };
    case GLASS_WINDOW:
        return (glass_params_t){
            180, 185, 200, 35,
            255, 255, 255, 35,
            15,
        };
    case GLASS_WINDOW_ACTIVE:
        return (glass_params_t){
            200, 205, 220, 45,
            255, 255, 255, 55,
            25,
        };
    case GLASS_MENU:
        return (glass_params_t){
            210, 210, 220, 50,
            255, 255, 255, 50,
            22,
        };
    }
    return (glass_params_t){ 200, 200, 200, 30, 255, 255, 255, 40, 15 };
}

/* ---- Rendering ---------------------------------------------------------- */

void glass_render(SDL_Renderer *renderer, const SDL_Rect *area,
                  glass_style_t style) {
    if (!s_blurred) return;

    glass_params_t p = get_params(style);

    /* 1. Render blurred wallpaper region */
    SDL_Rect src = *area;
    /* Clamp source rect */
    if (src.x < 0) src.x = 0;
    if (src.y < 0) src.y = 0;
    if (src.x + src.w > s_width) src.w = s_width - src.x;
    if (src.y + src.h > s_height) src.h = s_height - src.y;

    SDL_SetTextureBlendMode(s_blurred, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(s_blurred, 220);
    SDL_RenderCopy(renderer, s_blurred, &src, area);

    /* 2. Overlay tint */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, p.tint_r, p.tint_g, p.tint_b, p.tint_a);
    SDL_RenderFillRect(renderer, area);

    /* 3. Top-edge shine (1px gradient line for glass reflection) */
    if (p.shine_a > 0) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, p.shine_a);
        SDL_RenderDrawLine(renderer, area->x, area->y,
                           area->x + area->w - 1, area->y);
        /* Second line slightly dimmer */
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, p.shine_a / 2);
        SDL_RenderDrawLine(renderer, area->x, area->y + 1,
                           area->x + area->w - 1, area->y + 1);
    }

    /* 4. Border */
    SDL_SetRenderDrawColor(renderer, p.border_r, p.border_g, p.border_b,
                           p.border_a);
    SDL_RenderDrawRect(renderer, area);
}

void glass_render_rounded(SDL_Renderer *renderer, const SDL_Rect *area,
                          glass_style_t style, int radius) {
    /* For simplicity, render as regular rect + soften corners with bg color.
     * A full rounded-rect implementation would need per-pixel masking. */
    glass_render(renderer, area, style);
    (void)radius;
}

/* ---- Cleanup ------------------------------------------------------------ */

void glass_destroy(void) {
    if (s_blurred) {
        SDL_DestroyTexture(s_blurred);
        s_blurred = NULL;
    }
}
