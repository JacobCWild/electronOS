/* ===========================================================================
 * electronOS Desktop — Settings App
 * ===========================================================================
 * System configuration UI with categories: Display, Network, Security, About.
 * =========================================================================== */

#ifndef ELECTRONOS_DESKTOP_APP_SETTINGS_H
#define ELECTRONOS_DESKTOP_APP_SETTINGS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "window.h"

/* Settings categories */
typedef enum {
    SETTINGS_DISPLAY,
    SETTINGS_NETWORK,
    SETTINGS_SECURITY,
    SETTINGS_ABOUT,
    SETTINGS_CATEGORY_COUNT
} settings_category_t;

/* Settings context */
typedef struct {
    settings_category_t  active_category;
    TTF_Font            *font_title;
    TTF_Font            *font_label;
    TTF_Font            *font_value;
    int                  hover_category;    /* -1 = none */

    /* Display settings */
    int                  brightness;        /* 0-100 */

    /* Scroll state */
    int                  scroll_y;
} settings_ctx_t;

/* Create a settings app context. Returns app_callbacks for window use. */
app_callbacks_t app_settings_create(TTF_Font *title_font, TTF_Font *label_font,
                                    TTF_Font *value_font);

#endif /* ELECTRONOS_DESKTOP_APP_SETTINGS_H */
