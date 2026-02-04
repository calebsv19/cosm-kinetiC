#include "render/renderer_sdl.h"
#include "render/timer_hud_adapter.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "timer_hud/time_scope.h"
#include "render/render_common.h"
#include "render/hud_overlay.h"
#include "render/field_overlay.h"
#include "render/debug_draw_objects.h"
#include "render/velocity_overlay.h"
#include "render/particle_overlay.h"
#include "render/vk_shared_device.h"
#include "vk_renderer.h"

static SDL_Window   *g_window   = NULL;
static VkRenderer    g_renderer_storage;
static SDL_Renderer *g_renderer = NULL;
static bool g_use_shared_device = true;
static const float DENSITY_VISUAL_SCALE = 0.05f;
static uint8_t g_base_black_level = 0;

static int g_window_w = 0;
static int g_window_h = 0;
static int g_grid_w   = 0;
static int g_grid_h   = 0;

static float *g_density_tmp = NULL;
static float *g_density_blur = NULL;
static size_t g_density_capacity = 0;
static uint8_t *g_scene_pixels = NULL;
static size_t g_scene_capacity = 0;
static int g_scene_pitch = 0;
static uint8_t *g_capture_pixels = NULL;
static size_t g_capture_capacity = 0;
static VkCommandBuffer g_frame_cmd = VK_NULL_HANDLE;
static bool g_frame_active = false;
static bool g_device_lost = false;
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
    g_base_black_level = 0;

#if RENDERER_ENABLE_LINEAR_FILTER
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
#else
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
#endif

    if (TTF_WasInit() == 0) {
        if (TTF_Init() != 0) {
            fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
            return false;
        }
    }

    g_window = SDL_CreateWindow(
        "Physics Sim - Fluid2D",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowW, windowH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    VkRendererConfig cfg;
    vk_renderer_config_set_defaults(&cfg);
    cfg.enable_validation = SDL_FALSE;
    cfg.clear_color[0] = 0.0f;
    cfg.clear_color[1] = 0.0f;
    cfg.clear_color[2] = 0.0f;
    cfg.clear_color[3] = 1.0f;
#if defined(__APPLE__)
    cfg.frames_in_flight = 1;
#endif

#if defined(__APPLE__)
    g_use_shared_device = true;
#else
    g_use_shared_device = true;
#endif

    if (g_use_shared_device) {
        if (!vk_shared_device_init(g_window, &cfg)) {
            fprintf(stderr, "vk_shared_device_init failed.\n");
            SDL_DestroyWindow(g_window);
            g_window = NULL;
            return false;
        }

        VkRendererDevice* shared_device = vk_shared_device_get();
        if (!shared_device) {
            fprintf(stderr, "vk_shared_device_get failed.\n");
            SDL_DestroyWindow(g_window);
            g_window = NULL;
            return false;
        }

        VkResult init = vk_renderer_init_with_device(&g_renderer_storage, shared_device, g_window, &cfg);
        if (init != VK_SUCCESS) {
            fprintf(stderr, "vk_renderer_init failed: %d\n", init);
            SDL_DestroyWindow(g_window);
            g_window = NULL;
            return false;
        }
        vk_shared_device_acquire();
    } else {
        VkResult init = vk_renderer_init(&g_renderer_storage, g_window, &cfg);
        if (init != VK_SUCCESS) {
            fprintf(stderr, "vk_renderer_init failed: %d\n", init);
            SDL_DestroyWindow(g_window);
            g_window = NULL;
            return false;
        }
    }
    g_renderer = (SDL_Renderer *)&g_renderer_storage;
    vk_renderer_set_logical_size((VkRenderer *)g_renderer,
                                 (float)windowW,
                                 (float)windowH);

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

    free(g_density_tmp);
    g_density_tmp = NULL;
    free(g_density_blur);
    g_density_blur = NULL;
    g_density_capacity = 0;
    free(g_scene_pixels);
    g_scene_pixels = NULL;
    g_scene_capacity = 0;
    g_scene_pitch = 0;
    free(g_capture_pixels);
    g_capture_pixels = NULL;
    g_capture_capacity = 0;
    g_frame_cmd = VK_NULL_HANDLE;
    g_frame_active = false;
    g_device_lost = false;
    if (g_renderer) {
        vk_renderer_wait_idle((VkRenderer *)g_renderer);
        if (g_use_shared_device) {
            vk_renderer_shutdown_surface((VkRenderer *)g_renderer);
            vk_shared_device_release();
        } else {
            vk_renderer_shutdown((VkRenderer *)g_renderer);
        }
        g_renderer = NULL;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
}

bool renderer_sdl_device_lost(void) {
    return g_device_lost;
}


static bool renderer_upload_scene(const SceneState *scene) {
    if (!scene || !scene->smoke || !g_renderer) return false;
    const AppConfig *cfg = scene->config;
    if (cfg) {
        int level = cfg->render_black_level;
        if (level < 0) level = 0;
        if (level > 255) level = 255;
        g_base_black_level = (uint8_t)level;
    } else {
        g_base_black_level = 0;
    }

    int w = scene->smoke->w;
    int h = scene->smoke->h;
    if (w <= 0 || h <= 0) return false;
    g_grid_w = w;
    g_grid_h = h;

    size_t cell_count = (size_t)w * (size_t)h;
    if (!ensure_density_buffers(cell_count)) {
        return false;
    }

    size_t needed = cell_count * 4u;
    if (needed > g_scene_capacity) {
        uint8_t *resized = (uint8_t *)realloc(g_scene_pixels, needed);
        if (!resized) return false;
        g_scene_pixels = resized;
        g_scene_capacity = needed;
    }
    g_scene_pitch = w * 4;

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
        uint8_t *dst = g_scene_pixels + (size_t)y * (size_t)g_scene_pitch;
        for (int x = 0; x < w; ++x) {
            size_t i = (size_t)y * (size_t)w + (size_t)x;
            float norm = g_density_tmp[i];
            if (norm < 0.0f) norm = 0.0f;
            if (norm > 1.0f) norm = 1.0f;
            float span = 255.0f - (float)g_base_black_level;
            uint8_t c = (uint8_t)lroundf((float)g_base_black_level + norm * span);
            size_t offset = (size_t)x * 4u;
            dst[offset + 0] = c;
            dst[offset + 1] = c;
            dst[offset + 2] = c;
            dst[offset + 3] = 255;
        }
    }

    return true;
}

bool renderer_sdl_render_scene(const SceneState *scene) {
    if (!renderer_upload_scene(scene)) return false;

    FieldOverlayConfig overlay_cfg = {
        .draw_vorticity = g_draw_vorticity,
        .draw_pressure  = g_draw_pressure
    };
    field_overlay_apply(scene, g_scene_pixels, g_scene_pitch, &overlay_cfg);

    {
        double dt = (scene) ? scene->dt : (1.0 / 60.0);
        particle_overlay_update(scene, dt, g_draw_flow_particles);
    }

    if (!g_window || !g_renderer) return false;
    SDL_GetWindowSize(g_window, &g_window_w, &g_window_h);
    if (g_window_w <= 0 || g_window_h <= 0) return false;
    int drawable_w = g_window_w;
    int drawable_h = g_window_h;
    SDL_Vulkan_GetDrawableSize(g_window, &drawable_w, &drawable_h);
    if (drawable_w <= 0 || drawable_h <= 0) {
        return false;
    }
    VkExtent2D swap_extent = ((VkRenderer *)g_renderer)->context.swapchain.extent;
    if ((uint32_t)drawable_w != swap_extent.width ||
        (uint32_t)drawable_h != swap_extent.height) {
        vk_renderer_recreate_swapchain((VkRenderer *)g_renderer, g_window);
        vk_renderer_set_logical_size((VkRenderer *)g_renderer,
                                     (float)g_window_w,
                                     (float)g_window_h);
        return false;
    }

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFramebuffer fb = VK_NULL_HANDLE;
    VkExtent2D extent = {0};
    VkResult frame = vk_renderer_begin_frame((VkRenderer *)g_renderer, &cmd, &fb, &extent);
    if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
        vk_renderer_recreate_swapchain((VkRenderer *)g_renderer, g_window);
        vk_renderer_set_logical_size((VkRenderer *)g_renderer,
                                     (float)g_window_w,
                                     (float)g_window_h);
        return false;
    }
    if (frame == VK_ERROR_DEVICE_LOST) {
        static int logged_device_lost = 0;
        if (!logged_device_lost) {
            fprintf(stderr, "[renderer] Vulkan device lost; stopping render loop.\n");
            logged_device_lost = 1;
        }
        if (g_use_shared_device) {
            vk_shared_device_mark_lost();
        }
        g_device_lost = true;
        return false;
    }
    if (frame != VK_SUCCESS) {
        fprintf(stderr, "[renderer] vk_renderer_begin_frame failed: %d\n", frame);
        return false;
    }

    g_frame_cmd = cmd;
    g_frame_active = true;
    vk_renderer_set_logical_size((VkRenderer *)g_renderer,
                                 (float)g_window_w,
                                 (float)g_window_h);

    VkRendererTexture scene_tex = {0};
    VkFilter filter = RENDERER_ENABLE_LINEAR_FILTER ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    VkResult upload = vk_renderer_texture_create_from_rgba((VkRenderer *)g_renderer,
                                                           g_scene_pixels,
                                                           (uint32_t)g_grid_w,
                                                           (uint32_t)g_grid_h,
                                                           filter,
                                                           &scene_tex);
    if (upload != VK_SUCCESS) {
        fprintf(stderr, "[renderer] vk_renderer_texture_create_from_rgba failed: %d\n", upload);
        vk_renderer_end_frame((VkRenderer *)g_renderer, g_frame_cmd);
        g_frame_cmd = VK_NULL_HANDLE;
        g_frame_active = false;
        return false;
    }

    SDL_Rect dst_rect = {0, 0, g_window_w, g_window_h};
    vk_renderer_draw_texture((VkRenderer *)g_renderer, &scene_tex, NULL, &dst_rect);
    vk_renderer_queue_texture_destroy((VkRenderer *)g_renderer, &scene_tex);
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
    if (!g_renderer || !g_frame_active) return;
    hud_overlay_draw(hud);
    timer_hud_bind_renderer(g_renderer);
    ts_render();

    VkResult end = vk_renderer_end_frame((VkRenderer *)g_renderer, g_frame_cmd);
    g_frame_cmd = VK_NULL_HANDLE;
    g_frame_active = false;
    if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
        vk_renderer_recreate_swapchain((VkRenderer *)g_renderer, g_window);
        vk_renderer_set_logical_size((VkRenderer *)g_renderer,
                                     (float)g_window_w,
                                     (float)g_window_h);
    } else if (end != VK_SUCCESS) {
        fprintf(stderr, "[renderer] vk_renderer_end_frame failed: %d\n", end);
    }
}

bool renderer_sdl_capture_pixels(uint8_t **out_rgba, int *out_pitch) {
    if (!out_rgba || !g_scene_pixels) return false;
    if (g_window_w <= 0 || g_window_h <= 0 || g_grid_w <= 0 || g_grid_h <= 0) {
        return false;
    }
    int pitch = g_window_w * 4;
    size_t size = (size_t)pitch * (size_t)g_window_h;
    uint8_t *buffer = (uint8_t *)malloc(size);
    if (!buffer) return false;

    for (int y = 0; y < g_window_h; ++y) {
        int src_y = (int)((long long)y * g_grid_h / g_window_h);
        const uint8_t *src_row = g_scene_pixels + (size_t)src_y * (size_t)g_scene_pitch;
        uint8_t *dst_row = buffer + (size_t)y * (size_t)pitch;
        for (int x = 0; x < g_window_w; ++x) {
            int src_x = (int)((long long)x * g_grid_w / g_window_w);
            const uint8_t *src = src_row + (size_t)src_x * 4u;
            uint8_t *dst = dst_row + (size_t)x * 4u;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        }
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
