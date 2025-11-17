#include "text_render.h"

#include "font_paths.h"
#include <SDL2/SDL_ttf.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef USE_VULKAN
#define USE_VULKAN 0
#endif

static TTF_Font* g_ts_font = NULL;


static const char* FONT_PATHS[] = {
    FONT_BODY_PATH_1,
    FONT_BODY_PATH_2,
    FONT_TITLE_PATH_1,  // fallback to bold if needed
    FONT_TITLE_PATH_2
};


static bool ensure_font_loaded(void) {
    if (g_ts_font) return true;
    for (size_t i = 0; i < sizeof(FONT_PATHS) / sizeof(FONT_PATHS[0]); ++i) {
        g_ts_font = TTF_OpenFont(FONT_PATHS[i], 16);
        if (g_ts_font) return true;
    }
    fprintf(stderr, "[TimerHUD] Failed to load fallback font for HUD text.\n");
    return false;
}

bool Text_Init(void) {
    if (TTF_WasInit() == 0) {
        if (TTF_Init() == -1) {
            fprintf(stderr, "SDL_ttf init failed: %s\n", TTF_GetError());
            return false;
        }
    }
    return ensure_font_loaded();
}

void Text_Quit(void) {
    if (g_ts_font) {
        TTF_CloseFont(g_ts_font);
        g_ts_font = NULL;
    }
}

SDL_Rect Text_Measure(const char* text) {
    SDL_Rect rect = {0, 0, 0, 0};
    if (!text || !ensure_font_loaded()) return rect;

    TTF_SizeUTF8(g_ts_font, text, &rect.w, &rect.h);
    return rect;
}

int Text_LineHeight(void) {
    if (!ensure_font_loaded()) return 14;
    return TTF_FontHeight(g_ts_font);
}

void Text_Draw(SDL_Renderer* renderer, const char* text, int x, int y, int alignFlags, SDL_Color color) {
    if (!text || !renderer || !ensure_font_loaded()) return;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(g_ts_font, text, color);
    if (!surface) return;

    SDL_Rect dst = {x, y, surface->w, surface->h};

    if (alignFlags & ALIGN_CENTER)  dst.x -= surface->w / 2;
    if (alignFlags & ALIGN_RIGHT)   dst.x -= surface->w;
    if (alignFlags & ALIGN_MIDDLE)  dst.y -= surface->h / 2;
    if (alignFlags & ALIGN_BOTTOM)  dst.y -= surface->h;

#if USE_VULKAN
    (void)color;
    (void)alignFlags;
    (void)renderer;
#else
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
#endif

    SDL_FreeSurface(surface);
}
