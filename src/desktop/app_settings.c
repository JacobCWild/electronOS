/* ===========================================================================
 * electronOS Desktop — Settings App Implementation
 * ===========================================================================
 * System configuration UI with a sidebar category list and detail panel.
 * Categories: Display, Network, Security, About.
 * Styled with the liquid glass aesthetic.
 * =========================================================================== */

#include "app_settings.h"
#include "glass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

/* ---- Layout constants --------------------------------------------------- */
#define SIDEBAR_W       170
#define ITEM_H          36
#define PADDING         16
#define SECTION_GAP     24

/* ---- Category metadata -------------------------------------------------- */
static const char *CAT_NAMES[SETTINGS_CATEGORY_COUNT] = {
    "Display",
    "Network",
    "Security",
    "About",
};

static const char *CAT_ICONS[SETTINGS_CATEGORY_COUNT] = {
    "[D]",
    "[N]",
    "[S]",
    "[i]",
};

/* ---- Helpers ------------------------------------------------------------ */

static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                        const char *text, int x, int y, SDL_Color color) {
    if (!font || !text || !*text) return;
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

static void render_label_value(SDL_Renderer *renderer, TTF_Font *label_font,
                               TTF_Font *value_font, const char *label,
                               const char *value, int x, int *y) {
    SDL_Color dim = { 160, 160, 170, 255 };
    SDL_Color white = { 220, 220, 225, 255 };
    render_text(renderer, label_font, label, x, *y, dim);
    *y += 20;
    render_text(renderer, value_font, value, x, *y, white);
    *y += 28;
}

static void render_slider(SDL_Renderer *renderer, int x, int y, int w,
                          float value) {
    /* Track */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 80, 80, 90, 180);
    SDL_Rect track = { x, y + 4, w, 6 };
    SDL_RenderFillRect(renderer, &track);

    /* Fill */
    int fill_w = (int)((float)w * value);
    SDL_SetRenderDrawColor(renderer, 80, 140, 230, 220);
    SDL_Rect fill = { x, y + 4, fill_w, 6 };
    SDL_RenderFillRect(renderer, &fill);

    /* Knob */
    int knob_x = x + fill_w;
    SDL_SetRenderDrawColor(renderer, 220, 220, 230, 255);
    SDL_Rect knob = { knob_x - 6, y, 12, 14 };
    SDL_RenderFillRect(renderer, &knob);
}

/* ---- Detail page renderers --------------------------------------------- */

static void render_display_page(settings_ctx_t *s, SDL_Renderer *renderer,
                                SDL_Rect area) {
    int y = area.y + PADDING;
    int x = area.x + PADDING;

    render_text(renderer, s->font_title, "Display", x, y,
                (SDL_Color){ 230, 230, 235, 255 });
    y += 36;

    /* Brightness slider */
    render_text(renderer, s->font_label, "Brightness", x, y,
                (SDL_Color){ 170, 170, 180, 255 });
    y += 22;
    render_slider(renderer, x, y, area.w - 2 * PADDING,
                  (float)s->brightness / 100.0f);
    y += 28;

    char buf[32];
    snprintf(buf, sizeof(buf), "%d%%", s->brightness);
    render_text(renderer, s->font_value, buf, x, y,
                (SDL_Color){ 200, 200, 210, 255 });
    y += SECTION_GAP;

    /* Resolution (read-only) */
    render_label_value(renderer, s->font_label, s->font_value,
                       "Resolution", "1920 x 1080 (default)", x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "Refresh Rate", "60 Hz", x, &y);
}

static void render_network_page(settings_ctx_t *s, SDL_Renderer *renderer,
                                SDL_Rect area) {
    int y = area.y + PADDING;
    int x = area.x + PADDING;

    render_text(renderer, s->font_title, "Network", x, y,
                (SDL_Color){ 230, 230, 235, 255 });
    y += 36;

    /* Hostname */
    char hostname[256] = "electronos";
    gethostname(hostname, sizeof(hostname));
    render_label_value(renderer, s->font_label, s->font_value,
                       "Hostname", hostname, x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "Firewall", "nftables (active)", x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "SSH", "Disabled by default", x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "DNS", "systemd-resolved", x, &y);
}

static void render_security_page(settings_ctx_t *s, SDL_Renderer *renderer,
                                 SDL_Rect area) {
    int y = area.y + PADDING;
    int x = area.x + PADDING;

    render_text(renderer, s->font_title, "Security", x, y,
                (SDL_Color){ 230, 230, 235, 255 });
    y += 36;

    render_label_value(renderer, s->font_label, s->font_value,
                       "AppArmor", "Enforcing", x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "Firewall", "nftables — default deny incoming", x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "Disk Encryption", "LUKS2 (AES-XTS-512, Argon2id)",
                       x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "Kernel Hardening", "ASLR, stack protector, "
                       "restricted ptrace", x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "Audit Framework", "Linux Audit (active)", x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "SUID Binaries", "Minimized", x, &y);
}

static void render_about_page(settings_ctx_t *s, SDL_Renderer *renderer,
                              SDL_Rect area) {
    int y = area.y + PADDING;
    int x = area.x + PADDING;

    render_text(renderer, s->font_title, "About electronOS", x, y,
                (SDL_Color){ 230, 230, 235, 255 });
    y += 36;

    render_label_value(renderer, s->font_label, s->font_value,
                       "Version", "1.0.0", x, &y);

    /* Kernel info */
    struct utsname un;
    char kernel_str[256] = "Linux";
    if (uname(&un) == 0) {
        snprintf(kernel_str, sizeof(kernel_str), "%s %s", un.sysname,
                 un.release);
    }
    render_label_value(renderer, s->font_label, s->font_value,
                       "Kernel", kernel_str, x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "Architecture", "ARM64 (Raspberry Pi 5)", x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "Build System", "Buildroot 2024.11.1", x, &y);

    /* Memory */
    struct sysinfo si;
    char mem_str[64] = "Unknown";
    if (sysinfo(&si) == 0) {
        unsigned long total_mb = si.totalram / (1024 * 1024);
        snprintf(mem_str, sizeof(mem_str), "%lu MB", total_mb);
    }
    render_label_value(renderer, s->font_label, s->font_value,
                       "Memory", mem_str, x, &y);

    render_label_value(renderer, s->font_label, s->font_value,
                       "License", "MIT", x, &y);
}

/* ---- Callbacks ---------------------------------------------------------- */

static void settings_render_cb(void *ctx, SDL_Renderer *renderer,
                               SDL_Rect area) {
    settings_ctx_t *s = (settings_ctx_t *)ctx;
    if (!s) return;

    /* Sidebar background */
    SDL_Rect sidebar = { area.x, area.y, SIDEBAR_W, area.h };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 35, 35, 42, 240);
    SDL_RenderFillRect(renderer, &sidebar);

    /* Sidebar border */
    SDL_SetRenderDrawColor(renderer, 60, 60, 70, 200);
    SDL_RenderDrawLine(renderer, area.x + SIDEBAR_W - 1, area.y,
                       area.x + SIDEBAR_W - 1, area.y + area.h);

    /* Category items */
    for (int i = 0; i < SETTINGS_CATEGORY_COUNT; i++) {
        int iy = area.y + 8 + i * ITEM_H;
        SDL_Rect item_rect = { area.x, iy, SIDEBAR_W, ITEM_H };

        /* Active highlight */
        if (i == (int)s->active_category) {
            SDL_SetRenderDrawColor(renderer, 80, 140, 230, 50);
            SDL_RenderFillRect(renderer, &item_rect);
            /* Left accent bar */
            SDL_SetRenderDrawColor(renderer, 80, 140, 230, 200);
            SDL_Rect bar = { area.x, iy, 3, ITEM_H };
            SDL_RenderFillRect(renderer, &bar);
        } else if (i == s->hover_category) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 15);
            SDL_RenderFillRect(renderer, &item_rect);
        }

        SDL_Color icon_color = (i == (int)s->active_category)
            ? (SDL_Color){ 120, 170, 240, 255 }
            : (SDL_Color){ 140, 140, 150, 255 };
        SDL_Color text_color = (i == (int)s->active_category)
            ? (SDL_Color){ 230, 230, 235, 255 }
            : (SDL_Color){ 180, 180, 190, 255 };

        render_text(renderer, s->font_label, CAT_ICONS[i],
                    area.x + 12, iy + 9, icon_color);
        render_text(renderer, s->font_label, CAT_NAMES[i],
                    area.x + 42, iy + 9, text_color);
    }

    /* Detail panel */
    SDL_Rect detail = {
        area.x + SIDEBAR_W, area.y,
        area.w - SIDEBAR_W, area.h
    };

    switch (s->active_category) {
    case SETTINGS_DISPLAY:  render_display_page(s, renderer, detail);  break;
    case SETTINGS_NETWORK:  render_network_page(s, renderer, detail);  break;
    case SETTINGS_SECURITY: render_security_page(s, renderer, detail); break;
    case SETTINGS_ABOUT:    render_about_page(s, renderer, detail);    break;
    default: break;
    }
}

static void settings_handle_event_cb(void *ctx, const SDL_Event *event,
                                     SDL_Rect area) {
    settings_ctx_t *s = (settings_ctx_t *)ctx;
    if (!s) return;

    if (event->type == SDL_MOUSEMOTION) {
        int mx = event->motion.x;
        int my = event->motion.y;
        s->hover_category = -1;
        if (mx >= area.x && mx < area.x + SIDEBAR_W) {
            int idx = (my - area.y - 8) / ITEM_H;
            if (idx >= 0 && idx < SETTINGS_CATEGORY_COUNT) {
                s->hover_category = idx;
            }
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;
        if (mx >= area.x && mx < area.x + SIDEBAR_W) {
            int idx = (my - area.y - 8) / ITEM_H;
            if (idx >= 0 && idx < SETTINGS_CATEGORY_COUNT) {
                s->active_category = (settings_category_t)idx;
            }
        }
    }
}

static void settings_handle_key_cb(void *ctx, const SDL_Event *event) {
    (void)ctx;
    (void)event;
}

static void settings_update_cb(void *ctx) {
    (void)ctx;
}

static void settings_destroy_cb(void *ctx) {
    free(ctx);
}

/* ---- Public API --------------------------------------------------------- */

app_callbacks_t app_settings_create(TTF_Font *title_font, TTF_Font *label_font,
                                    TTF_Font *value_font) {
    app_callbacks_t cb = { NULL, NULL, NULL, NULL, NULL, NULL };

    settings_ctx_t *s = calloc(1, sizeof(settings_ctx_t));
    if (!s) return cb;

    s->font_title = title_font;
    s->font_label = label_font;
    s->font_value = value_font;
    s->active_category = SETTINGS_DISPLAY;
    s->hover_category = -1;
    s->brightness = 75;

    cb.ctx = s;
    cb.render = settings_render_cb;
    cb.handle_event = settings_handle_event_cb;
    cb.handle_key = settings_handle_key_cb;
    cb.update = settings_update_cb;
    cb.destroy = settings_destroy_cb;
    return cb;
}
