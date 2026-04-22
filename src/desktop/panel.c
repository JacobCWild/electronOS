/* ===========================================================================
 * electronOS Desktop — Top Panel Implementation
 * ===========================================================================
 * Ubuntu-style top panel with liquid glass effect.
 * Layout: [Activities]  ...  [Clock]  ...  [Power icon]
 * =========================================================================== */

#include "panel.h"
#include "glass.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ---- Module state ------------------------------------------------------- */
static TTF_Font *s_font_label = NULL;
static TTF_Font *s_font_clock = NULL;

/* ---- Helpers ------------------------------------------------------------ */

static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                        const char *text, int x, int y,
                        SDL_Color color) {
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

static void render_text_centered(SDL_Renderer *renderer, TTF_Font *font,
                                 const char *text, int center_x, int y,
                                 SDL_Color color) {
    int tw, th;
    TTF_SizeText(font, text, &tw, &th);
    render_text(renderer, font, text, center_x - tw / 2, y, color);
}

/* ---- API ---------------------------------------------------------------- */

int panel_init(TTF_Font *font_label, TTF_Font *font_clock) {
    s_font_label = font_label;
    s_font_clock = font_clock;
    return 0;
}

void panel_render(SDL_Renderer *renderer, int screen_w) {
    /* Glass background */
    SDL_Rect panel_rect = { 0, 0, screen_w, PANEL_HEIGHT };
    glass_render(renderer, &panel_rect, GLASS_PANEL);

    SDL_Color white = { 230, 230, 235, 255 };
    SDL_Color dim   = { 180, 180, 190, 255 };

    /* Activities button (left) */
    render_text(renderer, s_font_label, "Activities", 12, 6, white);

    /* Clock (center) */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char clock_buf[64];
    strftime(clock_buf, sizeof(clock_buf), "%a %b %d  %H:%M", tm);
    render_text_centered(renderer, s_font_clock, clock_buf,
                         screen_w / 2, 6, white);

    /* System tray area (right) */
    /* Power icon — simple text for now */
    int tw, th;
    TTF_SizeText(s_font_label, "Power", &tw, &th);
    render_text(renderer, s_font_label, "Power",
                screen_w - tw - 14, 6, dim);

    /* Bottom separator line */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 15);
    SDL_RenderDrawLine(renderer, 0, PANEL_HEIGHT - 1,
                       screen_w, PANEL_HEIGHT - 1);
}

panel_click_t panel_handle_event(const SDL_Event *event, int screen_w) {
    if (event->type != SDL_MOUSEBUTTONDOWN) return PANEL_CLICK_NONE;

    int mx = event->button.x;
    int my = event->button.y;

    /* Must be within panel height */
    if (my > PANEL_HEIGHT) return PANEL_CLICK_NONE;

    /* Activities (left 100px) */
    if (mx < 100) return PANEL_CLICK_ACTIVITIES;

    /* Power (right 80px) */
    if (mx > screen_w - 80) return PANEL_CLICK_POWER;

    return PANEL_CLICK_NONE;
}

void panel_destroy(void) {
    /* Fonts are owned externally */
    s_font_label = NULL;
    s_font_clock = NULL;
}
