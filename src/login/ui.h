/* ===========================================================================
 * electronOS Login Manager — UI Header
 * =========================================================================== */

#ifndef ELECTRONOS_LOGIN_UI_H
#define ELECTRONOS_LOGIN_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

/* ---- Constants ---------------------------------------------------------- */
#define MAX_USERNAME_LEN   64
#define MAX_PASSWORD_LEN   128
#define ERROR_DISPLAY_MS   3000
#define CURSOR_BLINK_MS    530

/* ---- Login states ------------------------------------------------------- */
typedef enum {
    LOGIN_STATE_USERNAME,     /* Entering username                           */
    LOGIN_STATE_PASSWORD,     /* Entering password                           */
    LOGIN_STATE_SUBMITTED,    /* Credentials submitted, awaiting auth        */
    LOGIN_STATE_SUCCESS,      /* Auth succeeded, launching session           */
    LOGIN_STATE_ERROR,        /* Auth failed, showing error message          */
} login_state_t;

/* ---- UI context --------------------------------------------------------- */
typedef struct {
    SDL_Renderer   *renderer;
    int             screen_w;
    int             screen_h;

    /* Fonts */
    TTF_Font       *font_title;     /* 28pt for "electronOS"                */
    TTF_Font       *font_label;     /* 16pt for labels                      */
    TTF_Font       *font_input;     /* 18pt for text fields                 */
    TTF_Font       *font_hint;      /* 12pt for hints                       */
    TTF_Font       *font_clock;     /* 14pt for clock                       */

    /* Logo */
    SDL_Texture    *logo_tex;
    int             logo_w;
    int             logo_h;

    /* State */
    login_state_t   state;
    char            username[MAX_USERNAME_LEN];
    int             username_len;
    char            password_buf[MAX_PASSWORD_LEN];
    int             password_len;
    Uint32          error_timer;
    Uint32          cursor_timer;
    bool            cursor_visible;

    /* Layout (computed on resize) */
    SDL_Rect        logo_rect;
    SDL_Rect        user_field_rect;
    SDL_Rect        pass_field_rect;
    SDL_Rect        login_btn_rect;
    SDL_Rect        back_btn_rect;
} login_ui_t;

/* ---- API ---------------------------------------------------------------- */
int  login_ui_init(login_ui_t *ui, SDL_Renderer *renderer, int w, int h);
void login_ui_destroy(login_ui_t *ui);
void login_ui_resize(login_ui_t *ui, int w, int h);
void login_ui_handle_event(login_ui_t *ui, const SDL_Event *event);
void login_ui_render(login_ui_t *ui);
void login_ui_reset(login_ui_t *ui);
void login_ui_clear_password(login_ui_t *ui);

#endif /* ELECTRONOS_LOGIN_UI_H */
