#include "render/velocity_overlay.h"
#include "render/kit_viz_field_adapter.h"

#include <math.h>
#include <stdlib.h>

#define DEFAULT_SAMPLE_STRIDE 2
#define DEFAULT_VECTOR_SCALE  0.4f
#define DEFAULT_SPEED_THRESHOLD 0.02f
#define DEFAULT_FIXED_FRACTION 0.66f

static PhysicsKitVizVectorSegment *g_vec_segments = NULL;
static size_t g_vec_segment_capacity = 0;

static float safe_scale(int window_dim, int grid_dim) {
    if (grid_dim <= 0) return 1.0f;
    if (window_dim <= 0) return 1.0f;
    return (float)window_dim / (float)grid_dim;
}

static bool ensure_vector_buffer(size_t needed) {
    if (needed == 0) return false;
    if (needed <= g_vec_segment_capacity) return true;
    PhysicsKitVizVectorSegment *resized = (PhysicsKitVizVectorSegment *)realloc(
        g_vec_segments, needed * sizeof(PhysicsKitVizVectorSegment));
    if (!resized) return false;
    g_vec_segments = resized;
    g_vec_segment_capacity = needed;
    return true;
}

static void velocity_overlay_draw_legacy(const SceneState *scene,
                                         SDL_Renderer *renderer,
                                         int window_w,
                                         int window_h,
                                         const VelocityOverlayConfig *cfg) {
    SceneFluidFieldView2D fluid = {0};
    SceneObstacleFieldView2D obstacles = {0};
    if (!scene || !renderer) return;
    if (!scene_backend_fluid_view_2d(scene, &fluid)) return;
    (void)scene_backend_obstacle_view_2d(scene, &obstacles);

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

    float scale_x = safe_scale(window_w, fluid.width);
    float scale_y = safe_scale(window_h, fluid.height);

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif

    for (int gy = stride / 2; gy < fluid.height; gy += stride) {
        for (int gx = stride / 2; gx < fluid.width; gx += stride) {
            size_t idx = (size_t)gy * (size_t)fluid.width + (size_t)gx;
            float vx = fluid.velocity_x[idx];
            float vy = fluid.velocity_y[idx];
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
            if (obstacles.distance) {
                size_t d_id = (size_t)gy * (size_t)fluid.width + (size_t)gx;
                falloff = obstacles.distance[d_id];
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

static bool velocity_overlay_draw_kit_viz(const SceneState *scene,
                                          SDL_Renderer *renderer,
                                          int window_w,
                                          int window_h,
                                          const VelocityOverlayConfig *cfg) {
    SceneFluidFieldView2D fluid = {0};
    SceneObstacleFieldView2D obstacles = {0};
    if (!scene || !renderer) return false;
    if (!scene_backend_fluid_view_2d(scene, &fluid)) return false;
    if (!fluid.velocity_x || !fluid.velocity_y || fluid.width <= 0 || fluid.height <= 0) return false;
    (void)scene_backend_obstacle_view_2d(scene, &obstacles);

    int stride = (cfg && cfg->sample_stride > 0) ? cfg->sample_stride
                                                 : DEFAULT_SAMPLE_STRIDE;
    float vector_scale = (cfg && cfg->vector_scale > 0.0f)
                             ? cfg->vector_scale
                             : DEFAULT_VECTOR_SCALE;
    float speed_threshold = (cfg && cfg->speed_threshold > 0.0f)
                                ? cfg->speed_threshold
                                : DEFAULT_SPEED_THRESHOLD;
    float fraction = (cfg && cfg->fixed_fraction > 0.0f)
                         ? cfg->fixed_fraction
                         : DEFAULT_FIXED_FRACTION;
    bool fixed = cfg && cfg->fixed_length;
    float build_scale = fixed ? 1.0f : vector_scale;

    size_t sample_w = (size_t)((fluid.width + stride - 1) / stride);
    size_t sample_h = (size_t)((fluid.height + stride - 1) / stride);
    size_t max_segments = sample_w * sample_h;
    if (max_segments == 0) return false;

    if (!ensure_vector_buffer(max_segments)) {
        return false;
    }

    size_t segment_count = 0;
    PhysicsKitVizVectorRequest request = {
        .vx = fluid.velocity_x,
        .vy = fluid.velocity_y,
        .width = (uint32_t)fluid.width,
        .height = (uint32_t)fluid.height,
        .stride = (uint32_t)stride,
        .scale = build_scale,
        .out_segments = g_vec_segments,
        .max_segments = max_segments,
        .out_segment_count = &segment_count
    };
    if (!physics_kit_viz_build_vectors(&request)) {
        return false;
    }

    float ref_speed = (scene->config && scene->config->tunnel_inflow_speed > 0.0f)
                          ? scene->config->tunnel_inflow_speed
                          : 1.0f;
    if (ref_speed < 0.0001f) ref_speed = 0.0001f;
    float scale_x = safe_scale(window_w, fluid.width);
    float scale_y = safe_scale(window_h, fluid.height);

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif

    for (size_t i = 0; i < segment_count; ++i) {
        const PhysicsKitVizVectorSegment *seg = &g_vec_segments[i];
        float dx = seg->x1 - seg->x0;
        float dy = seg->y1 - seg->y0;
        float speed = fixed ? sqrtf(dx * dx + dy * dy)
                            : sqrtf(dx * dx + dy * dy) / build_scale;
        if (speed < speed_threshold) {
            continue;
        }

        float start_x = seg->x0 * scale_x;
        float start_y = seg->y0 * scale_y;
        float end_x = seg->x1 * scale_x;
        float end_y = seg->y1 * scale_y;

        if (fixed) {
            float dir_len = sqrtf(dx * dx + dy * dy);
            if (dir_len < 1e-5f) continue;
            float dir_x = dx / dir_len;
            float dir_y = dy / dir_len;
            float line_len = (float)stride * fraction;
            end_x = start_x + dir_x * line_len * scale_x;
            end_y = start_y + dir_y * line_len * scale_y;
        }

        int gx = (int)floorf(seg->x0);
        int gy = (int)floorf(seg->y0);
        if (gx < 0 || gx >= fluid.width || gy < 0 || gy >= fluid.height) {
            continue;
        }
        float falloff = 1.0f;
        if (obstacles.distance) {
            size_t d_id = (size_t)gy * (size_t)fluid.width + (size_t)gx;
            falloff = obstacles.distance[d_id];
        }
        float fade = 0.35f + 0.65f * falloff;
        if (fade <= 0.05f) continue;

        float norm = fminf(speed / (ref_speed * 1.2f), 1.0f);
        Uint8 r = (Uint8)lroundf(30.0f + 40.0f * norm);
        Uint8 g = (Uint8)lroundf(120.0f + 135.0f * norm);
        Uint8 b = (Uint8)lroundf(30.0f + 50.0f * norm);
        Uint8 a = (Uint8)lroundf(((fixed ? 90.0f : 70.0f) + (fixed ? 170.0f : 180.0f) * norm) * fade);
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
        SDL_RenderDrawLine(renderer,
                           (int)lroundf(start_x),
                           (int)lroundf(start_y),
                           (int)lroundf(end_x),
                           (int)lroundf(end_y));
    }

    return true;
}

void velocity_overlay_draw(const SceneState *scene,
                           SDL_Renderer *renderer,
                           int window_w,
                           int window_h,
                           const VelocityOverlayConfig *cfg) {
    velocity_overlay_draw_legacy(scene, renderer, window_w, window_h, cfg);
}

VelocityOverlayRenderSource velocity_overlay_draw_adapter_first(const SceneState *scene,
                                                                SDL_Renderer *renderer,
                                                                int window_w,
                                                                int window_h,
                                                                const VelocityOverlayConfig *cfg,
                                                                bool prefer_kit_viz) {
    if (prefer_kit_viz && velocity_overlay_draw_kit_viz(scene, renderer, window_w, window_h, cfg)) {
        return VELOCITY_OVERLAY_SOURCE_KIT_VIZ;
    }
    velocity_overlay_draw_legacy(scene, renderer, window_w, window_h, cfg);
    return VELOCITY_OVERLAY_SOURCE_LEGACY;
}
