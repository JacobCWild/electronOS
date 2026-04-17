/* ===========================================================================
 * electronOS Login Manager — UI Implementation
 * ===========================================================================
 * Renders a macOS-inspired login screen:
 *   - Dark gradient background
 *   - Centered logo at top
 *   - User avatar circle with username
 *   - Password field with bullet masking
 *   - Login button with hover/active states
 *   - Clock display at top
 *   - Error shake animation on failed auth
 * =========================================================================== */

#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <SDL2/SDL_image.h>

/* ---- Color palette (macOS-inspired dark theme) -------------------------- */
#define BG_R   13
#define BG_G   13
#define BG_B   13       /* Near-black background       */
#define FG_R  230
#define FG_G  230
#define FG_B  230       /* Primary text                */
#define DIM_R  140
#define DIM_G  140
#define DIM_B  140      /* Secondary/hint text         */
#define FIELD_R  45
#define FIELD_G  45
#define FIELD_B  45     /* Input field background      */
#define FIELD_BORDER_R  80
#define FIELD_BORDER_G  80
#define FIELD_BORDER_B  80
#define ACCENT_R  50
#define ACCENT_G 140
#define ACCENT_B 230    /* Blue accent                 */
#define ERROR_R  230
#define ERROR_G   70
#define ERROR_B   70    /* Red error                   */
#define SUCCESS_R  60
#define SUCCESS_G 180
#define SUCCESS_B  80   /* Green success               */

/* Avatar colors (gradient circle) */
#define AVATAR_R1 100
#define AVATAR_G1 140
#define AVATAR_B1 220
#define AVATAR_R2  60
#define AVATAR_G2 100
#define AVATAR_B2 180

/* ---- Sizes -------------------------------------------------------------- */
#define AVATAR_RADIUS    48
#define FIELD_WIDTH     280
#define FIELD_HEIGHT     40
#define FIELD_RADIUS     8
#define BTN_WIDTH       280
#define BTN_HEIGHT       40
#define PADDING          16

/* ---- Font paths --------------------------------------------------------- */
static const char *FONT_PATHS[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    NULL
};

static const char *find_font(void) {
    for (int i = 0; FONT_PATHS[i]; i++) {
        FILE *f = fopen(FONT_PATHS[i], "r");
        if (f) {
            fclose(f);
            return FONT_PATHS[i];
        }
    }
    return NULL;
}

/* ---- Helpers ------------------------------------------------------------ */

static void render_rounded_rect(SDL_Renderer *r, const SDL_Rect *rect,
                                int radius, Uint8 cr, Uint8 cg, Uint8 cb,
                                Uint8 ca) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);

    /* Fill center */
    SDL_Rect center = {rect->x + radius, rect->y, rect->w - 2*radius, rect->h};
    SDL_RenderFillRect(r, &center);

    /* Fill left/right columns */
    SDL_Rect left = {rect->x, rect->y + radius, radius, rect->h - 2*radius};
    SDL_Rect right = {rect->x + rect->w - radius, rect->y + radius,
                      radius, rect->h - 2*radius};
    SDL_RenderFillRect(r, &left);
    SDL_RenderFillRect(r, &right);

    /* Draw corners */
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy <= radius*radius) {
                /* Top-left */
                SDL_RenderDrawPoint(r, rect->x + radius + dx,
                                    rect->y + radius + dy);
                /* Top-right */
                SDL_RenderDrawPoint(r, rect->x + rect->w - radius - 1 + dx,
                                    rect->y + radius + dy);
                /* Bottom-left */
                SDL_RenderDrawPoint(r, rect->x + radius + dx,
                                    rect->y + rect->h - radius - 1 + dy);
                /* Bottom-right */
                SDL_RenderDrawPoint(r, rect->x + rect->w - radius - 1 + dx,
                                    rect->y + rect->h - radius - 1 + dy);
            }
        }
    }
}

static void render_filled_circle(SDL_Renderer *r, int cx, int cy, int radius,
                                 Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrt((double)(radius * radius - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static SDL_Texture *render_text(SDL_Renderer *renderer, TTF_Font *font,
                                const char *text, SDL_Color color,
                                int *out_w, int *out_h) {
    if (!text || !text[0]) return NULL;

    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return NULL;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (out_w) *out_w = surface->w;
    if (out_h) *out_h = surface->h;
    SDL_FreeSurface(surface);
    return texture;
}

static void render_text_centered(SDL_Renderer *renderer, TTF_Font *font,
                                 const char *text, int cx, int cy,
                                 SDL_Color color) {
    int w, h;
    SDL_Texture *tex = render_text(renderer, font, text, color, &w, &h);
    if (!tex) return;

    SDL_Rect dst = {cx - w/2, cy - h/2, w, h};
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

/* ---- Layout computation ------------------------------------------------- */
static void compute_layout(login_ui_t *ui) {
    int cx = ui->screen_w / 2;
    int cy = ui->screen_h / 2;

    /* Logo: centered, upper third */
    if (ui->logo_tex) {
        int logo_display_w = ui->logo_w;
        int logo_display_h = ui->logo_h;
        int max_w = ui->screen_w / 4;
        if (logo_display_w > max_w) {
            float scale = (float)max_w / logo_display_w;
            logo_display_w = (int)(logo_display_w * scale);
            logo_display_h = (int)(logo_display_h * scale);
        }
        ui->logo_rect.x = cx - logo_display_w / 2;
        ui->logo_rect.y = cy - 180;
        ui->logo_rect.w = logo_display_w;
        ui->logo_rect.h = logo_display_h;
    }

    /* Input fields: centered below avatar area */
    int field_y = cy + 20;

    ui->user_field_rect = (SDL_Rect){
        cx - FIELD_WIDTH/2, field_y, FIELD_WIDTH, FIELD_HEIGHT
    };

    ui->pass_field_rect = (SDL_Rect){
        cx - FIELD_WIDTH/2, field_y, FIELD_WIDTH, FIELD_HEIGHT
    };

    ui->login_btn_rect = (SDL_Rect){
        cx - BTN_WIDTH/2, field_y + FIELD_HEIGHT + PADDING,
        BTN_WIDTH, BTN_HEIGHT
    };

    ui->back_btn_rect = (SDL_Rect){
        cx - 40, field_y + FIELD_HEIGHT + PADDING + BTN_HEIGHT + PADDING,
        80, 30
    };
}

/* ---- Public API --------------------------------------------------------- */

int login_ui_init(login_ui_t *ui, SDL_Renderer *renderer, int w, int h) {
    memset(ui, 0, sizeof(*ui));
    ui->renderer = renderer;
    ui->screen_w = w;
    ui->screen_h = h;
    ui->state = LOGIN_STATE_USERNAME;
    ui->cursor_visible = true;
    ui->cursor_timer = SDL_GetTicks();

    /* Load fonts */
    const char *font_path = find_font();
    if (!font_path) {
        fprintf(stderr, "No suitable font found\n");
        return -1;
    }

    ui->font_title = TTF_OpenFont(font_path, 28);
    ui->font_label = TTF_OpenFont(font_path, 16);
    ui->font_input = TTF_OpenFont(font_path, 18);
    ui->font_hint  = TTF_OpenFont(font_path, 12);
    ui->font_clock = TTF_OpenFont(font_path, 14);

    if (!ui->font_title || !ui->font_label || !ui->font_input ||
        !ui->font_hint || !ui->font_clock) {
        fprintf(stderr, "Failed to load fonts\n");
        return -1;
    }

    /* Load logo */
    SDL_Surface *logo_surf = IMG_Load("/usr/share/electronos/login/logo.png");
    if (!logo_surf) {
        /* Try development path */
        logo_surf = IMG_Load("../../assets/electronOSlogo.png");
    }
    if (!logo_surf) {
        logo_surf = IMG_Load("assets/electronOSlogo.png");
    }
    if (logo_surf) {
        ui->logo_tex = SDL_CreateTextureFromSurface(renderer, logo_surf);
        ui->logo_w = logo_surf->w;
        ui->logo_h = logo_surf->h;
        SDL_FreeSurface(logo_surf);
    }

    /* Default guest username */
    strncpy(ui->username, "Guest", MAX_USERNAME_LEN - 1);
    ui->username_len = 5;

    compute_layout(ui);
    return 0;
}

void login_ui_destroy(login_ui_t *ui) {
    /* Securely clear password */
    memset(ui->password_buf, 0, MAX_PASSWORD_LEN);

    if (ui->logo_tex) SDL_DestroyTexture(ui->logo_tex);
    if (ui->font_title) TTF_CloseFont(ui->font_title);
    if (ui->font_label) TTF_CloseFont(ui->font_label);
    if (ui->font_input) TTF_CloseFont(ui->font_input);
    if (ui->font_hint)  TTF_CloseFont(ui->font_hint);
    if (ui->font_clock) TTF_CloseFont(ui->font_clock);
}

void login_ui_resize(login_ui_t *ui, int w, int h) {
    ui->screen_w = w;
    ui->screen_h = h;
    compute_layout(ui);
}

void login_ui_clear_password(login_ui_t *ui) {
    memset(ui->password_buf, 0, MAX_PASSWORD_LEN);
    ui->password_len = 0;
}

void login_ui_reset(login_ui_t *ui) {
    login_ui_clear_password(ui);
    memset(ui->username, 0, MAX_USERNAME_LEN);
    strncpy(ui->username, "Guest", MAX_USERNAME_LEN - 1);
    ui->username_len = 5;
    ui->state = LOGIN_STATE_USERNAME;
}

void login_ui_handle_event(login_ui_t *ui, const SDL_Event *event) {
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;

        if (key == SDLK_ESCAPE) {
            if (ui->state == LOGIN_STATE_PASSWORD ||
                ui->state == LOGIN_STATE_ERROR) {
                ui->state = LOGIN_STATE_USERNAME;
                login_ui_clear_password(ui);
            }
            return;
        }

        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            if (ui->state == LOGIN_STATE_USERNAME) {
                if (ui->username_len > 0) {
                    ui->state = LOGIN_STATE_PASSWORD;
                }
            } else if (ui->state == LOGIN_STATE_PASSWORD) {
                ui->state = LOGIN_STATE_SUBMITTED;
            } else if (ui->state == LOGIN_STATE_ERROR) {
                ui->state = LOGIN_STATE_PASSWORD;
            }
            return;
        }

        if (key == SDLK_TAB) {
            if (ui->state == LOGIN_STATE_USERNAME && ui->username_len > 0) {
                ui->state = LOGIN_STATE_PASSWORD;
            }
            return;
        }

        if (key == SDLK_BACKSPACE) {
            if (ui->state == LOGIN_STATE_USERNAME && ui->username_len > 0) {
                ui->username[--ui->username_len] = '\0';
            } else if ((ui->state == LOGIN_STATE_PASSWORD ||
                        ui->state == LOGIN_STATE_ERROR) &&
                       ui->password_len > 0) {
                if (ui->state == LOGIN_STATE_ERROR) {
                    ui->state = LOGIN_STATE_PASSWORD;
                }
                ui->password_buf[--ui->password_len] = '\0';
            }
            return;
        }
    }

    if (event->type == SDL_TEXTINPUT) {
        const char *text = event->text.text;
        int len = (int)strlen(text);

        if (ui->state == LOGIN_STATE_USERNAME) {
            if (ui->username_len + len < MAX_USERNAME_LEN) {
                memcpy(ui->username + ui->username_len, text, len);
                ui->username_len += len;
                ui->username[ui->username_len] = '\0';
            }
        } else if (ui->state == LOGIN_STATE_PASSWORD ||
                   ui->state == LOGIN_STATE_ERROR) {
            if (ui->state == LOGIN_STATE_ERROR) {
                ui->state = LOGIN_STATE_PASSWORD;
                login_ui_clear_password(ui);
            }
            if (ui->password_len + len < MAX_PASSWORD_LEN) {
                memcpy(ui->password_buf + ui->password_len, text, len);
                ui->password_len += len;
                ui->password_buf[ui->password_len] = '\0';
            }
        }
    }

    /* Mouse click on login button */
    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;

        /* Click on login button */
        if (ui->state == LOGIN_STATE_PASSWORD &&
            mx >= ui->login_btn_rect.x &&
            mx <= ui->login_btn_rect.x + ui->login_btn_rect.w &&
            my >= ui->login_btn_rect.y &&
            my <= ui->login_btn_rect.y + ui->login_btn_rect.h) {
            ui->state = LOGIN_STATE_SUBMITTED;
        }

        /* Click on "Back" */
        if ((ui->state == LOGIN_STATE_PASSWORD ||
             ui->state == LOGIN_STATE_ERROR) &&
            mx >= ui->back_btn_rect.x &&
            mx <= ui->back_btn_rect.x + ui->back_btn_rect.w &&
            my >= ui->back_btn_rect.y &&
            my <= ui->back_btn_rect.y + ui->back_btn_rect.h) {
            ui->state = LOGIN_STATE_USERNAME;
            login_ui_clear_password(ui);
        }
    }
}

void login_ui_render(login_ui_t *ui) {
    SDL_Renderer *r = ui->renderer;
    Uint32 now = SDL_GetTicks();

    /* Update cursor blink */
    if (now - ui->cursor_timer >= CURSOR_BLINK_MS) {
        ui->cursor_visible = !ui->cursor_visible;
        ui->cursor_timer = now;
    }

    /* Clear error state after timeout */
    if (ui->state == LOGIN_STATE_ERROR &&
        now - ui->error_timer >= ERROR_DISPLAY_MS) {
        ui->state = LOGIN_STATE_PASSWORD;
    }

    /* ---- Background gradient -------------------------------------------- */
    for (int y = 0; y < ui->screen_h; y++) {
        float t = (float)y / ui->screen_h;
        Uint8 c = (Uint8)(13 + t * 5);  /* Very subtle gradient */
        SDL_SetRenderDrawColor(r, c, c, (Uint8)(c + 2), 255);
        SDL_RenderDrawLine(r, 0, y, ui->screen_w, y);
    }

    /* ---- Clock at top center -------------------------------------------- */
    {
        time_t now_t = time(NULL);
        struct tm *tm = localtime(&now_t);
        char clock_buf[64];
        strftime(clock_buf, sizeof(clock_buf), "%A, %B %d    %I:%M %p", tm);
        SDL_Color clock_color = {FG_R, FG_G, FG_B, 200};
        render_text_centered(r, ui->font_clock, clock_buf,
                             ui->screen_w / 2, 30, clock_color);
    }

    int cx = ui->screen_w / 2;

    /* ---- Logo ----------------------------------------------------------- */
    if (ui->logo_tex) {
        SDL_RenderCopy(r, ui->logo_tex, NULL, &ui->logo_rect);
    }

    /* ---- Avatar circle -------------------------------------------------- */
    int avatar_cy = ui->logo_rect.y + ui->logo_rect.h + 50;
    render_filled_circle(r, cx, avatar_cy, AVATAR_RADIUS,
                         AVATAR_R1, AVATAR_G1, AVATAR_B1, 255);
    render_filled_circle(r, cx, avatar_cy, AVATAR_RADIUS - 3,
                         AVATAR_R2, AVATAR_G2, AVATAR_B2, 255);

    /* Person silhouette in avatar */
    render_filled_circle(r, cx, avatar_cy - 12, 14, 180, 200, 230, 255);
    render_filled_circle(r, cx, avatar_cy + 28, 24, 180, 200, 230, 255);

    /* ---- Username display / field --------------------------------------- */
    SDL_Color white = {FG_R, FG_G, FG_B, 255};
    SDL_Color dim   = {DIM_R, DIM_G, DIM_B, 255};
    SDL_Color error_col = {ERROR_R, ERROR_G, ERROR_B, 255};

    int content_y = avatar_cy + AVATAR_RADIUS + 20;

    if (ui->state == LOGIN_STATE_USERNAME) {
        /* Show username field */
        render_text_centered(r, ui->font_label, "Enter Username",
                             cx, content_y, dim);

        SDL_Rect field = {cx - FIELD_WIDTH/2, content_y + 25,
                          FIELD_WIDTH, FIELD_HEIGHT};
        render_rounded_rect(r, &field, FIELD_RADIUS,
                            FIELD_R, FIELD_G, FIELD_B, 255);

        /* Field border */
        SDL_SetRenderDrawColor(r, FIELD_BORDER_R, FIELD_BORDER_G,
                               FIELD_BORDER_B, 255);
        SDL_RenderDrawRect(r, &field);

        /* Username text */
        if (ui->username_len > 0) {
            int tw, th;
            SDL_Texture *tex = render_text(r, ui->font_input, ui->username,
                                           white, &tw, &th);
            if (tex) {
                SDL_Rect dst = {field.x + 12, field.y + (field.h - th)/2,
                                tw, th};
                SDL_RenderCopy(r, tex, NULL, &dst);
                SDL_DestroyTexture(tex);

                /* Cursor */
                if (ui->cursor_visible) {
                    SDL_SetRenderDrawColor(r, FG_R, FG_G, FG_B, 255);
                    SDL_Rect cursor = {field.x + 12 + tw + 2,
                                       field.y + 8, 2, field.h - 16};
                    SDL_RenderFillRect(r, &cursor);
                }
            }
        } else {
            /* Placeholder */
            render_text_centered(r, ui->font_input, "Username",
                                 cx, field.y + field.h/2,
                                 (SDL_Color){DIM_R, DIM_G, DIM_B, 128});
            if (ui->cursor_visible) {
                SDL_SetRenderDrawColor(r, FG_R, FG_G, FG_B, 255);
                SDL_Rect cursor = {field.x + 12, field.y + 8, 2, field.h - 16};
                SDL_RenderFillRect(r, &cursor);
            }
        }

        /* Hint */
        render_text_centered(r, ui->font_hint,
                             "Press Enter to continue",
                             cx, field.y + field.h + 20, dim);

    } else {
        /* Show username label above password field */
        render_text_centered(r, ui->font_label, ui->username,
                             cx, content_y, white);

        /* Password field */
        SDL_Rect field = {cx - FIELD_WIDTH/2, content_y + 30,
                          FIELD_WIDTH, FIELD_HEIGHT};

        Uint8 border_r = FIELD_BORDER_R, border_g = FIELD_BORDER_G,
              border_b = FIELD_BORDER_B;

        if (ui->state == LOGIN_STATE_ERROR) {
            border_r = ERROR_R; border_g = ERROR_G; border_b = ERROR_B;
        } else if (ui->state == LOGIN_STATE_SUCCESS) {
            border_r = SUCCESS_R; border_g = SUCCESS_G; border_b = SUCCESS_B;
        }

        /* Shake animation on error */
        int shake_offset = 0;
        if (ui->state == LOGIN_STATE_ERROR) {
            Uint32 elapsed = now - ui->error_timer;
            if (elapsed < 400) {
                shake_offset = (int)(sin(elapsed * 0.05) * 8.0 *
                                     (1.0 - elapsed / 400.0));
            }
        }
        field.x += shake_offset;

        render_rounded_rect(r, &field, FIELD_RADIUS,
                            FIELD_R, FIELD_G, FIELD_B, 255);
        SDL_SetRenderDrawColor(r, border_r, border_g, border_b, 255);
        SDL_RenderDrawRect(r, &field);

        /* Password bullets */
        if (ui->password_len > 0) {
            char bullets[MAX_PASSWORD_LEN];
            int blen = ui->password_len < (int)sizeof(bullets) - 1 ?
                       ui->password_len : (int)sizeof(bullets) - 1;
            for (int i = 0; i < blen; i++) bullets[i] = '*';
            bullets[blen] = '\0';

            int tw, th;
            SDL_Texture *tex = render_text(r, ui->font_input, bullets,
                                           white, &tw, &th);
            if (tex) {
                SDL_Rect dst = {field.x + 12, field.y + (field.h - th)/2,
                                tw, th};
                SDL_RenderCopy(r, tex, NULL, &dst);
                SDL_DestroyTexture(tex);

                if (ui->cursor_visible &&
                    ui->state == LOGIN_STATE_PASSWORD) {
                    SDL_SetRenderDrawColor(r, FG_R, FG_G, FG_B, 255);
                    SDL_Rect cursor = {field.x + 12 + tw + 2,
                                       field.y + 8, 2, field.h - 16};
                    SDL_RenderFillRect(r, &cursor);
                }
            }
        } else {
            render_text_centered(r, ui->font_input, "Enter Password",
                                 cx + shake_offset,
                                 field.y + field.h/2,
                                 (SDL_Color){DIM_R, DIM_G, DIM_B, 128});
            if (ui->cursor_visible && ui->state == LOGIN_STATE_PASSWORD) {
                SDL_SetRenderDrawColor(r, FG_R, FG_G, FG_B, 255);
                SDL_Rect cursor = {field.x + 12, field.y + 8, 2, field.h - 16};
                SDL_RenderFillRect(r, &cursor);
            }
        }

        /* Login button */
        int btn_y = field.y + field.h + PADDING;
        SDL_Rect btn = {cx - BTN_WIDTH/2 + shake_offset, btn_y,
                        BTN_WIDTH, BTN_HEIGHT};
        ui->login_btn_rect = btn;

        if (ui->state == LOGIN_STATE_SUCCESS) {
            render_rounded_rect(r, &btn, FIELD_RADIUS,
                                SUCCESS_R, SUCCESS_G, SUCCESS_B, 255);
            render_text_centered(r, ui->font_label, "Welcome!",
                                 cx, btn.y + btn.h/2, white);
        } else {
            render_rounded_rect(r, &btn, FIELD_RADIUS,
                                ACCENT_R, ACCENT_G, ACCENT_B, 255);
            render_text_centered(r, ui->font_label, "Log In",
                                 cx, btn.y + btn.h/2, white);
        }

        /* Error message */
        if (ui->state == LOGIN_STATE_ERROR) {
            render_text_centered(r, ui->font_hint,
                                 "Incorrect password. Please try again.",
                                 cx, btn.y + btn.h + 16, error_col);
        }

        /* Back button */
        SDL_Rect back = {cx - 40, btn.y + btn.h + PADDING + 10, 80, 24};
        ui->back_btn_rect = back;
        render_text_centered(r, ui->font_hint, "< Back",
                             cx, back.y + back.h/2, dim);
    }

    /* ---- Footer --------------------------------------------------------- */
    {
        SDL_Color footer_color = {DIM_R, DIM_G, DIM_B, 100};
        render_text_centered(r, ui->font_hint, "electronOS v1.0.0",
                             cx, ui->screen_h - 20, footer_color);
    }
}
