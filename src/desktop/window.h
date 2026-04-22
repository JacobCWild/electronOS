/* ===========================================================================
 * electronOS Desktop — Window Management
 * ===========================================================================
 * Manages desktop windows with glass titlebars, move/resize/close controls.
 * =========================================================================== */

#ifndef ELECTRONOS_DESKTOP_WINDOW_H
#define ELECTRONOS_DESKTOP_WINDOW_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct desktop_window desktop_window_t;

/* App type identifiers */
typedef enum {
    APP_TERMINAL,
    APP_SETTINGS,
    APP_EDITOR,
    APP_COUNT
} app_type_t;

/* App callbacks for window content */
typedef struct {
    void *ctx;  /* App-specific context */
    void (*render)(void *ctx, SDL_Renderer *renderer, SDL_Rect content_area);
    void (*handle_event)(void *ctx, const SDL_Event *event, SDL_Rect content_area);
    void (*handle_key)(void *ctx, const SDL_Event *event);
    void (*update)(void *ctx);
    void (*destroy)(void *ctx);
} app_callbacks_t;

/* Window state */
struct desktop_window {
    int             id;
    char            title[128];
    app_type_t      app_type;
    SDL_Rect        rect;           /* Full window rect including titlebar */
    bool            minimized;
    bool            focused;
    bool            maximized;
    SDL_Rect        restore_rect;   /* Rect before maximize */

    /* Drag state */
    bool            dragging;
    int             drag_offset_x;
    int             drag_offset_y;

    /* Resize state */
    bool            resizing;
    int             resize_edge;    /* Bitmask: 1=left,2=right,4=top,8=bottom */

    /* App */
    app_callbacks_t app;

    /* Linked list */
    desktop_window_t *next;
};

/* ---- Titlebar dimensions ------------------------------------------------ */
#define TITLEBAR_HEIGHT     32
#define WINDOW_BORDER       1
#define WINDOW_MIN_W        300
#define WINDOW_MIN_H        200
#define WINDOW_BTN_SIZE     14
#define WINDOW_BTN_MARGIN   8
#define RESIZE_BORDER       6

/* ---- Window management API ---------------------------------------------- */

/* Create a new window. Returns the window pointer. */
desktop_window_t *window_create(const char *title, app_type_t type,
                                int x, int y, int w, int h,
                                app_callbacks_t app);

/* Render a window (titlebar + content) */
void window_render(SDL_Renderer *renderer, desktop_window_t *win,
                   TTF_Font *font);

/* Handle mouse event for a window. Returns true if event was consumed. */
bool window_handle_event(desktop_window_t *win, const SDL_Event *event,
                         int screen_w, int screen_h);

/* Get the content area (below titlebar) */
SDL_Rect window_content_rect(const desktop_window_t *win);

/* Check if a point is inside the window */
bool window_contains_point(const desktop_window_t *win, int x, int y);

/* Check if a point is on the close button */
bool window_close_hit(const desktop_window_t *win, int x, int y);

/* Check if a point is on the minimize button */
bool window_minimize_hit(const desktop_window_t *win, int x, int y);

/* Check if a point is on the maximize button */
bool window_maximize_hit(const desktop_window_t *win, int x, int y);

/* Toggle maximize state */
void window_toggle_maximize(desktop_window_t *win, int screen_w, int screen_h,
                            int panel_h, int dock_w);

/* Destroy a window */
void window_destroy(desktop_window_t *win);

#endif /* ELECTRONOS_DESKTOP_WINDOW_H */
