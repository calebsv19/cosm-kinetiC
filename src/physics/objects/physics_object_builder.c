#include "physics/objects/physics_object_builder.h"

#include "import/shape_asset_input.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}


/* Admission mirrors the shared rasterizer's float arithmetic and charges its
 * actual square visits / polygon edge tests before allocating any mask. */
static bool charge_raster_work(size_t *remaining, size_t a, size_t b, size_t c) {
    if (!a || !b || !c) return true;
    if (a > *remaining / b) return false;
    size_t ab = a * b;
    if (ab > *remaining / c) return false;
    *remaining -= ab * c;
    return true;
}

static bool raster_endpoint(ShapeAssetPoint p, float cx, float cy,
                            float cosine, float sine, float scale, float ox, float oy,
                            int *x, int *y) {
    float dx = p.x - cx, dy = p.y - cy;
    float rx = dx * cosine - dy * sine + cx;
    float ry = dx * sine + dy * cosine + cy;
    float gx = rx * scale + ox, gy = ry * scale + oy;
    const float limit = (float)(INT_MAX / 8);
    if (!isfinite(gx) || !isfinite(gy) || fabsf(gx) > limit || fabsf(gy) > limit) return false;
    *x = (int)lroundf(gx);
    *y = (int)lroundf(gy);
    return true;
}

static bool admit_raster_segment(ShapeAssetPoint a, ShapeAssetPoint b,
                                 float cx, float cy, float cosine, float sine,
                                 float scale, float ox, float oy, int half, size_t *remaining) {
    int ax, ay, bx, by;
    if (!raster_endpoint(a, cx, cy, cosine, sine, scale, ox, oy, &ax, &ay) ||
        !raster_endpoint(b, cx, cy, cosine, sine, scale, ox, oy, &bx, &by)) return false;
    float steps = ceilf(fmaxf(fabsf((float)(bx - ax)), fabsf((float)(by - ay))));
    if (steps < 1.0f) steps = 1.0f;
    size_t side = (size_t)half * 2 + 1;
    return charge_raster_work(remaining, (size_t)steps + 1, side, side);
}

static bool asset_raster_admitted(const ShapeAsset *asset, int w, int h,
                                  const ShapeAssetRasterOptions *opts) {
    if (!physics_sim_shape_asset_admitted(asset) ||
        !isfinite(opts->margin_cells) || !isfinite(opts->stroke) ||
        !isfinite(opts->max_error) || !isfinite(opts->position_x_norm) ||
        !isfinite(opts->position_y_norm) || !isfinite(opts->rotation_deg) ||
        !isfinite(opts->scale)) return false;
    const double coordinate_limit = sqrt((double)FLT_MAX) / 32.0;
    for (size_t i = 0; i < asset->path_count; ++i)
        for (size_t j = 0; j < asset->paths[i].point_count; ++j) {
            ShapeAssetPoint p = asset->paths[i].points[j];
            if (fabs((double)p.x) > coordinate_limit || fabs((double)p.y) > coordinate_limit) return false;
        }
    ShapeAssetBounds b;
    if (!shape_asset_bounds(asset, &b) || !b.valid) return false;
    float cx = 0.5f * (b.min_x + b.max_x), cy = 0.5f * (b.min_y + b.max_y);
    float scale = opts->scale, ox, oy;
    if (opts->center_fit) {
        float width = b.max_x - b.min_x, height = b.max_y - b.min_y;
        float aw = (float)(w - 1) - opts->margin_cells * 2.0f;
        float ah = (float)(h - 1) - opts->margin_cells * 2.0f;
        if (aw <= 0.0f) aw = (float)(w - 1);
        if (ah <= 0.0f) ah = (float)(h - 1);
        float sx = width > 1e-5f ? aw / width : aw;
        float sy = height > 1e-5f ? ah / height : ah;
        scale = fminf(sx, sy);
        if (scale <= 0.0f) scale = 1.0f;
        ox = 0.5f * (float)(w - 1) - cx * scale;
        oy = 0.5f * (float)(h - 1) - cy * scale;
    } else {
        ox = opts->position_x_norm * (float)(w - 1) - cx * scale;
        oy = opts->position_y_norm * (float)(h - 1) - cy * scale;
    }
    float radians = opts->rotation_deg * (float)M_PI / 180.0f;
    float cosine = cosf(radians), sine = sinf(radians);
    float half_float = ceilf(fmaxf(opts->stroke * 0.5f, 0.5f));
    if (!isfinite(scale) || scale <= 0.0f || !isfinite(ox) || !isfinite(oy) ||
        !isfinite(cosine) || !isfinite(sine) || !isfinite(half_float) ||
        half_float > (float)(INT_MAX / 8)) return false;
    int half = (int)half_float;
    size_t remaining = 64u * 1024u * 1024u;
    for (size_t i = 0; i < asset->path_count; ++i) {
        const ShapeAssetPath *path = &asset->paths[i];
        if (path->point_count < 2) continue;
        for (size_t j = 1; j < path->point_count; ++j)
            if (!admit_raster_segment(path->points[j-1], path->points[j], cx, cy,
                                      cosine, sine, scale, ox, oy, half, &remaining)) return false;
        if (path->closed) {
            if (!admit_raster_segment(path->points[path->point_count-1], path->points[0],
                                      cx, cy, cosine, sine, scale, ox, oy, half, &remaining)) return false;
            if (path->point_count >= 3) {
                if (!charge_raster_work(&remaining, (size_t)w, (size_t)h, path->point_count)) return false;
                /* Keep inverse fill coordinates / polygon products finite too. */
                float inverse = scale > 1e-6f ? 1.0f / scale : 1.0f;
                for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
                    float wx = ((float)(x ? w-1 : 0) - ox) * inverse;
                    float wy = ((float)(y ? h-1 : 0) - oy) * inverse;
                    float dx = wx - cx, dy = wy - cy;
                    float lx = dx * cosine + dy * sine + cx;
                    float ly = -dx * sine + dy * cosine + cy;
                    if (!isfinite(lx) || !isfinite(ly) || fabs((double)lx) > coordinate_limit ||
                        fabs((double)ly) > coordinate_limit) return false;
                }
            }
        }
    }
    return true;
}

void physics_object_free(PhysicsObject *obj) {
    if (!obj) return;
    free(obj->mask);
    obj->mask = NULL;
    obj->mask_w = 0;
    obj->mask_h = 0;
}

bool physics_object_from_asset(const ShapeAsset *asset,
                               const SceneObjectBase *base,
                               int grid_w,
                               int grid_h,
                               const ShapeAssetRasterOptions *extra_opts,
                               PhysicsObject *out) {
    const size_t max_cells = 64u * 1024u * 1024u;
    if (!asset || !base || !out || out->mask || grid_w <= 0 || grid_h <= 0 ||
        (size_t)grid_w > max_cells / (size_t)grid_h ||
        !isfinite(base->position.x) || !isfinite(base->position.y) ||
        !isfinite(base->rotation) || !isfinite(base->scale.x) || !isfinite(base->scale.y)) return false;
    if (extra_opts && (!isfinite(extra_opts->margin_cells) || !isfinite(extra_opts->stroke) ||
        !isfinite(extra_opts->max_error) || !isfinite(extra_opts->position_x_norm) ||
        !isfinite(extra_opts->position_y_norm) || !isfinite(extra_opts->rotation_deg) ||
        !isfinite(extra_opts->scale))) return false;

    ShapeAssetRasterOptions opts = {
        .margin_cells = 1.0f,
        .stroke = 1.0f,
        .position_x_norm = clamp01(base->position.x),
        .position_y_norm = clamp01(base->position.y),
        .rotation_deg = base->rotation * (180.0f / (float)M_PI),
        .scale = (base->scale.x > 0.0f) ? base->scale.x : 1.0f,
        .center_fit = false,
    };
    if (extra_opts) {
        // Copy only fields provided (center_fit defaults to false unless explicitly set).
        opts.margin_cells = (extra_opts->margin_cells >= 0.0f) ? extra_opts->margin_cells : opts.margin_cells;
        opts.stroke = (extra_opts->stroke > 0.0f) ? extra_opts->stroke : opts.stroke;
        if (extra_opts->position_x_norm >= 0.0f && extra_opts->position_x_norm <= 1.0f) {
            opts.position_x_norm = extra_opts->position_x_norm;
        }
        if (extra_opts->position_y_norm >= 0.0f && extra_opts->position_y_norm <= 1.0f) {
            opts.position_y_norm = extra_opts->position_y_norm;
        }
        opts.rotation_deg = extra_opts->rotation_deg;
        if (extra_opts->scale > 0.0f) opts.scale = extra_opts->scale;
        opts.center_fit = extra_opts->center_fit;
    }

    if (!asset_raster_admitted(asset, grid_w, grid_h, &opts)) return false;
    size_t mask_count = (size_t)grid_w * (size_t)grid_h;
    uint8_t *mask = (uint8_t *)calloc(mask_count, sizeof(uint8_t));
    if (!mask) return false;
    if (!shape_asset_rasterize(asset, grid_w, grid_h, &opts, mask)) {
        free(mask);
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->base = *base;
    out->mask = mask;
    out->mask_w = grid_w;
    out->mask_h = grid_h;
    out->density = 1.0f;
    out->friction = 0.2f;
    out->is_static = true;
    return true;
}
