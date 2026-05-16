#include "render/timer_hud_adapter.h"

#include <stdlib.h>

#include "timer_hud/settings_loader.h"
#include "timer_hud/time_scope.h"
#include "timer_hud/timer_hud_backend.h"
#include "render/font_bridge.h"
#include "render/text_draw.h"
#include "render/text_upload_policy.h"
#include "render/renderer_sdl.h"
#include "vk_renderer.h"

#include <SDL2/SDL_ttf.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define TIMER_HUD_DEFAULT_SETTINGS_REL_PATH "config/timer_hud_settings.json"
#define TIMER_HUD_RUNTIME_SETTINGS_REL_PATH "data/runtime/timer_hud_settings.json"

static SDL_Renderer* g_timer_hud_renderer = NULL;
static TTF_Font* g_timer_hud_font = NULL;

static int timer_hud_resolve_abs_from_cwd(const char* relative_path,
                                          char* out,
                                          size_t out_cap) {
    char cwd[PATH_MAX] = {0};
    int written = 0;
    if (!relative_path || !relative_path[0] || !out || out_cap == 0) {
        return 0;
    }
    if (relative_path[0] == '/') {
        written = snprintf(out, out_cap, "%s", relative_path);
        return written > 0 && (size_t)written < out_cap;
    }
    if (!getcwd(cwd, sizeof(cwd))) {
        return 0;
    }
    written = snprintf(out, out_cap, "%s/%s", cwd, relative_path);
    return written > 0 && (size_t)written < out_cap;
}

static bool timer_hud_parse_bool_token(const char* value, bool* out_enabled) {
    if (!value || !out_enabled) {
        return false;
    }
    if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "on") == 0 || strcasecmp(value, "yes") == 0) {
        *out_enabled = true;
        return true;
    }
    if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "off") == 0 || strcasecmp(value, "no") == 0) {
        *out_enabled = false;
        return true;
    }
    return false;
}

static bool timer_hud_is_valid_visual_mode(const char* value) {
    if (!value || !value[0]) {
        return false;
    }
    return strcmp(value, "text_compact") == 0 ||
           strcmp(value, "history_graph") == 0 ||
           strcmp(value, "hybrid") == 0;
}

static bool ensure_font_loaded(void) {
    if (g_timer_hud_font) return true;
    (void)physics_sim_font_bridge_open(g_timer_hud_renderer,
                                       NULL,
                                       PHYSICS_SIM_FONT_SLOT_TIMER_HUD,
                                       &g_timer_hud_font,
                                       NULL);
    if (g_timer_hud_font) return true;
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
    physics_sim_font_bridge_close(&g_timer_hud_font);
}

static int timer_hud_get_screen_size(int* out_w, int* out_h) {
    if (!g_timer_hud_renderer || !out_w || !out_h) return 0;
    *out_w = renderer_sdl_output_width();
    *out_h = renderer_sdl_output_height();
    return 1;
}

static int timer_hud_measure_text(const char* text, int* out_w, int* out_h) {
    if (!text || !ensure_font_loaded()) return 0;
    return physics_sim_text_measure_utf8(g_timer_hud_renderer, g_timer_hud_font, text, out_w, out_h);
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

    int logical_w = 0;
    int logical_h = 0;
    if (!physics_sim_text_measure_utf8(g_timer_hud_renderer, g_timer_hud_font, text, &logical_w, &logical_h)) {
        return;
    }
    SDL_Rect dst = {x, y, logical_w, logical_h};

    if (align_flags & TIMER_HUD_ALIGN_CENTER)  dst.x -= logical_w / 2;
    if (align_flags & TIMER_HUD_ALIGN_RIGHT)   dst.x -= logical_w;
    if (align_flags & TIMER_HUD_ALIGN_MIDDLE)  dst.y -= logical_h / 2;
    if (align_flags & TIMER_HUD_ALIGN_BOTTOM)  dst.y -= logical_h;

    (void)physics_sim_text_draw_utf8(g_timer_hud_renderer,
                                     g_timer_hud_font,
                                     text,
                                     (SDL_Color){color.r, color.g, color.b, color.a},
                                     &dst);
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
    char default_settings_path[PATH_MAX] = {0};
    char runtime_settings_path[PATH_MAX] = {0};

    ts_register_backend(&g_timer_hud_backend);
    ts_set_program_name("physics_sim");

    const char* outputRoot = getenv("TIMERHUD_OUTPUT_ROOT");
    if (outputRoot && outputRoot[0]) {
        ts_set_output_root(outputRoot);
    }

    if (timer_hud_resolve_abs_from_cwd(TIMER_HUD_DEFAULT_SETTINGS_REL_PATH,
                                       default_settings_path,
                                       sizeof(default_settings_path)) &&
        timer_hud_resolve_abs_from_cwd(TIMER_HUD_RUNTIME_SETTINGS_REL_PATH,
                                       runtime_settings_path,
                                       sizeof(runtime_settings_path))) {
        (void)default_settings_path;
        ts_set_settings_path(runtime_settings_path);
    }

    const char* overridePath = getenv("PHYSICS_SIM_TIMER_HUD_SETTINGS");
    if (overridePath && overridePath[0]) {
        ts_set_settings_path(overridePath);
    }
}

void timer_hud_apply_startup_env_overrides(void) {
    const char* hudEnv = getenv("PHYSICS_SIM_TIMER_HUD");
    const char* overlayEnv = getenv("PHYSICS_SIM_TIMER_HUD_OVERLAY");
    const char* visualModeEnv = getenv("PHYSICS_SIM_TIMER_HUD_VISUAL_MODE");
    bool enabled = false;
    bool setHybridByDefault = false;

    if (hudEnv && hudEnv[0]) {
        if (timer_hud_parse_bool_token(hudEnv, &enabled)) {
            ts_settings.hud_enabled = enabled;
            setHybridByDefault = enabled;
        } else {
            fprintf(stderr,
                    "[TimerHUD] ignoring invalid PHYSICS_SIM_TIMER_HUD=%s\n",
                    hudEnv);
        }
    }

    if (overlayEnv && overlayEnv[0]) {
        if (timer_hud_parse_bool_token(overlayEnv, &enabled)) {
            ts_settings.hud_enabled = enabled;
            setHybridByDefault = enabled;
        } else {
            fprintf(stderr,
                    "[TimerHUD] ignoring invalid PHYSICS_SIM_TIMER_HUD_OVERLAY=%s\n",
                    overlayEnv);
        }
    }

    if (visualModeEnv && visualModeEnv[0]) {
        if (timer_hud_is_valid_visual_mode(visualModeEnv)) {
            ts_set_hud_visual_mode(visualModeEnv);
        } else {
            fprintf(stderr,
                    "[TimerHUD] ignoring invalid PHYSICS_SIM_TIMER_HUD_VISUAL_MODE=%s\n",
                    visualModeEnv);
        }
    } else if (setHybridByDefault) {
        ts_set_hud_visual_mode("hybrid");
    }

    fprintf(stderr,
            "[TimerHUD] physics_sim startup hud_enabled=%d mode=%s log_enabled=%d log_file=%s\n",
            ts_settings.hud_enabled ? 1 : 0,
            ts_settings.hud_visual_mode,
            ts_settings.log_enabled ? 1 : 0,
            ts_settings.log_filepath);
}

void timer_hud_bind_renderer(SDL_Renderer* renderer) {
    if (g_timer_hud_renderer == renderer) {
        return;
    }
    physics_sim_font_bridge_close(&g_timer_hud_font);
    g_timer_hud_renderer = renderer;
}

void timer_hud_unbind_renderer(void) {
    physics_sim_font_bridge_close(&g_timer_hud_font);
    g_timer_hud_renderer = NULL;
}
