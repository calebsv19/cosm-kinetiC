#include "render/debug_draw_objects.h"

#include <math.h>
#include <stdlib.h>

#include "app/shape_lookup.h"
#include "geo/shape_asset.h"
#include "kit_viz.h"
#include "render/import_project.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define OBJECT_BORDER_THICKNESS 2

static float *g_polyline_xs = NULL;
static float *g_polyline_ys = NULL;
static KitVizVecSegment *g_polyline_segments = NULL;
static size_t g_polyline_capacity = 0;

static float safe_scale(int target, int source) {
    if (source <= 0) return 1.0f;
    if (target <= 0) return 1.0f;
    return (float)target / (float)source;
}

static bool ensure_polyline_buffers(size_t point_count) {
    if (point_count < 2) return false;
    if (point_count <= g_polyline_capacity) return true;

    float *new_xs = (float *)realloc(g_polyline_xs, point_count * sizeof(float));
    if (!new_xs) return false;
    g_polyline_xs = new_xs;

    float *new_ys = (float *)realloc(g_polyline_ys, point_count * sizeof(float));
    if (!new_ys) return false;
    g_polyline_ys = new_ys;

    size_t seg_count = point_count - 1u;
    KitVizVecSegment *new_segments = (KitVizVecSegment *)realloc(
        g_polyline_segments, seg_count * sizeof(KitVizVecSegment));
    if (!new_segments) return false;
    g_polyline_segments = new_segments;
    g_polyline_capacity = point_count;
    return true;
}

static void draw_polyline(SDL_Renderer *renderer, const SDL_Point *pts, int count) {
    if (!renderer || !pts || count < 2) return;
    if (!ensure_polyline_buffers((size_t)count)) {
        for (int i = 1; i < count; ++i) {
            SDL_RenderDrawLine(renderer,
                               pts[i - 1].x, pts[i - 1].y,
                               pts[i].x, pts[i].y);
        }
        return;
    }

    for (int i = 0; i < count; ++i) {
        g_polyline_xs[i] = (float)pts[i].x;
        g_polyline_ys[i] = (float)pts[i].y;
    }

    size_t segment_count = 0;
    CoreResult r = kit_viz_build_polyline_segments(g_polyline_xs,
                                                   g_polyline_ys,
                                                   (uint32_t)count,
                                                   g_polyline_segments,
                                                   (size_t)count - 1u,
                                                   &segment_count);
    if (r.code != CORE_OK) {
        for (int i = 1; i < count; ++i) {
            SDL_RenderDrawLine(renderer,
                               pts[i - 1].x, pts[i - 1].y,
                               pts[i].x, pts[i].y);
        }
        return;
    }

    for (size_t i = 0; i < segment_count; ++i) {
        SDL_RenderDrawLine(renderer,
                           (int)lroundf(g_polyline_segments[i].x0),
                           (int)lroundf(g_polyline_segments[i].y0),
                           (int)lroundf(g_polyline_segments[i].x1),
                           (int)lroundf(g_polyline_segments[i].y1));
    }
}

static void draw_poly_outline(const SceneObject *obj,
                              float scale_x,
                              float scale_y,
                              SDL_Renderer *renderer,
                              SDL_Color color) {
    if (!obj || !renderer) return;
    const RigidPoly *poly = &obj->body.poly;
    if (!poly->verts || poly->count < 3) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Point *pts = (SDL_Point *)alloca((size_t)(poly->count + 1) * sizeof(SDL_Point));
    for (int i = 0; i < poly->count; ++i) {
        Vec2 v = poly->verts[i];
        float rx = v.x * cosf(obj->body.angle) - v.y * sinf(obj->body.angle);
        float ry = v.x * sinf(obj->body.angle) + v.y * cosf(obj->body.angle);
        float wx = obj->body.position.x + rx;
        float wy = obj->body.position.y + ry;
        pts[i].x = (int)lroundf(wx * scale_x);
        pts[i].y = (int)lroundf(wy * scale_y);
    }
    pts[poly->count] = pts[0];
    draw_polyline(renderer, pts, poly->count + 1);
}

static void draw_import_outlines(const SceneState *scene,
                                 SDL_Renderer *renderer,
                                 int window_w,
                                 int window_h,
                                 const bool *skip_import) {
    if (!scene || !renderer || !scene->shape_library) return;
    if (scene->import_shape_count == 0) return;
    if (!scene->config) return;
    SDL_Color col = {200, 220, 255, 170};

    float span_x_cfg = 1.0f, span_y_cfg = 1.0f;
    import_compute_span_from_window(scene->config->window_w, scene->config->window_h, &span_x_cfg, &span_y_cfg);

    for (size_t i = 0; i < scene->import_shape_count; ++i) {
        const ImportedShape *imp = &scene->import_shapes[i];
        if (!imp->enabled) continue;
        if (skip_import && skip_import[i]) continue;
        const ShapeAsset *asset = shape_lookup_from_path(scene->shape_library, imp->path);
        if (!asset) continue;
        ShapeAssetBounds b;
        if (!shape_asset_bounds(asset, &b) || !b.valid) continue;
        float size_x = b.max_x - b.min_x;
        float size_y = b.max_y - b.min_y;
        float max_dim = fmaxf(size_x, size_y);
        if (max_dim <= 0.0001f) continue;
        const float desired_fit = 0.25f; // match editor default footprint
        float norm = (imp->scale * desired_fit) / max_dim;
        float cx = 0.5f * (b.min_x + b.max_x);
        float cy = 0.5f * (b.min_y + b.max_y);
        SDL_Color use_col = imp->gravity_enabled ? (SDL_Color){120, 240, 120, 180} : col;
        for (size_t pi = 0; pi < asset->path_count; ++pi) {
            const ShapeAssetPath *path = &asset->paths[pi];
            if (!path || path->point_count < 2) continue;
            for (size_t j = 1; j < path->point_count; ++j) {
                ShapeAssetPoint a = path->points[j - 1];
                ShapeAssetPoint bpt = path->points[j];
                float dax = (a.x - cx) * norm;
                float day = (a.y - cy) * norm;
                float dbx = (bpt.x - cx) * norm;
                float dby = (bpt.y - cy) * norm;
                ImportProjectParams proj = {
                    .grid_w = scene->config->grid_w,
                    .grid_h = scene->config->grid_h,
                    .window_w = window_w,
                    .window_h = window_h,
                    .span_x_cfg = span_x_cfg,
                    .span_y_cfg = span_y_cfg,
                    .pos_x = imp->position_x,
                    .pos_y = imp->position_y,
                    .rotation_deg = imp->rotation_deg,
                    .scale = imp->scale,
                    .bounds = &b
                };
                ImportProjectPoint pa = import_project_point(&proj, dax, day);
                ImportProjectPoint pb = import_project_point(&proj, dbx, dby);
                if (!pa.valid || !pb.valid) continue;
                int ax = (int)lroundf(pa.screen_x);
                int ay = (int)lroundf(pa.screen_y);
                int bx = (int)lroundf(pb.screen_x);
                int by = (int)lroundf(pb.screen_y);
                SDL_SetRenderDrawColor(renderer, use_col.r, use_col.g, use_col.b, use_col.a);
                SDL_RenderDrawLine(renderer, ax, ay, bx, by);
            }
        }
    }
}

static void draw_rotated_box_outline(const SceneObject *obj,
                                     float scale_x,
                                     float scale_y,
                                     SDL_Renderer *renderer,
                                     SDL_Color color) {
    if (!obj || !renderer) return;
    float cos_a = cosf(obj->body.angle);
    float sin_a = sinf(obj->body.angle);
    SDL_SetRenderDrawColor(renderer,
                           color.r,
                           color.g,
                           color.b,
                           color.a);

    for (int t = 0; t < OBJECT_BORDER_THICKNESS; ++t) {
        float shrink_x = (scale_x > 0.0f) ? (float)t / scale_x : 0.0f;
        float shrink_y = (scale_y > 0.0f) ? (float)t / scale_y : 0.0f;
        float hx = obj->body.half_extents.x - shrink_x;
        float hy = obj->body.half_extents.y - shrink_y;
        if (hx <= 0.0f || hy <= 0.0f) break;

        SDL_Point pts[5];
        const float corner_x[4] = {-hx,  hx,  hx, -hx};
        const float corner_y[4] = {-hy, -hy,  hy,  hy};
        for (int i = 0; i < 4; ++i) {
            float lx = corner_x[i];
            float ly = corner_y[i];
            float rx = lx * cos_a - ly * sin_a;
            float ry = lx * sin_a + ly * cos_a;
            float world_x = obj->body.position.x + rx;
            float world_y = obj->body.position.y + ry;
            pts[i].x = (int)lroundf(world_x * scale_x);
            pts[i].y = (int)lroundf(world_y * scale_y);
        }
        pts[4] = pts[0];
    draw_polyline(renderer, pts, 5);
}
}

void debug_draw_object_borders(const SceneState *scene,
                               SDL_Renderer *renderer,
                               int window_w,
                               int window_h) {
#if OBJECT_BORDER_THICKNESS <= 0
    (void)scene;
    (void)renderer;
    (void)window_w;
    (void)window_h;
    return;
#else
    if (!scene || !scene->config || !renderer) return;
    const ObjectManager *objects = &scene->objects;

    float scale_x = safe_scale(window_w, scene->config->window_w);
    float scale_y = safe_scale(window_h, scene->config->window_h);

    SDL_Color circle_color = {255, 80, 80, 255};
    SDL_Color box_color    = {170, 120, 80, 255};
    SDL_Color gravity_color = {90, 220, 120, 255};
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif

    bool skip_imp[MAX_IMPORTED_SHAPES] = {0};
    bool skip_obj[MAX_PRESET_OBJECTS] = {0};
    if (scene && scene->preset) {
        for (size_t ei = 0; ei < scene->preset->emitter_count && ei < MAX_FLUID_EMITTERS; ++ei) {
            int ao = scene->preset->emitters[ei].attached_object;
            int ai = scene->preset->emitters[ei].attached_import;
            if (ao >= 0 && ao < (int)MAX_PRESET_OBJECTS) skip_obj[ao] = true;
            if (ai >= 0 && ai < (int)MAX_IMPORTED_SHAPES) skip_imp[ai] = true;
        }
    }

    // Always draw import outlines, even if no physics objects are present, unless they are emitters.
    draw_import_outlines(scene, renderer, window_w, window_h, skip_imp);

    const int segments = 48;
    if (objects && objects->count > 0) {
        for (int i = 0; i < objects->count; ++i) {
            const SceneObject *obj = &objects->objects[i];
            if (scene && scene->preset && i < (int)scene->preset->object_count && skip_obj[i]) {
                continue; // skip drawing if this preset object is acting as an emitter
            }
            int cx = (int)lroundf(obj->body.position.x * scale_x);
            int cy = (int)lroundf(obj->body.position.y * scale_y);

            SDL_Color base_color = (obj->body.gravity_enabled)
                                       ? gravity_color
                                       : (obj->type == SCENE_OBJECT_CIRCLE ? circle_color : box_color);
            if (obj->type == SCENE_OBJECT_CIRCLE) {
                float radius_scale = (scale_x + scale_y) * 0.5f;
                if (radius_scale <= 0.0f) radius_scale = scale_x > 0.0f ? scale_x : 1.0f;
                int radius = (int)lroundf(obj->body.radius * radius_scale);
                if (radius < 2) radius = 2;
                SDL_SetRenderDrawColor(renderer,
                                       base_color.r,
                                       base_color.g,
                                       base_color.b,
                                       base_color.a);
                for (int t = 0; t < OBJECT_BORDER_THICKNESS; ++t) {
                    int r = radius - t;
                    if (r <= 0) break;
                    float prev_x = (float)(cx + r);
                    float prev_y = (float)cy;
                    for (int seg = 1; seg <= segments; ++seg) {
                        float theta = (float)seg / (float)segments * 2.0f * (float)M_PI;
                        float cur_x = (float)cx + cosf(theta) * (float)r;
                        float cur_y = (float)cy + sinf(theta) * (float)r;
                        SDL_RenderDrawLine(renderer,
                                           (int)lroundf(prev_x),
                                           (int)lroundf(prev_y),
                                           (int)lroundf(cur_x),
                                           (int)lroundf(cur_y));
                        prev_x = cur_x;
                        prev_y = cur_y;
                    }
                }
            } else if (obj->type == SCENE_OBJECT_BOX) {
                draw_rotated_box_outline(obj, scale_x, scale_y, renderer, base_color);
            } else if (obj->type == SCENE_OBJECT_POLY) {
                SDL_Color collider_color = (SDL_Color){255, 180, 60, 255};
                draw_poly_outline(obj, scale_x, scale_y, renderer, base_color);
                // Overlay collider outline to make colliders stand out from base shape
                draw_poly_outline(obj, scale_x, scale_y, renderer, collider_color);
            }
        }
    }
#endif
}
