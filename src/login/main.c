/* ===========================================================================
 * electronOS Login Manager
 * ===========================================================================
 * A graphical login screen inspired by macOS, rendered with SDL2.
 * Authenticates users via PAM, falls back to Guest if no accounts exist.
 *
 * Build: make  (see Makefile)
 * Run:   electronos-login [--test]
 * ===========================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <sys/wait.h>
#include <grp.h>
#include <errno.h>
#include <time.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "auth.h"
#include "ui.h"

/* ---- Constants ---------------------------------------------------------- */
#define WINDOW_TITLE    "electronOS Login"
#define MIN_WIDTH       800
#define MIN_HEIGHT      600
#define TARGET_FPS      60
#define FRAME_DELAY     (1000 / TARGET_FPS)

/* ---- Global state ------------------------------------------------------- */
static volatile sig_atomic_t g_running = 1;
static bool g_test_mode = false;

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

/* ---- Launch user session ------------------------------------------------ */
static void launch_session(const char *username, const char *shell) {
    struct passwd *pw = getpwnam(username);
    if (!pw) {
        fprintf(stderr, "electronos-login: user '%s' not found\n", username);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        /* Child: set up environment and exec the user shell */
        if (initgroups(pw->pw_name, pw->pw_gid) != 0) {
            perror("initgroups");
            _exit(1);
        }
        if (setgid(pw->pw_gid) != 0) {
            perror("setgid");
            _exit(1);
        }
        if (setuid(pw->pw_uid) != 0) {
            perror("setuid");
            _exit(1);
        }

        /* Set standard environment */
        setenv("HOME", pw->pw_dir, 1);
        setenv("USER", pw->pw_name, 1);
        setenv("LOGNAME", pw->pw_name, 1);
        setenv("SHELL", shell, 1);
        setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);
        setenv("TERM", "linux", 1);

        if (chdir(pw->pw_dir) != 0) {
            if (chdir("/") != 0) {
                perror("chdir");
            }
        }

        /* Execute shell as login shell */
        const char *shell_basename = strrchr(shell, '/');
        if (shell_basename) {
            shell_basename++;
        } else {
            shell_basename = shell;
        }

        char login_name[256];
        snprintf(login_name, sizeof(login_name), "-%s", shell_basename);

        execl(shell, login_name, (char *)NULL);
        perror("execl");
        _exit(1);
    }

    /* Parent: wait for session to end, then re-show login */
    int status;
    waitpid(pid, &status, 0);
}

/* ---- Main --------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            g_test_mode = true;
        }
    }

    /* Set up signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    /* SIGCHLD: use SIG_DFL so child exits don't terminate the login loop
     * but waitpid() still works correctly (SIG_IGN causes auto-reap which
     * breaks waitpid semantics — it returns -1/ECHILD instead of status). */
    sigaction(SIGCHLD, &(struct sigaction){ .sa_handler = SIG_DFL }, NULL);

    /* Initialize SDL2 */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (IMG_Init(IMG_INIT_PNG) == 0) {
        fprintf(stderr, "IMG_Init failed: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    /* Get display mode for fullscreen */
    SDL_DisplayMode display_mode;
    if (SDL_GetCurrentDisplayMode(0, &display_mode) != 0) {
        display_mode.w = 1920;
        display_mode.h = 1080;
    }

    int win_flags = SDL_WINDOW_SHOWN;
    int win_w = display_mode.w;
    int win_h = display_mode.h;

    if (g_test_mode) {
        win_w = 1280;
        win_h = 720;
        win_flags |= SDL_WINDOW_RESIZABLE;
    } else {
        win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h, win_flags
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    /* Disable cursor in production mode */
    if (!g_test_mode) {
        SDL_ShowCursor(SDL_DISABLE);
    }

    /* Initialize login UI */
    login_ui_t ui;
    if (login_ui_init(&ui, renderer, win_w, win_h) != 0) {
        fprintf(stderr, "Failed to initialize login UI\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    /* Main loop */
    while (g_running) {
        Uint32 frame_start = SDL_GetTicks();

        /* Handle events */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                g_running = 0;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    login_ui_resize(&ui, event.window.data1,
                                    event.window.data2);
                }
                break;

            default:
                login_ui_handle_event(&ui, &event);
                break;
            }
        }

        /* Check if login was submitted */
        if (ui.state == LOGIN_STATE_SUBMITTED) {
            auth_session_t auth_session = { .pamh = NULL, .conv = NULL };
            auth_result_t result = auth_authenticate(
                ui.username, ui.password_buf, &auth_session
            );

            if (result == AUTH_SUCCESS) {
                ui.state = LOGIN_STATE_SUCCESS;
                login_ui_render(&ui);
                SDL_RenderPresent(renderer);
                SDL_Delay(800);  /* Brief success animation */

                /* Determine shell */
                const char *shell = "/usr/bin/electronos-shell";
                struct passwd *pw = getpwnam(ui.username);
                if (pw && pw->pw_shell && strlen(pw->pw_shell) > 0) {
                    shell = pw->pw_shell;
                }

                /* Clear sensitive data */
                login_ui_clear_password(&ui);

                if (g_test_mode) {
                    printf("LOGIN OK: user=%s shell=%s\n", ui.username, shell);
                    auth_close_session(&auth_session);
                    g_running = 0;
                } else {
                    /* Hide window, launch session, show again */
                    SDL_HideWindow(window);
                    launch_session(ui.username, shell);
                    /* Close PAM session after user logs out */
                    auth_close_session(&auth_session);
                    SDL_ShowWindow(window);
                    login_ui_reset(&ui);
                }
            } else {
                ui.state = LOGIN_STATE_ERROR;
                ui.error_timer = SDL_GetTicks();
                login_ui_clear_password(&ui);

                /* Rate limit: brief delay on failure */
                SDL_Delay(500);
            }
        }

        /* Render */
        login_ui_render(&ui);
        SDL_RenderPresent(renderer);

        /* Frame rate limiting */
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }

    /* Cleanup */
    login_ui_destroy(&ui);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
