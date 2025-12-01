#include "physics/objects/physics_object_builder.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
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
    if (!asset || !base || !out || grid_w <= 0 || grid_h <= 0) return false;
    memset(out, 0, sizeof(*out));

    size_t mask_count = (size_t)grid_w * (size_t)grid_h;
    uint8_t *mask = (uint8_t *)calloc(mask_count, sizeof(uint8_t));
    if (!mask) return false;

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

    if (!shape_asset_rasterize(asset, grid_w, grid_h, &opts, mask)) {
        free(mask);
        return false;
    }

    out->base = *base;
    out->mask = mask;
    out->mask_w = grid_w;
    out->mask_h = grid_h;
    out->density = 1.0f;
    out->friction = 0.2f;
    out->is_static = true;
    return true;
}
