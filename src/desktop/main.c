/* ===========================================================================
 * electronOS Desktop — Main Entry Point
 * ===========================================================================
 * Initializes SDL2, creates the desktop compositor, and runs the main
 * event/render loop. Supports --test mode for windowed development.
 *
 * Build: make  (see Makefile)
 * Run:   electronos-desktop [--test]
 * =========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "desktop.h"

/* ---- Constants ---------------------------------------------------------- */
#define WINDOW_TITLE    "electronOS Desktop"
#define TARGET_FPS      60
#define FRAME_DELAY     (1000 / TARGET_FPS)
#define TEST_WIDTH      1280
#define TEST_HEIGHT     720

/* ---- Global state ------------------------------------------------------- */
static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

/* ---- Main --------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    bool test_mode = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            test_mode = true;
        }
    }

    /* Signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
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

    /* Display mode */
    SDL_DisplayMode display_mode;
    if (SDL_GetCurrentDisplayMode(0, &display_mode) != 0) {
        display_mode.w = 1920;
        display_mode.h = 1080;
    }

    int win_flags = SDL_WINDOW_SHOWN;
    int win_w, win_h;

    if (test_mode) {
        win_w = TEST_WIDTH;
        win_h = TEST_HEIGHT;
        win_flags |= SDL_WINDOW_RESIZABLE;
    } else {
        win_w = display_mode.w;
        win_h = display_mode.h;
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
        /* Fall back to software renderer */
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

    /* Initialize desktop */
    desktop_t desktop;
    if (desktop_init(&desktop, renderer, win_w, win_h, test_mode) != 0) {
        fprintf(stderr, "Desktop initialization failed\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    printf("electronOS Desktop started (%dx%d, %s mode)\n",
           win_w, win_h, test_mode ? "test" : "fullscreen");

    /* Main loop */
    while (g_running) {
        Uint32 frame_start = SDL_GetTicks();

        /* Process events */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                g_running = 0;
                break;
            }

            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_RESIZED) {
                win_w = event.window.data1;
                win_h = event.window.data2;
                desktop.screen_w = win_w;
                desktop.screen_h = win_h;
            }

            desktop_handle_event(&desktop, &event);
        }

        /* Update */
        desktop_update(&desktop);

        /* Render */
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        desktop_render(&desktop);
        SDL_RenderPresent(renderer);

        /* Frame rate cap */
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }

    printf("electronOS Desktop shutting down\n");

    /* Cleanup */
    desktop_destroy(&desktop);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
