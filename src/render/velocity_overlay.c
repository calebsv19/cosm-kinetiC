#include "render/velocity_overlay.h"

#include <math.h>

#define DEFAULT_SAMPLE_STRIDE 2
#define DEFAULT_VECTOR_SCALE  0.4f
#define DEFAULT_SPEED_THRESHOLD 0.02f
#define DEFAULT_FIXED_FRACTION 0.66f

static float safe_scale(int window_dim, int grid_dim) {
    if (grid_dim <= 0) return 1.0f;
    if (window_dim <= 0) return 1.0f;
    return (float)window_dim / (float)grid_dim;
}

void velocity_overlay_draw(const SceneState *scene,
                           SDL_Renderer *renderer,
                           int window_w,
                           int window_h,
                           const VelocityOverlayConfig *cfg) {
    if (!scene || !scene->smoke || !renderer) return;

    const Fluid2D *grid = scene->smoke;
    int stride = (cfg && cfg->sample_stride > 0) ? cfg->sample_stride
                                                 : DEFAULT_SAMPLE_STRIDE;
    float vector_scale = (cfg && cfg->vector_scale > 0.0f)
                             ? cfg->vector_scale
                             : DEFAULT_VECTOR_SCALE;
    float speed_threshold = (cfg && cfg->speed_threshold > 0.0f)
                                ? cfg->speed_threshold
                                : DEFAULT_SPEED_THRESHOLD;
    float ref_speed = (scene && scene->config && scene->config->tunnel_inflow_speed > 0.0f)
                          ? scene->config->tunnel_inflow_speed
                          : 1.0f;
    if (ref_speed < 0.0001f) ref_speed = 0.0001f;

    float scale_x = safe_scale(window_w, grid->w);
    float scale_y = safe_scale(window_h, grid->h);

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif

    for (int gy = stride / 2; gy < grid->h; gy += stride) {
        for (int gx = stride / 2; gx < grid->w; gx += stride) {
            size_t idx = (size_t)gy * (size_t)grid->w + (size_t)gx;
            float vx = grid->velX[idx];
            float vy = grid->velY[idx];
            float speed = sqrtf(vx * vx + vy * vy);
            if (speed < speed_threshold) {
                continue;
            }

            float start_x = ((float)gx + 0.5f) * scale_x;
            float start_y = ((float)gy + 0.5f) * scale_y;

            float dir_x = vx;
            float dir_y = vy;
            float dir_len = sqrtf(dir_x * dir_x + dir_y * dir_y);
            if (dir_len < 1e-5f) continue;
            dir_x /= dir_len;
            dir_y /= dir_len;

            bool fixed = cfg && cfg->fixed_length;
            float fraction = (cfg && cfg->fixed_fraction > 0.0f)
                                 ? cfg->fixed_fraction
                                 : DEFAULT_FIXED_FRACTION;
            float line_len;
            if (fixed) {
                float gap = (float)stride;
                line_len = gap * fraction;
            } else {
                line_len = dir_len * vector_scale;
            }

            float end_x = start_x + dir_x * line_len * scale_x;
            float end_y = start_y + dir_y * line_len * scale_y;

            float falloff = 1.0f;
            if (scene && scene->obstacle_distance) {
                size_t d_id = (size_t)gy * (size_t)grid->w + (size_t)gx;
                falloff = scene->obstacle_distance[d_id];
            }
            float fade = 0.35f + 0.65f * falloff;
            if (fade <= 0.05f) continue;

            if (!fixed) {
                float norm = fminf(speed / (ref_speed * 1.2f), 1.0f);
                Uint8 r = (Uint8)lroundf(30.0f + 40.0f * norm);
                Uint8 g = (Uint8)lroundf(120.0f + 135.0f * norm);
                Uint8 b = (Uint8)lroundf(30.0f + 50.0f * norm);
                Uint8 a = (Uint8)lroundf((70.0f + 180.0f * norm) * fade);
                SDL_SetRenderDrawColor(renderer, r, g, b, a);
            } else {
                float norm = fminf(speed / (ref_speed * 1.2f), 1.0f);
                Uint8 r = (Uint8)lroundf(30.0f + 40.0f * norm);
                Uint8 g = (Uint8)lroundf(120.0f + 135.0f * norm);
                Uint8 b = (Uint8)lroundf(30.0f + 50.0f * norm);
                Uint8 a = (Uint8)lroundf((90.0f + 170.0f * norm) * fade);
                SDL_SetRenderDrawColor(renderer, r, g, b, a);
            }

            SDL_RenderDrawLine(renderer,
                               (int)lroundf(start_x),
                               (int)lroundf(start_y),
                               (int)lroundf(end_x),
                               (int)lroundf(end_y));
        }
    }
}
