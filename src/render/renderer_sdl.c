#include "render/renderer_sdl.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture  *g_texture  = NULL;
static SDL_PixelFormat *g_format = NULL;
static const float DENSITY_VISUAL_SCALE = 0.05f;

static int g_window_w = 0;
static int g_window_h = 0;
static int g_grid_w   = 0;
static int g_grid_h   = 0;

bool renderer_sdl_init(int windowW, int windowH, int gridW, int gridH) {
    g_window_w = windowW;
    g_window_h = windowH;
    g_grid_w   = gridW;
    g_grid_h   = gridH;

    g_window = SDL_CreateWindow(
        "Physics Sim - Fluid2D",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowW, windowH,
        SDL_WINDOW_SHOWN
    );
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        return false;
    }

    g_texture = SDL_CreateTexture(
        g_renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        gridW, gridH
    );
    if (!g_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        g_renderer = NULL;
        g_window = NULL;
        return false;
    }

    g_format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    if (!g_format) {
        fprintf(stderr, "SDL_AllocFormat failed: %s\n", SDL_GetError());
        renderer_sdl_shutdown();
        return false;
    }

    return true;
}

void renderer_sdl_shutdown(void) {
    if (g_format) {
        SDL_FreeFormat(g_format);
        g_format = NULL;
    }
    if (g_texture) {
        SDL_DestroyTexture(g_texture);
        g_texture = NULL;
    }
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
}

void renderer_sdl_draw(const SceneState *scene) {
    if (!scene || !scene->smoke || !g_renderer || !g_texture) return;

    int tex_pitch = 0;
    void *pixels = NULL;
    if (SDL_LockTexture(g_texture, NULL, &pixels, &tex_pitch) != 0) {
        fprintf(stderr, "SDL_LockTexture failed: %s\n", SDL_GetError());
        return;
    }

    // tex_pitch is in bytes per row
    int w = scene->smoke->w;
    int h = scene->smoke->h;

    for (int y = 0; y < h; ++y) {
        Uint32 *dst = (Uint32 *)((Uint8 *)pixels + y * tex_pitch);
        for (int x = 0; x < w; ++x) {
            size_t i = (size_t)y * (size_t)w + (size_t)x;
            float d = scene->smoke->density[i];
            float norm = d * DENSITY_VISUAL_SCALE;
            if (norm < 0.0f) norm = 0.0f;
            if (norm > 1.0f) norm = 1.0f;

            Uint8 c = (Uint8)(norm * 255.0f);
            Uint32 pixel = g_format
                ? SDL_MapRGBA(g_format, c, c, c, 255)
                : (Uint32)(0xFFu << 24 | (c << 16) | (c << 8) | c);
            dst[x] = pixel;
        }
    }

    SDL_UnlockTexture(g_texture);

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    SDL_Rect dst_rect = {0, 0, g_window_w, g_window_h};
    SDL_RenderCopy(g_renderer, g_texture, NULL, &dst_rect);

    SDL_RenderPresent(g_renderer);
}
