#include "render/field_overlay.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "render/render_common.h"

static float *g_vorticity_tmp   = NULL;
static float *g_vorticity_blur  = NULL;
static size_t g_vorticity_cap   = 0;
static float *g_pressure_tmp    = NULL;
static float *g_pressure_blur   = NULL;
static size_t g_pressure_cap    = 0;

static inline float solid_alpha_falloff(const SceneState *scene, size_t id) {
    if (!scene || !scene->obstacle_distance) return 1.0f;
    return scene->obstacle_distance[id];
}

static bool ensure_vorticity_buffers(size_t cell_count) {
    if (cell_count == 0) return false;
    if (cell_count <= g_vorticity_cap) return true;
    float *tmp = (float *)realloc(g_vorticity_tmp, cell_count * sizeof(float));
    if (!tmp) return false;
    g_vorticity_tmp = tmp;
    float *blur = (float *)realloc(g_vorticity_blur, cell_count * sizeof(float));
    if (!blur) return false;
    g_vorticity_blur = blur;
    g_vorticity_cap = cell_count;
    return true;
}

static bool ensure_pressure_buffers(size_t cell_count) {
    if (cell_count == 0) return false;
    if (cell_count <= g_pressure_cap) return true;
    float *tmp = (float *)realloc(g_pressure_tmp, cell_count * sizeof(float));
    if (!tmp) return false;
    g_pressure_tmp = tmp;
    float *blur = (float *)realloc(g_pressure_blur, cell_count * sizeof(float));
    if (!blur) return false;
    g_pressure_blur = blur;
    g_pressure_cap = cell_count;
    return true;
}

static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline void blend_pixel(Uint32 *dst,
                               SDL_PixelFormat *format,
                               Uint8 r,
                               Uint8 g,
                               Uint8 b,
                               Uint8 alpha) {
    if (!dst || !format || alpha == 0) return;
    Uint8 base_r = 0, base_g = 0, base_b = 0, base_a = 0;
    SDL_GetRGBA(*dst, format, &base_r, &base_g, &base_b, &base_a);
    base_r = (Uint8)((r * alpha + base_r * (255 - alpha)) / 255);
    base_g = (Uint8)((g * alpha + base_g * (255 - alpha)) / 255);
    base_b = (Uint8)((b * alpha + base_b * (255 - alpha)) / 255);
    *dst = SDL_MapRGBA(format, base_r, base_g, base_b, base_a ? base_a : 255);
}

bool field_overlay_init(void) {
    // Nothing heavy yet; buffers are grown lazily.
    return true;
}

void field_overlay_shutdown(void) {
    free(g_vorticity_tmp);
    g_vorticity_tmp = NULL;
    free(g_vorticity_blur);
    g_vorticity_blur = NULL;
    g_vorticity_cap = 0;

    free(g_pressure_tmp);
    g_pressure_tmp = NULL;
    free(g_pressure_blur);
    g_pressure_blur = NULL;
    g_pressure_cap = 0;
}

static void apply_vorticity_overlay(const SceneState *scene,
                                    Uint8 *pixels,
                                    int pitch,
                                    SDL_PixelFormat *format) {
    if (!scene || !scene->smoke || !pixels) return;
    const Fluid2D *grid = scene->smoke;
    int w = grid->w;
    int h = grid->h;
    if (w < 3 || h < 3) return;

    size_t cell_count = (size_t)w * (size_t)h;
    if (!ensure_vorticity_buffers(cell_count)) return;

    // 1) Compute raw vorticity (centered differences) in interior
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            size_t id = (size_t)y * (size_t)w + (size_t)x;

            float dvy_dx = (grid->velY[id + 1]     - grid->velY[id - 1]) * 0.5f;
            float dvx_dy = (grid->velX[id + w]     - grid->velX[id - w]) * 0.5f;
            float vort   = dvy_dx - dvx_dy;

            g_vorticity_tmp[id] = vort;
        }
    }

    // Zero borders (we don't have valid centered differences there)
    for (int x = 0; x < w; ++x) {
        g_vorticity_tmp[x] = 0.0f;
        g_vorticity_tmp[(size_t)(h - 1) * (size_t)w + (size_t)x] = 0.0f;
    }
    for (int y = 0; y < h; ++y) {
        g_vorticity_tmp[(size_t)y * (size_t)w]               = 0.0f;
        g_vorticity_tmp[(size_t)y * (size_t)w + (size_t)(w - 1)] = 0.0f;
    }

    // 2) Optional blur to make vort filaments softer
    bool blur_enabled = (scene->config && scene->config->enable_render_blur);
#if RENDERER_ENABLE_SMOOTHING
    if (blur_enabled) {
        // Horizontal blur
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sx = x + k;
                    if (sx < 0) sx = 0;
                    if (sx >= w) sx = w - 1;
                    accum += g_vorticity_tmp[(size_t)y * (size_t)w + (size_t)sx] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_vorticity_blur[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
        // Vertical blur
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sy = y + k;
                    if (sy < 0) sy = 0;
                    if (sy >= h) sy = h - 1;
                    accum += g_vorticity_blur[(size_t)sy * (size_t)w + (size_t)x] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_vorticity_tmp[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
    }
#else
    (void)blur_enabled;
#endif

    // 3) Find max magnitude for normalization
    float max_vort = 0.0f;
    for (size_t i = 0; i < cell_count; ++i) {
        float mag = fabsf(g_vorticity_tmp[i]);
        if (mag > max_vort) max_vort = mag;
    }
    if (max_vort <= FLT_EPSILON) {
        return;
    }

    // 4) Overlay as color map (red for positive, blue for negative)
    for (int y = 0; y < h; ++y) {
        Uint32 *row = (Uint32 *)(pixels + y * pitch);
        for (int x = 0; x < w; ++x) {
            size_t id = (size_t)y * (size_t)w + (size_t)x;
            float v = g_vorticity_tmp[id] / max_vort;
            float magnitude = fabsf(v);
            if (magnitude <= 0.05f) continue;

            float normalized = (magnitude - 0.05f) / (1.0f - 0.05f);
            if (normalized < 0.0f) normalized = 0.0f;
            if (normalized > 1.0f) normalized = 1.0f;
            float intensity = powf(normalized, 1.4f);

            Uint8 r = 0, g = 0, b = 0;
            if (v > 0.0f) {
                r = (Uint8)lroundf(lerp(150.0f, 255.0f, intensity));
                g = (Uint8)lroundf(lerp(50.0f, 130.0f, intensity));
                b = (Uint8)lroundf(lerp(15.0f, 60.0f, 1.0f - intensity));
            } else if (v < 0.0f) {
                b = (Uint8)lroundf(lerp(150.0f, 255.0f, intensity));
                r = (Uint8)lroundf(lerp(35.0f, 105.0f, intensity));
                g = (Uint8)lroundf(lerp(25.0f, 85.0f, intensity));
            }

            Uint8 alpha = (Uint8)lroundf(30.0f + 110.0f * intensity);
            if (alpha == 0) continue;
            blend_pixel(&row[x], format, r, g, b, alpha);
        }
    }
}

static void apply_pressure_overlay(const SceneState *scene,
                                   Uint8 *pixels,
                                   int pitch,
                                   SDL_PixelFormat *format) {
    if (!scene || !scene->smoke || !pixels) return;
    const Fluid2D *grid = scene->smoke;
    int w = grid->w;
    int h = grid->h;
    if (w < 3 || h < 3 || !grid->pressure) return;

    size_t cell_count = (size_t)w * (size_t)h;
    if (!ensure_pressure_buffers(cell_count)) return;

    float ref_sum = 0.0f;
    int ref_count = 0;
    int ref_band = (int)fmaxf(1.0f, (float)w * 0.05f);
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < ref_band; ++x) {
            float p = grid->pressure[(size_t)y * (size_t)w + (size_t)x];
            if (!isfinite(p)) continue;
            ref_sum += p;
            ++ref_count;
        }
    }
    float p_ref = (ref_count > 0) ? ref_sum / (float)ref_count : 0.0f;

    float max_pos = 0.0f;
    float max_neg = 0.0f;
    for (size_t i = 0; i < cell_count; ++i) {
        float p = grid->pressure[i];
        if (!isfinite(p)) {
            g_pressure_tmp[i] = 0.0f;
            continue;
        }
        float p_prime = p - p_ref;
        g_pressure_tmp[i] = p_prime;
        if (p_prime > 0.0f) {
            if (p_prime > max_pos) max_pos = p_prime;
        } else if (p_prime < 0.0f) {
            float mag = -p_prime;
            if (mag > max_neg) max_neg = mag;
        }
    }
    if (max_pos <= FLT_EPSILON) max_pos = 1.0f;
    if (max_neg <= FLT_EPSILON) max_neg = 1.0f;
    const float intensity_boost = 0.7f; // smaller scale => stronger color
    float pos_scale = fmaxf(max_pos * intensity_boost, 1e-3f);
    float neg_scale = fmaxf(max_neg * intensity_boost, 1e-3f);

    bool blur_enabled = (scene->config && scene->config->enable_render_blur);
#if RENDERER_ENABLE_SMOOTHING
    if (blur_enabled) {
        // Horizontal blur
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sx = x + k;
                    if (sx < 0)    sx = 0;
                    if (sx >= w)   sx = w - 1;
                    accum += g_pressure_tmp[(size_t)y * (size_t)w + (size_t)sx] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_pressure_blur[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
        // Vertical blur
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float accum = 0.0f;
                for (int k = -1; k <= 1; ++k) {
                    int sy = y + k;
                    if (sy < 0)    sy = 0;
                    if (sy >= h)   sy = h - 1;
                    accum += g_pressure_blur[(size_t)sy * (size_t)w + (size_t)x] *
                             RENDERER_SMOOTH_KERNEL_1D[k + 1];
                }
                g_pressure_tmp[(size_t)y * (size_t)w + (size_t)x] =
                    accum / RENDERER_SMOOTH_KERNEL_1D_SUM;
            }
        }
    }
#else
    (void)blur_enabled;
#endif

    const Uint8 base_gray = 200;
    for (int y = 0; y < h; ++y) {
        Uint32 *row = (Uint32 *)(pixels + y * pitch);
        for (int x = 0; x < w; ++x) {
            size_t id = (size_t)y * (size_t)w + (size_t)x;
            float value = g_pressure_tmp[id];
            if (!isfinite(value)) continue;

            float norm = 0.0f;
            bool positive = value >= 0.0f;
            if (positive) {
                norm = value / pos_scale;
            } else {
                norm = value / neg_scale;
            }
            if (norm > 1.0f) norm = 1.0f;
            if (norm < -1.0f) norm = -1.0f;

            float magnitude = fabsf(norm);
            if (magnitude < 0.01f) continue;

            float falloff = solid_alpha_falloff(scene, id);
            float alpha_factor = 0.25f + 0.75f * falloff;
            magnitude = powf(magnitude, 0.9f);

            Uint8 overlay_r = base_gray;
            Uint8 overlay_g = base_gray;
            Uint8 overlay_b = base_gray;
            if (positive) {
                overlay_r = (Uint8)lroundf(base_gray + (255.0f - base_gray) * magnitude);
                overlay_g = (Uint8)lroundf(base_gray * (1.0f - magnitude));
                overlay_b = overlay_g;
            } else {
                overlay_b = (Uint8)lroundf(base_gray + (255.0f - base_gray) * magnitude);
                overlay_r = (Uint8)lroundf(base_gray * (1.0f - magnitude));
                overlay_g = overlay_r;
            }
            Uint8 alpha = (Uint8)lroundf(180.0f * magnitude * alpha_factor);
            if (alpha == 0) continue;
            blend_pixel(&row[x], format, overlay_r, overlay_g, overlay_b, alpha);
        }
    }
}

void field_overlay_apply(const SceneState *scene,
                         SDL_Texture *texture,
                         SDL_PixelFormat *format,
                         const FieldOverlayConfig *cfg) {
    if (!scene || !scene->smoke || !texture || !cfg) return;
    if (!cfg->draw_pressure && !cfg->draw_vorticity) return;

    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(texture, NULL, &pixels, &pitch) != 0) {
        fprintf(stderr, "[field_overlay] Failed to lock scene texture: %s\n",
                SDL_GetError());
        return;
    }

    Uint8 *base = (Uint8 *)pixels;
    if (cfg->draw_pressure) {
        apply_pressure_overlay(scene, base, pitch, format);
    }
    if (cfg->draw_vorticity) {
        apply_vorticity_overlay(scene, base, pitch, format);
    }

    SDL_UnlockTexture(texture);
}
