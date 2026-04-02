#include "app/editor/scene_editor_canvas.h"
#include "app/shape_lookup.h"
#include "app/space_mode_adapter.h"

#include <math.h>

static SpaceMode s_scene_editor_canvas_space_mode = SPACE_MODE_2D;
static SpaceMode s_scene_editor_canvas_projection_mode = SPACE_MODE_2D;

static float clampf_local(float value, float min_v, float max_v) {
    if (value < min_v) return min_v;
    if (value > max_v) return max_v;
    return value;
}

static float object_depth_visual_scale(const PresetObject *obj) {
    if (!obj) return 1.0f;
    if (s_scene_editor_canvas_space_mode != SPACE_MODE_3D) return 1.0f;
    return clampf_local(1.0f - (obj->position_z * 0.06f), 0.55f, 1.75f);
}

void scene_editor_canvas_set_space_mode(SpaceMode mode) {
    s_scene_editor_canvas_space_mode = space_mode_adapter_resolve(mode);
    s_scene_editor_canvas_projection_mode = SPACE_MODE_2D;
}

void scene_editor_canvas_set_mode_route(const SimModeRoute *route) {
    SpaceModeViewContext view_ctx = space_mode_adapter_build_canvas_view_context_for_route(route,
                                                                                            0,
                                                                                            0,
                                                                                            1,
                                                                                            1);
    s_scene_editor_canvas_space_mode = view_ctx.requested_mode;
    s_scene_editor_canvas_projection_mode = view_ctx.projection_mode;
}

float scene_editor_canvas_object_visual_radius_px(const PresetObject *obj, int canvas_w) {
    if (!obj) return (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX;
    float radius = obj->size_x * (float)canvas_w * object_depth_visual_scale(obj);
    if (radius < (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX) {
        radius = (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX;
    }
    return radius;
}

void scene_editor_canvas_object_visual_half_sizes_px(const PresetObject *obj,
                                                     int canvas_w,
                                                     int canvas_h,
                                                     float *out_half_w_px,
                                                     float *out_half_h_px) {
    if (!obj) return;
    float depth_scale = object_depth_visual_scale(obj);
    float half_w = obj->size_x * (float)canvas_w * depth_scale;
    float half_h = obj->size_y * (float)canvas_h * depth_scale;
    if (half_w < (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX) half_w = (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX;
    if (half_h < (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX) half_h = (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX;
    if (out_half_w_px) *out_half_w_px = half_w;
    if (out_half_h_px) *out_half_h_px = half_h;
}

float scene_editor_canvas_object_handle_length_px(const PresetObject *obj,
                                                  int canvas_w,
                                                  int canvas_h) {
    if (!obj) return 0.0f;
    if (obj->type == PRESET_OBJECT_CIRCLE) {
        return scene_editor_canvas_object_visual_radius_px(obj, canvas_w);
    }
    float half_w = 0.0f, half_h = 0.0f;
    scene_editor_canvas_object_visual_half_sizes_px(obj, canvas_w, canvas_h, &half_w, &half_h);
    (void)half_h; // currently unused; handle follows the box's local +X axis.
    return half_w;
}

float scene_editor_canvas_handle_size_px(int canvas_w, int canvas_h) {
    if (canvas_w <= 0 || canvas_h <= 0) return 0.0f;
    float min_dim = (float)((canvas_w < canvas_h) ? canvas_w : canvas_h);
    float size = min_dim * 0.04f; // scale with canvas while staying visible when zoomed.
    if (size < 12.0f) size = 12.0f;
    if (size > 36.0f) size = 36.0f;
    return size;
}

float scene_editor_canvas_handle_size_norm(int canvas_w, int canvas_h) {
    float size_px = scene_editor_canvas_handle_size_px(canvas_w, canvas_h);
    if (size_px <= 0.0f) return 0.0f;
    float min_dim = (float)((canvas_w < canvas_h) ? canvas_w : canvas_h);
    if (min_dim <= 0.0f) return 0.0f;
    return size_px / min_dim;
}

void scene_editor_canvas_project(int canvas_x,
                                 int canvas_y,
                                 int canvas_w,
                                 int canvas_h,
                                 float px,
                                 float py,
                                 int *out_x,
                                 int *out_y) {
    SpaceModeViewContext view_ctx = space_mode_adapter_build_canvas_view_context_ex(
        s_scene_editor_canvas_space_mode,
        s_scene_editor_canvas_projection_mode,
        canvas_x,
        canvas_y,
        canvas_w,
        canvas_h);
    space_mode_adapter_world_to_screen(&view_ctx, px, py, out_x, out_y);
}

void scene_editor_canvas_to_normalized(int canvas_x,
                                       int canvas_y,
                                       int canvas_w,
                                       int canvas_h,
                                       int sx,
                                       int sy,
                                       float *out_x,
                                       float *out_y) {
    SpaceModeViewContext view_ctx = space_mode_adapter_build_canvas_view_context_ex(
        s_scene_editor_canvas_space_mode,
        s_scene_editor_canvas_projection_mode,
        canvas_x,
        canvas_y,
        canvas_w,
        canvas_h);
    space_mode_adapter_screen_to_world_clamped(&view_ctx, sx, sy, out_x, out_y);
}

void scene_editor_canvas_to_import_normalized(int canvas_x,
                                              int canvas_y,
                                              int canvas_w,
                                              int canvas_h,
                                              int sx,
                                              int sy,
                                              float *out_x,
                                              float *out_y) {
    SpaceModeViewContext view_ctx = space_mode_adapter_build_canvas_view_context_ex(
        s_scene_editor_canvas_space_mode,
        s_scene_editor_canvas_projection_mode,
        canvas_x,
        canvas_y,
        canvas_w,
        canvas_h);
    space_mode_adapter_screen_to_import_world_clamped(&view_ctx, sx, sy, out_x, out_y);
}

static bool import_hit_oriented_box(const ImportedShape *imp,
                                    const ShapeAssetBounds *b,
                                    float px_norm,
                                    float py_norm,
                                    float padding_norm) {
    if (!imp || !b || !b->valid) return false;
    float size_x = b->max_x - b->min_x;
    float size_y = b->max_y - b->min_y;
    float max_dim = fmaxf(size_x, size_y);
    if (max_dim <= 0.0001f) return false;
    const float desired_fit = 0.25f;
    float norm = (imp->scale * desired_fit) / max_dim;
    float half_w = 0.5f * size_x * norm;
    float half_h = 0.5f * size_y * norm;

    float dx = px_norm - imp->position_x;
    float dy = py_norm - imp->position_y;
    float cos_a = cosf(imp->rotation_deg * (float)M_PI / 180.0f);
    float sin_a = sinf(imp->rotation_deg * (float)M_PI / 180.0f);
    float local_x = dx * cos_a + dy * sin_a;
    float local_y = -dx * sin_a + dy * cos_a;
    float pad = padding_norm;
    if (fabsf(local_x) <= half_w + pad && fabsf(local_y) <= half_h + pad) {
        return true;
    }
    return false;
}

int scene_editor_canvas_hit_import(const FluidScenePreset *preset,
                                   const ShapeAssetLibrary *lib,
                                   int canvas_x,
                                   int canvas_y,
                                   int canvas_w,
                                   int canvas_h,
                                   int px,
                                   int py) {
    if (!preset || !lib || canvas_w <= 0 || canvas_h <= 0) return -1;
    float nx = 0.0f, ny = 0.0f;
    scene_editor_canvas_to_import_normalized(canvas_x,
                                             canvas_y,
                                             canvas_w,
                                             canvas_h,
                                             px,
                                             py,
                                             &nx,
                                             &ny);
    float pad_norm = scene_editor_canvas_handle_size_norm(canvas_w, canvas_h);
    for (int ii = (int)preset->import_shape_count - 1; ii >= 0; --ii) {
        const ImportedShape *imp = &preset->import_shapes[ii];
        if (!imp->enabled) continue;
        const ShapeAsset *asset = shape_lookup_from_path(lib, imp->path);
        if (!asset) continue;
        ShapeAssetBounds b;
        if (!shape_asset_bounds(asset, &b) || !b.valid) continue;
        if (import_hit_oriented_box(imp, &b, nx, ny, pad_norm)) {
            return ii;
        }
    }
    return -1;
}

bool scene_editor_canvas_import_handle_point(int canvas_x,
                                             int canvas_y,
                                             int canvas_w,
                                             int canvas_h,
                                             const ShapeAssetLibrary *lib,
                                             const ImportedShape *imp,
                                             int *out_x,
                                             int *out_y) {
    if (!imp || !out_x || !out_y) return false;
    float scale_px = (float)((canvas_w < canvas_h) ? canvas_w : canvas_h);
    if (scale_px <= 0.0f) return false;

    // Base handle length scales with import size so the knob sits outside the shape.
    float handle_norm = scene_editor_canvas_handle_size_norm(canvas_w, canvas_h);
    if (lib) {
        const ShapeAsset *asset = shape_lookup_from_path(lib, imp->path);
        ShapeAssetBounds b;
        if (asset && shape_asset_bounds(asset, &b) && b.valid) {
            float size_x = b.max_x - b.min_x;
            float size_y = b.max_y - b.min_y;
            float max_dim = fmaxf(size_x, size_y);
            if (max_dim > 0.0001f) {
                const float desired_fit = 0.25f;
                float norm = (imp->scale * desired_fit) / max_dim;
                float half_w = 0.5f * size_x * norm;
                float half_h = 0.5f * size_y * norm;
                float extent = fmaxf(half_w, half_h);
                float margin = scene_editor_canvas_handle_size_norm(canvas_w, canvas_h) * 0.6f;
                handle_norm = extent + margin;
            }
        }
    }
    float angle = imp->rotation_deg * (float)M_PI / 180.0f;
    float hx = imp->position_x + cosf(angle) * (handle_norm * 1.4f);
    float hy = imp->position_y + sinf(angle) * (handle_norm * 1.4f);
    float cx_px = (float)canvas_x + 0.5f * (float)canvas_w;
    float cy_px = (float)canvas_y + 0.5f * (float)canvas_h;
    *out_x = (int)lroundf(cx_px + (hx - 0.5f) * scale_px);
    *out_y = (int)lroundf(cy_px + (hy - 0.5f) * scale_px);
    return true;
}

bool scene_editor_canvas_object_handle_point(const FluidScenePreset *preset,
                                             int canvas_x,
                                             int canvas_y,
                                             int canvas_w,
                                             int canvas_h,
                                             int object_index,
                                             int *out_x,
                                             int *out_y) {
    if (!preset || object_index < 0 || object_index >= (int)preset->object_count) return false;
    if (!out_x || !out_y) return false;
    const PresetObject *obj = &preset->objects[object_index];
    int cx, cy;
    scene_editor_canvas_project(canvas_x,
                                canvas_y,
                                canvas_w,
                                canvas_h,
                                obj->position_x,
                                obj->position_y,
                                &cx,
                                &cy);
    float base_len_px = scene_editor_canvas_object_handle_length_px(obj, canvas_w, canvas_h);
    // Push the handle noticeably beyond the object footprint so it remains clickable at any zoom.
    float handle_len_px = base_len_px * 1.6f
                          + (float)SCENE_EDITOR_OBJECT_HANDLE_MARGIN_PX
                          + scene_editor_canvas_handle_size_px(canvas_w, canvas_h) * 0.6f;
    float angle = obj->angle;
    *out_x = cx + (int)lroundf(cosf(angle) * handle_len_px);
    *out_y = cy + (int)lroundf(sinf(angle) * handle_len_px);
    return true;
}
