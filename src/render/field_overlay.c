#include "render/field_overlay.h"

#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "render/render_common.h"

typedef struct PressureVizProfile {
    float clamp_pos;   // maximum positive pressure delta visualized
    float clamp_neg;   // maximum negative pressure delta visualized
    float gamma;       // curve applied to normalized magnitude
    float alpha_scale; // scales final alpha
} PressureVizProfile;

static const PressureVizProfile PRESSURE_PROFILE_TUNNEL = {
    .clamp_pos = 6.0f,
    .clamp_neg = 6.0f,
    .gamma = 1.4f,
    .alpha_scale = 1.0f
};

static const PressureVizProfile PRESSURE_PROFILE_BOX = {
    .clamp_pos = 2.5f,
    .clamp_neg = 2.5f,
    .gamma = 1.2f,
    .alpha_scale = 0.8f
};

static float *g_vorticity_tmp   = NULL;
static float *g_vorticity_blur  = NULL;
static size_t g_vorticity_cap   = 0;
static float *g_pressure_tmp    = NULL;
static float *g_pressure_blur   = NULL;
static size_t g_pressure_cap    = 0;

static inline uint8_t clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

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

static inline void blend_pixel(uint8_t *dst,
                               uint8_t r,
                               uint8_t g,
                               uint8_t b,
                               uint8_t alpha) {
    if (!dst || alpha == 0) return;
    uint8_t base_r = dst[0];
    uint8_t base_g = dst[1];
    uint8_t base_b = dst[2];
    dst[0] = (uint8_t)((r * alpha + base_r * (255 - alpha)) / 255);
    dst[1] = (uint8_t)((g * alpha + base_g * (255 - alpha)) / 255);
    dst[2] = (uint8_t)((b * alpha + base_b * (255 - alpha)) / 255);
    dst[3] = 255;
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
                                    uint8_t *pixels,
                                    int pitch) {
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
        uint8_t *row = pixels + y * pitch;
        for (int x = 0; x < w; ++x) {
            size_t id = (size_t)y * (size_t)w + (size_t)x;
            float v = g_vorticity_tmp[id] / max_vort;
            float magnitude = fabsf(v);
            if (magnitude <= 0.05f) continue;

            float normalized = (magnitude - 0.05f) / (1.0f - 0.05f);
            if (normalized < 0.0f) normalized = 0.0f;
            if (normalized > 1.0f) normalized = 1.0f;
            float intensity = powf(normalized, 1.4f);

            uint8_t r = 0, g = 0, b = 0;
            if (v > 0.0f) {
                r = (uint8_t)lroundf(lerp(150.0f, 255.0f, intensity));
                g = (uint8_t)lroundf(lerp(50.0f, 130.0f, intensity));
                b = (uint8_t)lroundf(lerp(15.0f, 60.0f, 1.0f - intensity));
            } else if (v < 0.0f) {
                b = (uint8_t)lroundf(lerp(150.0f, 255.0f, intensity));
                r = (uint8_t)lroundf(lerp(35.0f, 105.0f, intensity));
                g = (uint8_t)lroundf(lerp(25.0f, 85.0f, intensity));
            }

            uint8_t alpha = (uint8_t)lroundf(30.0f + 110.0f * intensity);
            if (alpha == 0) continue;
            blend_pixel(&row[x * 4], r, g, b, alpha);
        }
    }
}

static void apply_pressure_overlay(const SceneState *scene,
                                   uint8_t *pixels,
                                   int pitch) {
    if (!scene || !scene->smoke || !pixels) return;
    const Fluid2D *grid = scene->smoke;
    int w = grid->w;
    int h = grid->h;
    if (w < 3 || h < 3 || !grid->pressure) return;

    size_t cell_count = (size_t)w * (size_t)h;
    if (!ensure_pressure_buffers(cell_count)) return;

    SimulationMode mode = (scene->config) ? scene->config->sim_mode : SIM_MODE_BOX;
    const PressureVizProfile *profile = (mode == SIM_MODE_WIND_TUNNEL)
                                            ? &PRESSURE_PROFILE_TUNNEL
                                            : &PRESSURE_PROFILE_BOX;

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
        // Clamp based on profile to keep colors tame in box mode.
        if (p_prime > profile->clamp_pos) p_prime = profile->clamp_pos;
        if (p_prime < -profile->clamp_neg) p_prime = -profile->clamp_neg;
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
    float pos_scale = fmaxf(max_pos, 1e-3f);
    float neg_scale = fmaxf(max_neg, 1e-3f);

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

    const AppConfig *cfg = scene->config;
    uint8_t base_black = cfg ? clamp_u8(cfg->render_black_level) : 0;
    uint8_t base_gray = (uint8_t)lroundf((float)base_black + (200.0f - (float)base_black) * 0.6f);
    for (int y = 0; y < h; ++y) {
        uint8_t *row = pixels + y * pitch;
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
            magnitude = powf(magnitude, profile->gamma);

            uint8_t overlay_r = base_gray;
            uint8_t overlay_g = base_gray;
            uint8_t overlay_b = base_gray;
            if (positive) {
                overlay_r = (uint8_t)lroundf(base_gray + (255.0f - base_gray) * magnitude);
                overlay_g = (uint8_t)lroundf(base_gray * (1.0f - magnitude));
                overlay_b = overlay_g;
            } else {
                overlay_b = (uint8_t)lroundf(base_gray + (255.0f - base_gray) * magnitude);
                overlay_r = (uint8_t)lroundf(base_gray * (1.0f - magnitude));
                overlay_g = overlay_r;
            }
            uint8_t alpha = (uint8_t)lroundf(180.0f * magnitude * alpha_factor * profile->alpha_scale);
            if (alpha == 0) continue;
            blend_pixel(&row[x * 4], overlay_r, overlay_g, overlay_b, alpha);
        }
    }
}

void field_overlay_apply(const SceneState *scene,
                         uint8_t *pixels,
                         int pitch,
                         const FieldOverlayConfig *cfg) {
    if (!scene || !scene->smoke || !pixels || !cfg) return;
    if (!cfg->draw_pressure && !cfg->draw_vorticity) return;

    if (cfg->draw_pressure) {
        apply_pressure_overlay(scene, pixels, pitch);
    }
    if (cfg->draw_vorticity) {
        apply_vorticity_overlay(scene, pixels, pitch);
    }
}
