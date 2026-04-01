#include "app/space_mode_adapter.h"
#include "app/sim_mode.h"

static SpaceMode clamp_space_mode(SpaceMode mode) {
    if (mode < SPACE_MODE_2D || mode >= SPACE_MODE_COUNT) {
        return SPACE_MODE_2D;
    }
    return mode;
}

static float clamp_unit(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

SpaceMode space_mode_adapter_resolve(SpaceMode mode) {
    return clamp_space_mode(mode);
}

SpaceModeViewContext space_mode_adapter_build_canvas_view_context(SpaceMode mode,
                                                                  int canvas_x,
                                                                  int canvas_y,
                                                                  int canvas_w,
                                                                  int canvas_h) {
    return space_mode_adapter_build_canvas_view_context_ex(mode,
                                                           SPACE_MODE_2D,
                                                           canvas_x,
                                                           canvas_y,
                                                           canvas_w,
                                                           canvas_h);
}

SpaceModeViewContext space_mode_adapter_build_canvas_view_context_ex(SpaceMode requested_mode,
                                                                     SpaceMode projection_mode,
                                                                     int canvas_x,
                                                                     int canvas_y,
                                                                     int canvas_w,
                                                                     int canvas_h) {
    SpaceModeViewContext ctx;
    ctx.requested_mode = clamp_space_mode(requested_mode);
    ctx.projection_mode = clamp_space_mode(projection_mode);
    ctx.canvas_x = canvas_x;
    ctx.canvas_y = canvas_y;
    ctx.canvas_w = canvas_w;
    ctx.canvas_h = canvas_h;
    return ctx;
}

SpaceModeViewContext space_mode_adapter_build_canvas_view_context_for_route(const SimModeRoute *route,
                                                                            int canvas_x,
                                                                            int canvas_y,
                                                                            int canvas_w,
                                                                            int canvas_h) {
    if (!route) {
        return space_mode_adapter_build_canvas_view_context_ex(SPACE_MODE_2D,
                                                               SPACE_MODE_2D,
                                                               canvas_x,
                                                               canvas_y,
                                                               canvas_w,
                                                               canvas_h);
    }
    return space_mode_adapter_build_canvas_view_context_ex(route->requested_space_mode,
                                                           route->projection_space_mode,
                                                           canvas_x,
                                                           canvas_y,
                                                           canvas_w,
                                                           canvas_h);
}

bool space_mode_adapter_is_3d_requested(const SpaceModeViewContext *ctx) {
    if (!ctx) return false;
    return clamp_space_mode(ctx->requested_mode) == SPACE_MODE_3D;
}

void space_mode_adapter_world_to_screen(const SpaceModeViewContext *ctx,
                                        float world_x,
                                        float world_y,
                                        int *out_x,
                                        int *out_y) {
    if (!out_x || !out_y) return;
    if (!ctx || ctx->canvas_w <= 0 || ctx->canvas_h <= 0) {
        *out_x = (int)world_x;
        *out_y = (int)world_y;
        return;
    }

    // PS-U2 seam: projection path is centralized here. 3D requests intentionally
    // use controlled 2D projection until dimensional scene contracts land.
    *out_x = ctx->canvas_x + (int)(world_x * (float)ctx->canvas_w + 0.5f);
    *out_y = ctx->canvas_y + (int)(world_y * (float)ctx->canvas_h + 0.5f);
}

void space_mode_adapter_screen_to_world_clamped(const SpaceModeViewContext *ctx,
                                                int screen_x,
                                                int screen_y,
                                                float *out_x,
                                                float *out_y) {
    float nx = 0.0f;
    float ny = 0.0f;
    if (!out_x || !out_y) return;
    if (!ctx || ctx->canvas_w <= 0 || ctx->canvas_h <= 0) {
        *out_x = 0.0f;
        *out_y = 0.0f;
        return;
    }

    nx = (float)(screen_x - ctx->canvas_x) / (float)ctx->canvas_w;
    ny = (float)(screen_y - ctx->canvas_y) / (float)ctx->canvas_h;
    *out_x = clamp_unit(nx);
    *out_y = clamp_unit(ny);
}

void space_mode_adapter_screen_to_import_world_clamped(const SpaceModeViewContext *ctx,
                                                       int screen_x,
                                                       int screen_y,
                                                       float *out_x,
                                                       float *out_y) {
    float min_dim = 0.0f;
    float cx_px = 0.0f;
    float cy_px = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    float span_x = 0.0f;
    float span_y = 0.0f;
    float nx = 0.5f;
    float ny = 0.5f;
    float min_x = 0.0f;
    float max_x = 1.0f;
    float min_y = 0.0f;
    float max_y = 1.0f;
    if (!out_x || !out_y) return;
    if (!ctx || ctx->canvas_w <= 0 || ctx->canvas_h <= 0) {
        *out_x = 0.5f;
        *out_y = 0.5f;
        return;
    }

    min_dim = (float)((ctx->canvas_w < ctx->canvas_h) ? ctx->canvas_w : ctx->canvas_h);
    if (min_dim <= 0.0f) {
        *out_x = 0.5f;
        *out_y = 0.5f;
        return;
    }
    cx_px = (float)ctx->canvas_x + 0.5f * (float)ctx->canvas_w;
    cy_px = (float)ctx->canvas_y + 0.5f * (float)ctx->canvas_h;
    dx = ((float)screen_x - cx_px) / min_dim;
    dy = ((float)screen_y - cy_px) / min_dim;
    span_x = 0.5f * ((float)ctx->canvas_w / min_dim);
    span_y = 0.5f * ((float)ctx->canvas_h / min_dim);

    nx = 0.5f + dx;
    ny = 0.5f + dy;
    min_x = 0.5f - span_x;
    max_x = 0.5f + span_x;
    min_y = 0.5f - span_y;
    max_y = 0.5f + span_y;
    if (nx < min_x) nx = min_x;
    if (nx > max_x) nx = max_x;
    if (ny < min_y) ny = min_y;
    if (ny > max_y) ny = max_y;
    *out_x = nx;
    *out_y = ny;
}
