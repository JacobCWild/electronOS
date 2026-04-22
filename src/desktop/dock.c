/* ===========================================================================
 * electronOS Desktop — Dock Implementation
 * ===========================================================================
 * Ubuntu-style left dock with application icons rendered as labeled circles.
 * Each icon has a hover glow effect and an indicator dot for running apps.
 * =========================================================================== */

#include "dock.h"
#include "glass.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ---- Module state ------------------------------------------------------- */
static TTF_Font *s_font = NULL;
static int s_hover_index = -1;

/* ---- App icon data ------------------------------------------------------ */
typedef struct {
    const char *label;      /* Short label for the icon */
    const char *symbol;     /* Single character symbol */
    Uint8 r, g, b;          /* Icon background color */
} dock_icon_t;

static const dock_icon_t DOCK_ICONS[APP_COUNT] = {
    [APP_TERMINAL] = { "Terminal", ">_", 40, 40, 45 },
    [APP_SETTINGS] = { "Settings", "*",  70, 70, 80 },
    [APP_EDITOR]   = { "Editor",   "A",  50, 80, 130 },
};

/* ---- Helpers ------------------------------------------------------------ */

static void render_text_centered(SDL_Renderer *renderer, TTF_Font *font,
                                 const char *text, int cx, int cy,
                                 SDL_Color color) {
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = { cx - surf->w / 2, cy - surf->h / 2,
                         surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

static void fill_circle(SDL_Renderer *renderer, int cx, int cy, int r) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static SDL_Rect icon_rect(int index, int panel_h) {
    int y_start = panel_h + DOCK_PADDING + 4;
    int y = y_start + index * (DOCK_ICON_SIZE + DOCK_PADDING);
    int x = (DOCK_WIDTH - DOCK_ICON_SIZE) / 2;
    return (SDL_Rect){ x, y, DOCK_ICON_SIZE, DOCK_ICON_SIZE };
}

/* ---- API ---------------------------------------------------------------- */

int dock_init(TTF_Font *font) {
    s_font = font;
    return 0;
}

void dock_render(SDL_Renderer *renderer, int screen_h, int panel_h,
                 const bool *app_running) {
    /* Glass background (full left column below panel) */
    int dock_h = screen_h - panel_h;
    SDL_Rect dock_rect = { 0, panel_h, DOCK_WIDTH, dock_h };
    glass_render(renderer, &dock_rect, GLASS_DOCK);

    /* Right border */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 20);
    SDL_RenderDrawLine(renderer, DOCK_WIDTH - 1, panel_h,
                       DOCK_WIDTH - 1, screen_h);

    SDL_Color icon_text_color = { 220, 220, 225, 255 };

    for (int i = 0; i < APP_COUNT; i++) {
        SDL_Rect ir = icon_rect(i, panel_h);
        const dock_icon_t *icon = &DOCK_ICONS[i];

        /* Hover highlight */
        if (i == s_hover_index) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 25);
            SDL_Rect hover_r = { ir.x - 2, ir.y - 2, ir.w + 4, ir.h + 4 };
            SDL_RenderFillRect(renderer, &hover_r);
        }

        /* Icon background (rounded via filled circle-ish rect) */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, icon->r, icon->g, icon->b, 180);
        SDL_RenderFillRect(renderer, &ir);

        /* Icon border */
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 30);
        SDL_RenderDrawRect(renderer, &ir);

        /* Icon symbol text */
        render_text_centered(renderer, s_font, icon->symbol,
                             ir.x + ir.w / 2, ir.y + ir.h / 2,
                             icon_text_color);

        /* Running indicator dot (left side) */
        if (app_running && app_running[i]) {
            SDL_SetRenderDrawColor(renderer, 230, 230, 240, 200);
            fill_circle(renderer, 3, ir.y + ir.h / 2, 2);
        }
    }
}

dock_click_t dock_handle_event(const SDL_Event *event, int screen_h,
                               int panel_h) {
    dock_click_t result = { false, APP_TERMINAL };
    (void)screen_h;

    if (event->type == SDL_MOUSEMOTION) {
        int mx = event->motion.x;
        int my = event->motion.y;
        s_hover_index = -1;
        if (mx < DOCK_WIDTH && my > panel_h) {
            for (int i = 0; i < APP_COUNT; i++) {
                SDL_Rect ir = icon_rect(i, panel_h);
                if (mx >= ir.x && mx < ir.x + ir.w &&
                    my >= ir.y && my < ir.y + ir.h) {
                    s_hover_index = i;
                    break;
                }
            }
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;
        if (mx < DOCK_WIDTH && my > panel_h) {
            for (int i = 0; i < APP_COUNT; i++) {
                SDL_Rect ir = icon_rect(i, panel_h);
                if (mx >= ir.x && mx < ir.x + ir.w &&
                    my >= ir.y && my < ir.y + ir.h) {
                    result.clicked = true;
                    result.app = (app_type_t)i;
                    break;
                }
            }
        }
    }

    return result;
}

void dock_destroy(void) {
    s_font = NULL;
}
