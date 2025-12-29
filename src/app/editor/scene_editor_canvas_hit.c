#include "app/editor/scene_editor_canvas.h"
#include "app/shape_lookup.h"

#include <math.h>

static float emitter_visual_radius_norm(const FluidScenePreset *preset,
                                        int emitter_index,
                                        const int *emitter_object_map,
                                        const int *emitter_import_map) {
    if (!preset || emitter_index < 0 || emitter_index >= (int)preset->emitter_count) return 0.08f;
    const FluidEmitter *em = &preset->emitters[emitter_index];
    (void)emitter_object_map;
    (void)emitter_import_map;
    float radius_norm = em->radius;
    if (radius_norm < 0.02f) radius_norm = 0.02f;
    return radius_norm;
}

static int push_hit(SceneEditorHit *out_hits, int max_hits, int count, SceneEditorHit hit) {
    if (!out_hits || max_hits <= 0) return count;
    if (count < max_hits) {
        out_hits[count] = hit;
    }
    return (count < max_hits) ? count + 1 : count;
}

int scene_editor_canvas_collect_hits(const FluidScenePreset *preset,
                                     const ShapeAssetLibrary *lib,
                                     int canvas_x,
                                     int canvas_y,
                                     int canvas_w,
                                     int canvas_h,
                                     int px,
                                     int py,
                                     const int *emitter_object_map,
                                     const int *emitter_import_map,
                                     SceneEditorHit *out_hits,
                                     int max_hits) {
    if (!preset || !out_hits || max_hits <= 0) return 0;
    int count = 0;

    float nx = 0.0f, ny = 0.0f;
    scene_editor_canvas_to_import_normalized(canvas_x,
                                             canvas_y,
                                             canvas_w,
                                             canvas_h,
                                             px,
                                             py,
                                             &nx,
                                             &ny);

    // Emitter handles first so they win overlaps.
    for (int i = (int)preset->emitter_count - 1; i >= 0; --i) {
        int hx = 0, hy = 0;
        float hit_r = 0.0f;
        if (scene_editor_canvas_emitter_handle_point(preset,
                                                     canvas_x,
                                                     canvas_y,
                                                     canvas_w,
                                                     canvas_h,
                                                     i,
                                                     emitter_object_map,
                                                     emitter_import_map,
                                                     &hx,
                                                     &hy,
                                                     &hit_r)) {
            float adx = (float)px - (float)hx;
            float ady = (float)py - (float)hy;
            if ((adx * adx + ady * ady) <= hit_r * hit_r) {
                SceneEditorHit h = {.kind = HIT_EMITTER, .index = i, .drag_mode = DRAG_DIRECTION, .boundary_edge = -1};
                count = push_hit(out_hits, max_hits, count, h);
            }
        }
    }

    // Import handles (topmost first)
    if (lib) {
        float hit_radius_px = scene_editor_canvas_handle_size_px(canvas_w, canvas_h) * 0.6f;
        float hit_r2 = hit_radius_px * hit_radius_px;
        int hx = 0, hy = 0;
        for (int i = (int)preset->import_shape_count - 1; i >= 0; --i) {
            const ImportedShape *imp = &preset->import_shapes[i];
            if (!imp->enabled) continue;
            if (!scene_editor_canvas_import_handle_point(canvas_x,
                                                         canvas_y,
                                                         canvas_w,
                                                         canvas_h,
                                                         lib,
                                                         imp,
                                                         &hx,
                                                         &hy)) {
                continue;
            }
            float dx = (float)px - (float)hx;
            float dy = (float)py - (float)hy;
            if ((dx * dx + dy * dy) <= hit_r2) {
                SceneEditorHit h = {.kind = HIT_IMPORT_HANDLE, .index = i, .drag_mode = DRAG_DIRECTION, .boundary_edge = -1};
                count = push_hit(out_hits, max_hits, count, h);
            }
        }
    }

    // Imports (outline hit)
    if (lib) {
        for (int i = (int)preset->import_shape_count - 1; i >= 0; --i) {
            const ImportedShape *imp = &preset->import_shapes[i];
            if (!imp->enabled) continue;
            const ShapeAsset *asset = shape_lookup_from_path(lib, imp->path);
            if (!asset) continue;
            ShapeAssetBounds b;
            if (!shape_asset_bounds(asset, &b) || !b.valid) continue;
            if (scene_editor_canvas_hit_import(preset,
                                               lib,
                                               canvas_x,
                                               canvas_y,
                                               canvas_w,
                                               canvas_h,
                                               px,
                                               py) == i) {
                SceneEditorHit h = {.kind = HIT_IMPORT, .index = i, .drag_mode = DRAG_POSITION, .boundary_edge = -1};
                count = push_hit(out_hits, max_hits, count, h);
            }
        }
    }

    // Object handles
    const int handle_radius = SCENE_EDITOR_OBJECT_HANDLE_HIT_RADIUS_PX;
    float handle_r2 = (float)(handle_radius * handle_radius);
    for (int i = (int)preset->object_count - 1; i >= 0; --i) {
        int hx = 0, hy = 0;
        if (!scene_editor_canvas_object_handle_point(preset,
                                                     canvas_x,
                                                     canvas_y,
                                                     canvas_w,
                                                     canvas_h,
                                                     i,
                                                     &hx,
                                                     &hy)) {
            continue;
        }
        float dx = (float)px - (float)hx;
        float dy = (float)py - (float)hy;
        if ((dx * dx + dy * dy) <= handle_r2) {
            SceneEditorHit h = {.kind = HIT_OBJECT_HANDLE, .index = i, .drag_mode = DRAG_DIRECTION, .boundary_edge = -1};
            count = push_hit(out_hits, max_hits, count, h);
        }
    }

    // Object bodies
    for (int i = (int)preset->object_count - 1; i >= 0; --i) {
        const PresetObject *obj = &preset->objects[i];
        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                    obj->position_x, obj->position_y,
                                    &cx, &cy);
        if (obj->type == PRESET_OBJECT_CIRCLE) {
            int radius = (int)lroundf(scene_editor_canvas_object_visual_radius_px(obj, canvas_w));
            float dx = (float)px - (float)cx;
            float dy = (float)py - (float)cy;
            if (dx * dx + dy * dy <= (float)(radius * radius)) {
                SceneEditorHit h = {.kind = HIT_OBJECT, .index = i, .drag_mode = DRAG_POSITION, .boundary_edge = -1};
                count = push_hit(out_hits, max_hits, count, h);
            }
        } else {
            float half_w = 0.0f, half_h = 0.0f;
            scene_editor_canvas_object_visual_half_sizes_px(obj, canvas_w, canvas_h, &half_w, &half_h);
            float dx = (float)px - (float)cx;
            float dy = (float)py - (float)cy;
            float cos_a = cosf(obj->angle);
            float sin_a = sinf(obj->angle);
            float local_x = dx * cos_a + dy * sin_a;
            float local_y = -dx * sin_a + dy * cos_a;
            if (fabsf(local_x) <= half_w && fabsf(local_y) <= half_h) {
                SceneEditorHit h = {.kind = HIT_OBJECT, .index = i, .drag_mode = DRAG_POSITION, .boundary_edge = -1};
                count = push_hit(out_hits, max_hits, count, h);
            }
        }
    }

    // Emitters (body)
    for (int i = (int)preset->emitter_count - 1; i >= 0; --i) {
        const FluidEmitter *em = &preset->emitters[i];
        float radius_norm = emitter_visual_radius_norm(preset, i, emitter_object_map, emitter_import_map);
        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                    em->position_x, em->position_y,
                                    &cx, &cy);
        int radius_px = (int)(radius_norm * (float)fmin(canvas_w, canvas_h));
        if (radius_px < 4) radius_px = 4;
        float dx = (float)px - (float)cx;
        float dy = (float)py - (float)cy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= (float)radius_px) {
            SceneEditorHit h = {.kind = HIT_EMITTER, .index = i, .drag_mode = DRAG_POSITION, .boundary_edge = -1};
            count = push_hit(out_hits, max_hits, count, h);
        }
    }

    int edge = scene_editor_canvas_hit_edge(canvas_x,
                                            canvas_y,
                                            canvas_w,
                                            canvas_h,
                                            px,
                                            py);
    if (edge >= 0) {
        SceneEditorHit h = {.kind = HIT_BOUNDARY_EDGE, .index = -1, .drag_mode = DRAG_NONE, .boundary_edge = edge};
        count = push_hit(out_hits, max_hits, count, h);
    }

    return count;
}

int scene_editor_canvas_hit_test(const FluidScenePreset *preset,
                                 int canvas_x,
                                 int canvas_y,
                                 int canvas_w,
                                 int canvas_h,
                                 int px,
                                 int py,
                                 EditorDragMode *mode,
                                 const int *emitter_object_map,
                                 const int *emitter_import_map) {
    if (!preset) return -1;
    int closest = -1;
    float best_dist = 1e9f;

    for (size_t i = 0; i < preset->emitter_count; ++i) {
        int hx = 0, hy = 0;
        float hit_r = 0.0f;
        if (scene_editor_canvas_emitter_handle_point(preset,
                                                     canvas_x,
                                                     canvas_y,
                                                     canvas_w,
                                                     canvas_h,
                                                     (int)i,
                                                     emitter_object_map,
                                                     emitter_import_map,
                                                     &hx,
                                                     &hy,
                                                     &hit_r)) {
            float adx = (float)px - (float)hx;
            float ady = (float)py - (float)hy;
            float adist = sqrtf(adx * adx + ady * ady);
            if (adist <= hit_r && adist < best_dist) {
                closest = (int)i;
                best_dist = adist;
                if (mode) *mode = DRAG_DIRECTION;
            }
        }

        const FluidEmitter *em = &preset->emitters[i];
        float radius_norm = emitter_visual_radius_norm(preset, (int)i, emitter_object_map, emitter_import_map);
        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                    em->position_x, em->position_y,
                                    &cx, &cy);
        int radius_px = (int)(radius_norm * (float)fmin(canvas_w, canvas_h));
        if (radius_px < 4) radius_px = 4;
        float dx = (float)px - (float)cx;
        float dy = (float)py - (float)cy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= (float)radius_px && dist < best_dist) {
            closest = (int)i;
            best_dist = dist;
            if (mode) *mode = DRAG_POSITION;
        }
    }
    return closest;
}

int scene_editor_canvas_hit_object(const FluidScenePreset *preset,
                                   int canvas_x,
                                   int canvas_y,
                                   int canvas_w,
                                   int canvas_h,
                                   int px,
                                   int py) {
    if (!preset) return -1;
    for (size_t i = 0; i < preset->object_count; ++i) {
        const PresetObject *obj = &preset->objects[i];
        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                    obj->position_x, obj->position_y,
                                    &cx, &cy);
        if (obj->type == PRESET_OBJECT_CIRCLE) {
            int radius = (int)lroundf(scene_editor_canvas_object_visual_radius_px(obj, canvas_w));
            float dx = (float)px - (float)cx;
            float dy = (float)py - (float)cy;
            if (dx * dx + dy * dy <= (float)(radius * radius)) {
                return (int)i;
            }
        } else {
            float half_w = 0.0f, half_h = 0.0f;
            scene_editor_canvas_object_visual_half_sizes_px(obj, canvas_w, canvas_h, &half_w, &half_h);
            float dx = (float)px - (float)cx;
            float dy = (float)py - (float)cy;
            float cos_a = cosf(obj->angle);
            float sin_a = sinf(obj->angle);
            float local_x = dx * cos_a + dy * sin_a;
            float local_y = -dx * sin_a + dy * cos_a;
            if (fabsf(local_x) <= half_w && fabsf(local_y) <= half_h) {
                return (int)i;
            }
        }
    }
    return -1;
}

int scene_editor_canvas_hit_object_handle(const FluidScenePreset *preset,
                                          int canvas_x,
                                          int canvas_y,
                                          int canvas_w,
                                          int canvas_h,
                                          int px,
                                          int py) {
    if (!preset) return -1;
    const int handle_radius = SCENE_EDITOR_OBJECT_HANDLE_HIT_RADIUS_PX;
    for (size_t i = 0; i < preset->object_count; ++i) {
        int hx = 0, hy = 0;
        if (!scene_editor_canvas_object_handle_point(preset,
                                                     canvas_x,
                                                     canvas_y,
                                                     canvas_w,
                                                     canvas_h,
                                                     (int)i,
                                                     &hx,
                                                     &hy)) {
            continue;
        }
        float dx = (float)px - (float)hx;
        float dy = (float)py - (float)hy;
        if ((dx * dx + dy * dy) <= (float)(handle_radius * handle_radius)) {
            return (int)i;
        }
    }
    return -1;
}

int scene_editor_canvas_hit_edge(int canvas_x,
                                 int canvas_y,
                                 int canvas_w,
                                 int canvas_h,
                                 int px,
                                 int py) {
    int canvas_right = canvas_x + canvas_w;
    int canvas_bottom = canvas_y + canvas_h;
    int margin = SCENE_EDITOR_BOUNDARY_HIT_MARGIN;

    bool inside_x = (px >= canvas_x - margin) && (px <= canvas_right + margin);
    bool inside_y = (py >= canvas_y - margin) && (py <= canvas_bottom + margin);
    if (!inside_x || !inside_y) return -1;

    if (py >= canvas_y - margin && py <= canvas_y + margin) {
        return BOUNDARY_EDGE_TOP;
    }
    if (py >= canvas_bottom - margin && py <= canvas_bottom + margin) {
        return BOUNDARY_EDGE_BOTTOM;
    }
    if (px >= canvas_x - margin && px <= canvas_x + margin) {
        return BOUNDARY_EDGE_LEFT;
    }
    if (px >= canvas_right - margin && px <= canvas_right + margin) {
        return BOUNDARY_EDGE_RIGHT;
    }
    return -1;
}

bool scene_editor_canvas_emitter_handle_point(const FluidScenePreset *preset,
                                              int canvas_x,
                                              int canvas_y,
                                              int canvas_w,
                                              int canvas_h,
                                              int emitter_index,
                                              const int *emitter_object_map,
                                              const int *emitter_import_map,
                                              int *out_x,
                                              int *out_y,
                                              float *out_hit_radius_px) {
    if (!preset || emitter_index < 0 || emitter_index >= (int)preset->emitter_count) return false;
    if (!out_x || !out_y) return false;
    const FluidEmitter *em = &preset->emitters[emitter_index];
    float radius_norm = emitter_visual_radius_norm(preset, emitter_index, emitter_object_map, emitter_import_map);
    int cx, cy;
    scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                em->position_x, em->position_y,
                                &cx, &cy);
    float min_dim = (float)((canvas_w < canvas_h) ? canvas_w : canvas_h);
    int base_px = (int)(radius_norm * min_dim);
    // Keep a minimum visible reach even for tiny radii, but do not scale with object/import size
    // so emitter handle remains independent from the object handle.
    int min_handle_px = (int)lroundf(scene_editor_canvas_handle_size_px(canvas_w, canvas_h));
    if (base_px < min_handle_px) base_px = min_handle_px;

    int arrow_len = base_px + 30;
    int hx = cx + (int)(em->dir_x * arrow_len);
    int hy = cy + (int)(em->dir_y * arrow_len);
    float hit_r = scene_editor_canvas_handle_size_px(canvas_w, canvas_h) * 0.4f;
    if (hit_r < 6.0f) hit_r = 6.0f;
    *out_x = hx;
    *out_y = hy;
    if (out_hit_radius_px) *out_hit_radius_px = hit_r;
    return true;
}
