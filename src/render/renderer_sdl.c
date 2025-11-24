#include "render/renderer_sdl.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/TimerHUD/src/api/time_scope.h"
#include "render/render_common.h"
#include "render/hud_overlay.h"
#include "render/field_overlay.h"
#include "render/debug_draw_objects.h"
#include "render/velocity_overlay.h"
#include "render/particle_overlay.h"

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture  *g_texture  = NULL;
static SDL_PixelFormat *g_format = NULL;
static bool g_ttf_initialized = false;
static const float DENSITY_VISUAL_SCALE = 0.05f;

static int g_window_w = 0;
static int g_window_h = 0;
static int g_grid_w   = 0;
static int g_grid_h   = 0;

static float *g_density_tmp = NULL;
static float *g_density_blur = NULL;
static size_t g_density_capacity = 0;
static bool g_draw_vorticity = false;
static bool g_draw_pressure = false;
static bool g_draw_velocity_vectors = false;
static bool g_draw_flow_particles = false;
static bool g_velocity_fixed_length = false;

static bool ensure_density_buffers(size_t cell_count) {
    if (cell_count == 0) return false;
    if (cell_count <= g_density_capacity) return true;
    float *tmp = (float *)realloc(g_density_tmp, cell_count * sizeof(float));
    if (!tmp) return false;
    g_density_tmp = tmp;
    float *blur = (float *)realloc(g_density_blur, cell_count * sizeof(float));
    if (!blur) return false;
    g_density_blur = blur;
    g_density_capacity = cell_count;
    return true;
}

bool renderer_sdl_init(int windowW, int windowH, int gridW, int gridH) {
    g_window_w = windowW;
    g_window_h = windowH;
    g_grid_w   = gridW;
    g_grid_h   = gridH;

#if RENDERER_ENABLE_LINEAR_FILTER
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
#else
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
#endif

    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return false;
    }
    g_ttf_initialized = true;

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

    if (!field_overlay_init()) {
        fprintf(stderr, "[renderer] Failed to init field overlays.\n");
    }
    if (!hud_overlay_init(g_renderer)) {
        fprintf(stderr, "[renderer] Failed to init HUD overlay (HUD disabled).\n");
    }
    if (!particle_overlay_init(gridW, gridH)) {
        fprintf(stderr, "[renderer] Failed to init particle overlay.\n");
    }

    return true;
}

void renderer_sdl_shutdown(void) {
    hud_overlay_shutdown();
    field_overlay_shutdown();
    particle_overlay_shutdown();

    if (g_format) {
        SDL_FreeFormat(g_format);
        g_format = NULL;
    }
    if (g_texture) {
        SDL_DestroyTexture(g_texture);
        g_texture = NULL;
    }
    free(g_density_tmp);
    g_density_tmp = NULL;
    free(g_density_blur);
    g_density_blur = NULL;
    g_density_capacity = 0;
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
    if (g_ttf_initialized) {
        TTF_Quit();
        g_ttf_initialized = false;
    }
}


static bool renderer_upload_scene(const SceneState *scene) {
    if (!scene || !scene->smoke || !g_renderer || !g_texture) return false;
    int tex_pitch = 0;
    void *pixels = NULL;
    if (SDL_LockTexture(g_texture, NULL, &pixels, &tex_pitch) != 0) {
        fprintf(stderr, "SDL_LockTexture failed: %s\n", SDL_GetError());
        return false;
    }

    // tex_pitch is in bytes per row
    int w = scene->smoke->w;
    int h = scene->smoke->h;
    size_t cell_count = (size_t)w * (size_t)h;
    if (!ensure_density_buffers(cell_count)) {
        SDL_UnlockTexture(g_texture);
        return false;
    }

    for (size_t i = 0; i < cell_count; ++i) {
        float norm = scene->smoke->density[i] * DENSITY_VISUAL_SCALE;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        g_density_tmp[i] = norm;
    }

    bool blur_enabled = (scene->config && scene->config->enable_render_blur);
#if RENDERER_ENABLE_SMOOTHING
    if (blur_enabled) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sx = x + k;
                    if (sx < 0) sx = 0;
                    if (sx >= w) sx = w - 1;
                    accum += g_density_tmp[(size_t)y * (size_t)w + (size_t)sx] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_density_blur[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sy = y + k;
                    if (sy < 0) sy = 0;
                    if (sy >= h) sy = h - 1;
                    accum += g_density_blur[(size_t)sy * (size_t)w + (size_t)x] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_density_tmp[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
    }
#else
    (void)blur_enabled;
#endif

    for (int y = 0; y < h; ++y) {
        Uint32 *dst = (Uint32 *)((Uint8 *)pixels + y * tex_pitch);
        for (int x = 0; x < w; ++x) {
            size_t i = (size_t)y * (size_t)w + (size_t)x;
            float norm = g_density_tmp[i];
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

    return true;
}

bool renderer_sdl_render_scene(const SceneState *scene) {
    if (!renderer_upload_scene(scene)) return false;

    FieldOverlayConfig overlay_cfg = {
        .draw_vorticity = g_draw_vorticity,
        .draw_pressure  = g_draw_pressure
    };
    field_overlay_apply(scene, g_texture, g_format, &overlay_cfg);

    if (g_draw_flow_particles) {
        double dt = (scene) ? scene->dt : (1.0 / 60.0);
        particle_overlay_update(scene, dt);
    } else {
        particle_overlay_reset();
    }

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    SDL_Rect dst_rect = {0, 0, g_window_w, g_window_h};
    SDL_RenderCopy(g_renderer, g_texture, NULL, &dst_rect);
    debug_draw_object_borders(scene, g_renderer, g_window_w, g_window_h);
    if (g_draw_velocity_vectors) {
        VelocityOverlayConfig vel_cfg = {
            .sample_stride = 4,
            .vector_scale = 0.8f,
            .speed_threshold = 0.02f,
            .fixed_length = g_velocity_fixed_length,
            .fixed_fraction = 0.66f
        };
        velocity_overlay_draw(scene, g_renderer, g_window_w, g_window_h, &vel_cfg);
    }
    if (g_draw_flow_particles) {
        particle_overlay_draw(scene, g_renderer, g_window_w, g_window_h);
    }
    return true;
}

void renderer_sdl_present_with_hud(const RendererHudInfo *hud) {
    hud_overlay_draw(hud);
    ts_render(g_renderer);
    SDL_RenderPresent(g_renderer);
}

bool renderer_sdl_capture_pixels(uint8_t **out_rgba, int *out_pitch) {
    if (!out_rgba || !g_renderer) return false;
    int pitch = g_window_w * 4;
    uint8_t *buffer = (uint8_t *)malloc((size_t)pitch * (size_t)g_window_h);
    if (!buffer) return false;
    if (SDL_RenderReadPixels(g_renderer,
                             NULL,
                             SDL_PIXELFORMAT_ABGR8888,
                             buffer,
                             pitch) != 0) {
        fprintf(stderr, "[renderer] SDL_RenderReadPixels failed: %s\n", SDL_GetError());
        free(buffer);
        return false;
    }
    *out_rgba = buffer;
    if (out_pitch) *out_pitch = pitch;
    return true;
}

void renderer_sdl_free_capture(uint8_t *pixels) {
    free(pixels);
}

int renderer_sdl_output_width(void) {
    return g_window_w;
}

int renderer_sdl_output_height(void) {
    return g_window_h;
}



bool renderer_sdl_toggle_vorticity(void) {
    g_draw_vorticity = !g_draw_vorticity;
    return g_draw_vorticity;
}

bool renderer_sdl_vorticity_enabled(void) {
    return g_draw_vorticity;
}

bool renderer_sdl_toggle_pressure(void) {
    g_draw_pressure = !g_draw_pressure;
    return g_draw_pressure;
}

bool renderer_sdl_pressure_enabled(void) {
    return g_draw_pressure;
}

bool renderer_sdl_toggle_velocity_vectors(void) {
    g_draw_velocity_vectors = !g_draw_velocity_vectors;
    return g_draw_velocity_vectors;
}

bool renderer_sdl_velocity_vectors_enabled(void) {
    return g_draw_velocity_vectors;
}

bool renderer_sdl_toggle_flow_particles(void) {
    g_draw_flow_particles = !g_draw_flow_particles;
    if (!g_draw_flow_particles) {
        particle_overlay_reset();
    }
    return g_draw_flow_particles;
}

bool renderer_sdl_flow_particles_enabled(void) {
    return g_draw_flow_particles;
}

bool renderer_sdl_toggle_velocity_mode(void) {
    g_velocity_fixed_length = !g_velocity_fixed_length;
    return g_velocity_fixed_length;
}

bool renderer_sdl_velocity_mode_fixed(void) {
    return g_velocity_fixed_length;
}
