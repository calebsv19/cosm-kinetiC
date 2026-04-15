#include "render/timer_hud_adapter.h"

#include "timer_hud/time_scope.h"
#include "timer_hud/timer_hud_backend.h"
#include "font_paths.h"
#include "render/text_upload_policy.h"
#include "render/renderer_sdl.h"
#include "vk_renderer.h"

#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static SDL_Renderer* g_timer_hud_renderer = NULL;
static TTF_Font* g_timer_hud_font = NULL;

static const char* FONT_PATHS[] = {
    FONT_BODY_PATH_1,
    FONT_BODY_PATH_2,
    FONT_TITLE_PATH_1,
    FONT_TITLE_PATH_2
};

static bool ensure_font_loaded(void) {
    int point_size = 16;
    if (g_timer_hud_font) return true;
    point_size = physics_sim_text_raster_point_size(g_timer_hud_renderer, 16, 8);
    for (size_t i = 0; i < sizeof(FONT_PATHS) / sizeof(FONT_PATHS[0]); ++i) {
        g_timer_hud_font = TTF_OpenFont(FONT_PATHS[i], point_size);
        if (g_timer_hud_font) return true;
    }
    fprintf(stderr, "[TimerHUD] Failed to load fallback font for HUD text.\n");
    return false;
}

static void timer_hud_backend_init(void) {
    if (TTF_WasInit() == 0) {
        if (TTF_Init() == -1) {
            fprintf(stderr, "SDL_ttf init failed: %s\n", TTF_GetError());
        }
    }
    ensure_font_loaded();
}

static void timer_hud_backend_shutdown(void) {
    if (TTF_WasInit() == 0) {
        g_timer_hud_font = NULL;
        return;
    }
    if (g_timer_hud_font) {
        TTF_CloseFont(g_timer_hud_font);
        g_timer_hud_font = NULL;
    }
}

static int timer_hud_get_screen_size(int* out_w, int* out_h) {
    if (!g_timer_hud_renderer || !out_w || !out_h) return 0;
    *out_w = renderer_sdl_output_width();
    *out_h = renderer_sdl_output_height();
    return 1;
}

static int timer_hud_measure_text(const char* text, int* out_w, int* out_h) {
    int raster_w = 0;
    int raster_h = 0;
    if (!text || !ensure_font_loaded()) return 0;
    if (TTF_SizeUTF8(g_timer_hud_font, text, &raster_w, &raster_h) != 0) return 0;
    if (out_w) *out_w = physics_sim_text_logical_pixels(g_timer_hud_renderer, raster_w);
    if (out_h) *out_h = physics_sim_text_logical_pixels(g_timer_hud_renderer, raster_h);
    return 1;
}

static int timer_hud_line_height(void) {
    int raster_h = 0;
    if (!ensure_font_loaded()) return 0;
    raster_h = TTF_FontHeight(g_timer_hud_font);
    return physics_sim_text_logical_pixels(g_timer_hud_renderer, raster_h);
}

static void timer_hud_draw_rect(int x, int y, int w, int h, TimerHUDColor color) {
    if (!g_timer_hud_renderer) return;
    vk_renderer_set_draw_color((VkRenderer*)g_timer_hud_renderer,
                               color.r / 255.0f,
                               color.g / 255.0f,
                               color.b / 255.0f,
                               color.a / 255.0f);
    SDL_Rect rect = {x, y, w, h};
    vk_renderer_fill_rect((VkRenderer*)g_timer_hud_renderer, &rect);
}

static void timer_hud_draw_line(int x1, int y1, int x2, int y2, TimerHUDColor color) {
    if (!g_timer_hud_renderer) return;
    vk_renderer_set_draw_color((VkRenderer*)g_timer_hud_renderer,
                               color.r / 255.0f,
                               color.g / 255.0f,
                               color.b / 255.0f,
                               color.a / 255.0f);
    SDL_RenderDrawLine(g_timer_hud_renderer, x1, y1, x2, y2);
}

static void timer_hud_draw_text(const char* text, int x, int y, int align_flags, TimerHUDColor color) {
    if (!text || !g_timer_hud_renderer || !ensure_font_loaded()) return;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(g_timer_hud_font, text,
                                                  (SDL_Color){color.r, color.g, color.b, color.a});
    if (!surface) return;

    int logical_w = physics_sim_text_logical_pixels(g_timer_hud_renderer, surface->w);
    int logical_h = physics_sim_text_logical_pixels(g_timer_hud_renderer, surface->h);
    SDL_Rect dst = {x, y, logical_w, logical_h};

    if (align_flags & TIMER_HUD_ALIGN_CENTER)  dst.x -= logical_w / 2;
    if (align_flags & TIMER_HUD_ALIGN_RIGHT)   dst.x -= logical_w;
    if (align_flags & TIMER_HUD_ALIGN_MIDDLE)  dst.y -= logical_h / 2;
    if (align_flags & TIMER_HUD_ALIGN_BOTTOM)  dst.y -= logical_h;

    VkRendererTexture texture = {0};
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)g_timer_hud_renderer,
                                                   surface,
                                                   &texture,
                                                   physics_sim_text_upload_filter(g_timer_hud_renderer)) == VK_SUCCESS) {
        vk_renderer_draw_texture((VkRenderer*)g_timer_hud_renderer, &texture, NULL, &dst);
        vk_renderer_queue_texture_destroy((VkRenderer*)g_timer_hud_renderer, &texture);
    }

    SDL_FreeSurface(surface);
}

static const TimerHUDBackend g_timer_hud_backend = {
    .init = timer_hud_backend_init,
    .shutdown = timer_hud_backend_shutdown,
    .get_screen_size = timer_hud_get_screen_size,
    .measure_text = timer_hud_measure_text,
    .get_line_height = timer_hud_line_height,
    .draw_rect = timer_hud_draw_rect,
    .draw_line = timer_hud_draw_line,
    .draw_text = timer_hud_draw_text,
    .hud_padding = 6,
    .hud_spacing = 4,
    .hud_bg_alpha = 40
};

void timer_hud_register_backend(void) {
    ts_register_backend(&g_timer_hud_backend);
    ts_set_program_name("physics_sim");

    const char* outputRoot = getenv("TIMERHUD_OUTPUT_ROOT");
    if (outputRoot && outputRoot[0]) {
        ts_set_output_root(outputRoot);
    }

    const char* overridePath = getenv("PHYSICS_SIM_TIMER_HUD_SETTINGS");
    if (overridePath && overridePath[0]) {
        ts_set_settings_path(overridePath);
    }
}

void timer_hud_bind_renderer(SDL_Renderer* renderer) {
    if (g_timer_hud_renderer == renderer) {
        return;
    }
    if (g_timer_hud_font) {
        TTF_CloseFont(g_timer_hud_font);
        g_timer_hud_font = NULL;
    }
    g_timer_hud_renderer = renderer;
}
