#include "render/retained_runtime_scene_overlay.h"

#include <math.h>
#include <string.h>

#include "app/editor/scene_editor_viewport.h"
#include "app/sim_runtime_3d_anchor.h"
#include "render/retained_runtime_scene_overlay_geom.h"
#include "render/retained_runtime_scene_overlay_readout.h"
#include "render/retained_runtime_scene_overlay_space.h"

static SDL_Color COLOR_DYNAMIC = {255, 184, 90, 255};
static SDL_Color COLOR_STATIC = {96, 172, 255, 255};
static SDL_Color COLOR_SOURCE = {246, 233, 90, 255};
static SDL_Color COLOR_JET = {74, 232, 124, 255};
static SDL_Color COLOR_SINK = {232, 96, 136, 255};
static SDL_Color COLOR_DOMAIN_DERIVED = {132, 164, 188, 188};
static SDL_Color COLOR_DOMAIN_AUTHORED = {138, 198, 154, 210};
static SDL_Color COLOR_AXIS_X = {232, 84, 79, 255};
static SDL_Color COLOR_AXIS_Y = {92, 194, 108, 255};
static SDL_Color COLOR_AXIS_Z = {84, 156, 255, 255};
static SDL_Color COLOR_ORIGIN = {236, 240, 245, 210};
static SDL_Color COLOR_FLUID_LOW = {102, 196, 255, 88};
static SDL_Color COLOR_FLUID_HIGH = {214, 245, 255, 192};
static SDL_Color COLOR_SOLID_SLICE = {178, 136, 220, 136};
static SDL_Color COLOR_SLICE_PLANE = {108, 144, 176, 124};
static SDL_Color COLOR_SLICE_PROJECTION = {176, 196, 214, 156};
static SDL_Color COLOR_MESH_PREVIEW = {188, 128, 255, 214};

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MESH_PREVIEW_EDGE_CACHE_CAPACITY 64
#define MESH_PREVIEW_EDGE_BUDGET 2048u

typedef struct MeshPreviewEdgeCacheEntry {
    bool occupied;
    bool attempted;
    bool drawable;
    char key[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX];
    CoreMeshPreviewRuntimePayload payload;
} MeshPreviewEdgeCacheEntry;

static MeshPreviewEdgeCacheEntry g_mesh_preview_edge_cache[MESH_PREVIEW_EDGE_CACHE_CAPACITY];

static float overlay_clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static SDL_Color lighten_color(SDL_Color color, float factor) {
    SDL_Color result = color;
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    result.r = (Uint8)(color.r + (Uint8)((255 - color.r) * factor));
    result.g = (Uint8)(color.g + (Uint8)((255 - color.g) * factor));
    result.b = (Uint8)(color.b + (Uint8)((255 - color.b) * factor));
    return result;
}

static SDL_Color lerp_color(SDL_Color a, SDL_Color b, float t) {
    SDL_Color result;
    t = overlay_clampf(t, 0.0f, 1.0f);
    result.r = (Uint8)(a.r + (Uint8)((float)(b.r - a.r) * t));
    result.g = (Uint8)(a.g + (Uint8)((float)(b.g - a.g) * t));
    result.b = (Uint8)(a.b + (Uint8)((float)(b.b - a.b) * t));
    result.a = (Uint8)(a.a + (Uint8)((float)(b.a - a.a) * t));
    return result;
}

static void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    if (!renderer || radius <= 0) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_Rect dot = {cx + dx, cy + dy, 1, 1};
                SDL_RenderFillRect(renderer, &dot);
            }
        }
    }
}

static void draw_square(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_Rect rect;
    if (!renderer || radius < 0) return;
    rect.x = cx - radius;
    rect.y = cy - radius;
    rect.w = radius * 2 + 1;
    rect.h = radius * 2 + 1;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

static void draw_line(SDL_Renderer *renderer, int x0, int y0, int x1, int y1, SDL_Color color) {
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
}

static void project_point(const SceneEditorViewportState *viewport,
                          int window_w,
                          int window_h,
                          CoreObjectVec3 point,
                          int *out_x,
                          int *out_y) {
    scene_editor_viewport_project_point3(viewport,
                                         0,
                                         0,
                                         window_w,
                                         window_h,
                                         (float)point.x,
                                         (float)point.y,
                                         (float)point.z,
                                         out_x,
                                         out_y);
}

static void draw_segment(SDL_Renderer *renderer,
                         const SceneEditorViewportState *viewport,
                         int window_w,
                         int window_h,
                         CoreObjectVec3 a,
                         CoreObjectVec3 b,
                         SDL_Color color) {
    int ax = 0;
    int ay = 0;
    int bx = 0;
    int by = 0;
    project_point(viewport, window_w, window_h, a, &ax, &ay);
    project_point(viewport, window_w, window_h, b, &bx, &by);
    draw_line(renderer, ax, ay, bx, by, color);
}

static CoreObjectVec3 rotate_xyz_degrees(CoreObjectVec3 value, CoreObjectVec3 rotation_deg) {
    const double rx = rotation_deg.x * M_PI / 180.0;
    const double ry = rotation_deg.y * M_PI / 180.0;
    const double rz = rotation_deg.z * M_PI / 180.0;
    const double cx = cos(rx);
    const double sx = sin(rx);
    const double cy = cos(ry);
    const double sy = sin(ry);
    const double cz = cos(rz);
    const double sz = sin(rz);
    CoreObjectVec3 r = value;
    CoreObjectVec3 tmp = r;

    tmp.y = r.y * cx - r.z * sx;
    tmp.z = r.y * sx + r.z * cx;
    r = tmp;

    tmp = r;
    tmp.x = r.x * cy + r.z * sy;
    tmp.z = -r.x * sy + r.z * cy;
    r = tmp;

    tmp = r;
    tmp.x = r.x * cz - r.y * sz;
    tmp.y = r.x * sz + r.y * cz;
    return tmp;
}

static CoreObjectVec3 transform_mesh_preview_point(
    CoreObjectVec3 local,
    const PhysicsSimRuntimeMeshPreviewInstance *preview) {
    CoreObjectVec3 scaled = {0};
    CoreObjectVec3 rotated = {0};
    if (!preview) return local;
    scaled.x = local.x * preview->transform_scale.x;
    scaled.y = local.y * preview->transform_scale.y;
    scaled.z = local.z * preview->transform_scale.z;
    rotated = rotate_xyz_degrees(scaled, preview->transform_rotation_deg);
    rotated.x += preview->transform_position.x;
    rotated.y += preview->transform_position.y;
    rotated.z += preview->transform_position.z;
    return rotated;
}

static MeshPreviewEdgeCacheEntry *mesh_preview_edge_cache_entry(const char *key) {
    MeshPreviewEdgeCacheEntry *free_entry = NULL;
    if (!key || !key[0]) return NULL;
    for (int i = 0; i < MESH_PREVIEW_EDGE_CACHE_CAPACITY; ++i) {
        MeshPreviewEdgeCacheEntry *entry = &g_mesh_preview_edge_cache[i];
        if (entry->occupied && strcmp(entry->key, key) == 0) return entry;
        if (!entry->occupied && !free_entry) free_entry = entry;
    }
    if (!free_entry) return NULL;
    memset(free_entry, 0, sizeof(*free_entry));
    core_mesh_preview_runtime_payload_init(&free_entry->payload);
    snprintf(free_entry->key, sizeof(free_entry->key), "%s", key);
    free_entry->occupied = true;
    return free_entry;
}

static const CoreMeshPreviewRuntimePayload *mesh_preview_edges_for_instance(
    const PhysicsSimRuntimeMeshPreviewInstance *preview) {
    MeshPreviewEdgeCacheEntry *entry = NULL;
    CoreResult result = core_result_ok();
    if (!preview || !preview->runtime_path_resolved || !preview->runtime_mesh_path[0]) return NULL;
    entry = mesh_preview_edge_cache_entry(preview->runtime_mesh_path);
    if (!entry) return NULL;
    if (entry->attempted) return entry->drawable ? &entry->payload : NULL;

    entry->attempted = true;
    if (preview->preview_file_readable &&
        preview->preview_schema_supported &&
        preview->metadata.mode == CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1 &&
        preview->preview_path[0]) {
        result = core_mesh_preview_load_file(preview->preview_path, &entry->payload);
    } else {
        result = core_mesh_preview_build_from_runtime_file(
            preview->runtime_mesh_path,
            CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1,
            MESH_PREVIEW_EDGE_BUDGET,
            &entry->payload);
    }
    entry->drawable =
        result.code == CORE_OK &&
        entry->payload.mode == CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1 &&
        entry->payload.edge_count > 0u &&
        entry->payload.edges != NULL;
    return entry->drawable ? &entry->payload : NULL;
}

static void draw_cross(SDL_Renderer *renderer, int x, int y, int radius, SDL_Color color) {
    if (!renderer || radius <= 0) return;
    draw_line(renderer, x - radius, y, x + radius, y, color);
    draw_line(renderer, x, y - radius, x, y + radius, color);
}

static void draw_origin_axes(SDL_Renderer *renderer,
                             const SceneEditorViewportState *viewport,
                             int window_w,
                             int window_h) {
    CoreObjectVec3 origin = {0};
    CoreObjectVec3 axis_x = {1.4, 0.0, 0.0};
    CoreObjectVec3 axis_y = {0.0, 1.4, 0.0};
    CoreObjectVec3 axis_z = {0.0, 0.0, 1.4};
    int ox = 0;
    int oy = 0;
    draw_segment(renderer, viewport, window_w, window_h, origin, axis_x, COLOR_AXIS_X);
    draw_segment(renderer, viewport, window_w, window_h, origin, axis_y, COLOR_AXIS_Y);
    draw_segment(renderer, viewport, window_w, window_h, origin, axis_z, COLOR_AXIS_Z);
    project_point(viewport, window_w, window_h, origin, &ox, &oy);
    draw_circle(renderer, ox, oy, 3, COLOR_ORIGIN);
}

static void draw_domain_box(SDL_Renderer *renderer,
                            const SceneEditorViewportState *viewport,
                            int window_w,
                            int window_h,
                            const PhysicsSimRuntimeVisualBootstrap *visual) {
    CoreObjectVec3 corners[8];
    static const int edges[12][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0},
        {4, 5}, {5, 7}, {7, 6}, {6, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    SDL_Color edge_color;
    int index = 0;
    if (!renderer || !viewport || !visual || !visual->scene_domain.enabled) return;
    edge_color = visual->scene_domain_authored ? COLOR_DOMAIN_AUTHORED : COLOR_DOMAIN_DERIVED;
    for (int sx = 0; sx <= 1; ++sx) {
        for (int sy = 0; sy <= 1; ++sy) {
            for (int sz = 0; sz <= 1; ++sz) {
                corners[index++] = (CoreObjectVec3){
                    sx ? visual->scene_domain.max.x : visual->scene_domain.min.x,
                    sy ? visual->scene_domain.max.y : visual->scene_domain.min.y,
                    sz ? visual->scene_domain.max.z : visual->scene_domain.min.z
                };
            }
        }
    }
    for (int i = 0; i < 12; ++i) {
        draw_segment(renderer,
                     viewport,
                     window_w,
                     window_h,
                     corners[edges[i][0]],
                     corners[edges[i][1]],
                     edge_color);
    }
}

static void draw_slice_plane_outline(SDL_Renderer *renderer,
                                     const SceneEditorViewportState *viewport,
                                     int window_w,
                                     int window_h,
                                     CoreObjectVec3 min,
                                     CoreObjectVec3 max,
                                     float slice_z,
                                     SDL_Color color,
                                     bool draw_interior) {
    CoreObjectVec3 corners[4];
    CoreObjectVec3 center_a;
    CoreObjectVec3 center_b;
    corners[0] = (CoreObjectVec3){min.x, min.y, slice_z};
    corners[1] = (CoreObjectVec3){max.x, min.y, slice_z};
    corners[2] = (CoreObjectVec3){max.x, max.y, slice_z};
    corners[3] = (CoreObjectVec3){min.x, max.y, slice_z};
    center_a = (CoreObjectVec3){min.x, (min.y + max.y) * 0.5, slice_z};
    center_b = (CoreObjectVec3){max.x, (min.y + max.y) * 0.5, slice_z};

    draw_segment(renderer, viewport, window_w, window_h, corners[0], corners[1], color);
    draw_segment(renderer, viewport, window_w, window_h, corners[1], corners[2], color);
    draw_segment(renderer, viewport, window_w, window_h, corners[2], corners[3], color);
    draw_segment(renderer, viewport, window_w, window_h, corners[3], corners[0], color);
    if (draw_interior) {
        draw_segment(renderer, viewport, window_w, window_h, corners[0], corners[2], color);
        draw_segment(renderer, viewport, window_w, window_h, corners[1], corners[3], color);
        draw_segment(renderer, viewport, window_w, window_h, center_a, center_b, color);
    }
}

static SDL_Color ghost_slice_color(bool has_fluid, bool has_obstacles, int distance) {
    SDL_Color color = has_fluid ? COLOR_FLUID_LOW : COLOR_SLICE_PROJECTION;
    Uint8 alpha = has_fluid ? 92 : 38;
    if (has_fluid && has_obstacles) {
        color = lighten_color(COLOR_SLICE_PROJECTION, 0.10f);
        alpha = 108;
    } else if (has_obstacles) {
        color = COLOR_SLICE_PROJECTION;
    }
    if (distance > 1) {
        int faded = (int)alpha - (distance - 1) * 20;
        if (faded < 20) faded = 20;
        alpha = (Uint8)faded;
    }
    color.a = alpha;
    return color;
}

static void draw_slice_stack_preview(SDL_Renderer *renderer,
                                     const SceneEditorViewportState *viewport,
                                     int window_w,
                                     int window_h,
                                     const SceneState *scene,
                                     CoreObjectVec3 min,
                                     CoreObjectVec3 max) {
    SimRuntimeBackendReport report = {0};
    if (!renderer || !viewport || !scene) return;
    if (!scene_backend_report(scene, &report)) return;
    if (!report.secondary_debug_slice_stack_live ||
        !report.compatibility_view_2d_derived ||
        report.domain_d <= 1 ||
        report.secondary_debug_slice_stack_radius <= 0) {
        return;
    }

    for (int distance = report.secondary_debug_slice_stack_radius; distance >= 1; --distance) {
        for (int direction = -1; direction <= 1; direction += 2) {
            int preview_z = report.compatibility_slice_z + direction * distance;
            bool has_fluid = false;
            bool has_obstacles = false;
            SDL_Color color;
            double world_z = 0.0;
            if (!scene_backend_compatibility_slice_activity(
                    scene, preview_z, &has_fluid, &has_obstacles)) {
                continue;
            }
            if (!has_fluid && !has_obstacles) continue;
            world_z = retained_runtime_overlay_slice_world_z_for_index(scene, min, max, preview_z);
            color = ghost_slice_color(has_fluid, has_obstacles, distance);
            draw_slice_plane_outline(renderer,
                                     viewport,
                                     window_w,
                                     window_h,
                                     min,
                                     max,
                                     (float)world_z,
                                     color,
                                     false);
        }
    }
}

static void draw_slice_plane(SDL_Renderer *renderer,
                             const SceneEditorViewportState *viewport,
                             int window_w,
                             int window_h,
                             const SceneState *scene) {
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    float slice_z = 0.0f;
    if (!renderer || !viewport || !scene) return;
    if (!retained_runtime_overlay_compute_visual_bounds(scene, &min, &max)) return;
    slice_z = (float)retained_runtime_overlay_slice_z(scene, min, max);
    draw_slice_plane_outline(renderer,
                             viewport,
                             window_w,
                             window_h,
                             min,
                             max,
                             slice_z,
                             COLOR_SLICE_PLANE,
                             true);
}

static void draw_fluid_slice(SDL_Renderer *renderer,
                             const SceneState *scene,
                             const SceneEditorViewportState *viewport,
                             int window_w,
                             int window_h) {
    SceneFluidFieldView2D fluid = {0};
    SceneObstacleFieldView2D obstacles = {0};
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    float span_x = 1.0f;
    float span_y = 1.0f;
    float slice_z = 0.0f;
    int stride = 1;
    int solid_stride = 1;
    bool have_aligned_obstacles = false;
    if (!renderer || !scene || !viewport) return;
    if (!scene_backend_fluid_view_2d(scene, &fluid)) return;
    if (fluid.width <= 0 || fluid.height <= 0) return;
    if (!retained_runtime_overlay_compute_visual_bounds(scene, &min, &max)) return;
    have_aligned_obstacles =
        scene_backend_obstacle_view_2d(scene, &obstacles) &&
        obstacles.solid_mask &&
        obstacles.width == fluid.width &&
        obstacles.height == fluid.height &&
        obstacles.cell_count >= fluid.cell_count;

    span_x = (float)(max.x - min.x);
    span_y = (float)(max.y - min.y);
    if (fabsf(span_x) < 0.0001f) span_x = 1.0f;
    if (fabsf(span_y) < 0.0001f) span_y = 1.0f;
    slice_z = (float)retained_runtime_overlay_slice_z(scene, min, max);
    stride = fluid.width > 160 || fluid.height > 160 ? 3 : 2;
    if (fluid.width <= 48 && fluid.height <= 48) stride = 1;
    solid_stride = stride > 2 ? 2 : stride;

    if (have_aligned_obstacles) {
        for (int y = 0; y < obstacles.height; y += solid_stride) {
            for (int x = 0; x < obstacles.width; x += solid_stride) {
                size_t idx = (size_t)y * (size_t)obstacles.width + (size_t)x;
                float u = 0.5f;
                float v = 0.5f;
                CoreObjectVec3 point = {0};
                int screen_x = 0;
                int screen_y = 0;
                if (idx >= obstacles.cell_count || !obstacles.solid_mask[idx]) continue;
                u = (obstacles.width > 1) ? ((float)x / (float)(obstacles.width - 1)) : 0.5f;
                v = (obstacles.height > 1) ? ((float)y / (float)(obstacles.height - 1)) : 0.5f;
                point.x = min.x + (double)(u * span_x);
                point.y = min.y + (double)(v * span_y);
                point.z = slice_z;
                project_point(viewport, window_w, window_h, point, &screen_x, &screen_y);
                draw_square(renderer, screen_x, screen_y, solid_stride > 1 ? 1 : 0, COLOR_SOLID_SLICE);
            }
        }
    }

    for (int y = 0; y < fluid.height; y += stride) {
        for (int x = 0; x < fluid.width; x += stride) {
            size_t idx = (size_t)y * (size_t)fluid.width + (size_t)x;
            float density = fluid.density[idx] * 0.05f;
            if (have_aligned_obstacles && idx < obstacles.cell_count && obstacles.solid_mask[idx]) {
                continue;
            }
            if (density <= 0.035f) continue;
            density = overlay_clampf(density, 0.0f, 1.0f);
            float u = (fluid.width > 1) ? ((float)x / (float)(fluid.width - 1)) : 0.5f;
            float v = (fluid.height > 1) ? ((float)y / (float)(fluid.height - 1)) : 0.5f;
            CoreObjectVec3 point = {
                .x = min.x + (double)(u * span_x),
                .y = min.y + (double)(v * span_y),
                .z = slice_z
            };
            SDL_Color color = lerp_color(COLOR_FLUID_LOW, COLOR_FLUID_HIGH, density);
            int screen_x = 0;
            int screen_y = 0;
            int radius = density > 0.35f ? 2 : 1;
            project_point(viewport, window_w, window_h, point, &screen_x, &screen_y);
            draw_circle(renderer, screen_x, screen_y, radius, color);
        }
    }
}

static SDL_Color object_color(const SceneState *scene, int object_index) {
    SDL_Color color = COLOR_DYNAMIC;
    if (scene && scene->preset && object_index >= 0 && object_index < (int)scene->preset->object_count) {
        if (scene->preset->objects[object_index].is_static) {
            color = COLOR_STATIC;
        }
    }
    return color;
}

static SDL_Color emitter_color(const FluidEmitter *emitter) {
    if (!emitter) return COLOR_SOURCE;
    switch (emitter->type) {
    case EMITTER_VELOCITY_JET:
        return COLOR_JET;
    case EMITTER_SINK:
        return COLOR_SINK;
    case EMITTER_DENSITY_SOURCE:
    default:
        return COLOR_SOURCE;
    }
}

static void draw_emitter_projection_guides(SDL_Renderer *renderer,
                                           const SceneEditorViewportState *viewport,
                                           int window_w,
                                           int window_h,
                                           const SceneState *scene,
                                           bool draw_slice_debug) {
    if (!renderer || !viewport || !scene || !scene->preset) return;
    for (size_t i = 0; i < scene->preset->emitter_count && i < MAX_FLUID_EMITTERS; ++i) {
        const FluidEmitter *emitter = &scene->preset->emitters[i];
        CoreObjectVec3 actual = {0};
        CoreObjectVec3 slice = {0};
        SDL_Color color = emitter_color(emitter);
        int actual_x = 0;
        int actual_y = 0;
        int slice_x = 0;
        int slice_y = 0;
        if (!retained_runtime_overlay_emitter_actual_and_slice_points(
                scene, emitter, &actual, &slice)) {
            continue;
        }
        project_point(viewport, window_w, window_h, actual, &actual_x, &actual_y);
        if (draw_slice_debug) {
            project_point(viewport, window_w, window_h, slice, &slice_x, &slice_y);
            if (fabs(actual.z - slice.z) > 0.001) {
                draw_segment(renderer, viewport, window_w, window_h, actual, slice, COLOR_SLICE_PROJECTION);
            }
            draw_cross(renderer, slice_x, slice_y, 4, lighten_color(color, 0.22f));
        }
        draw_circle(renderer, actual_x, actual_y, 3, color);
    }
}

static void draw_plane(SDL_Renderer *renderer,
                       const SceneEditorViewportState *viewport,
                       int window_w,
                       int window_h,
                       const CoreScenePlanePrimitive *plane,
                       SDL_Color color) {
    CoreObjectVec3 corners[4];
    retained_runtime_overlay_fill_plane_corners(plane, corners);
    draw_segment(renderer, viewport, window_w, window_h, corners[0], corners[1], color);
    draw_segment(renderer, viewport, window_w, window_h, corners[1], corners[2], color);
    draw_segment(renderer, viewport, window_w, window_h, corners[2], corners[3], color);
    draw_segment(renderer, viewport, window_w, window_h, corners[3], corners[0], color);
}

static void draw_prism(SDL_Renderer *renderer,
                       const SceneEditorViewportState *viewport,
                       int window_w,
                       int window_h,
                       const CoreSceneRectPrismPrimitive *prism,
                       SDL_Color color) {
    CoreObjectVec3 corners[8];
    static const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    retained_runtime_overlay_fill_prism_corners(prism, corners);
    for (int i = 0; i < 12; ++i) {
        draw_segment(renderer,
                     viewport,
                     window_w,
                     window_h,
                     corners[edges[i][0]],
                     corners[edges[i][1]],
                     color);
    }
}

static void draw_aabb(SDL_Renderer *renderer,
                      const SceneEditorViewportState *viewport,
                      int window_w,
                      int window_h,
                      CoreObjectVec3 min,
                      CoreObjectVec3 max,
                      SDL_Color color) {
    CoreObjectVec3 corners[8];
    static const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    retained_runtime_overlay_fill_aabb_corners(min, max, corners);
    for (int i = 0; i < 12; ++i) {
        draw_segment(renderer,
                     viewport,
                     window_w,
                     window_h,
                     corners[edges[i][0]],
                     corners[edges[i][1]],
                     color);
    }
}

static bool draw_mesh_preview_edges(SDL_Renderer *renderer,
                                    const SceneEditorViewportState *viewport,
                                    int window_w,
                                    int window_h,
                                    const PhysicsSimRuntimeMeshPreviewInstance *preview,
                                    SDL_Color color) {
    const CoreMeshPreviewRuntimePayload *payload = NULL;
    if (!renderer || !viewport || !preview) return false;
    payload = mesh_preview_edges_for_instance(preview);
    if (!payload || !payload->edges || payload->edge_count == 0u) return false;
    if (payload->coverage_ratio > 0.0 && payload->coverage_ratio < 0.5) {
        color.a = (Uint8)((int)color.a > 190 ? 190 : color.a);
    }
    for (size_t edge_i = 0; edge_i < payload->edge_count; ++edge_i) {
        CoreObjectVec3 a = transform_mesh_preview_point(payload->edges[edge_i].a, preview);
        CoreObjectVec3 b = transform_mesh_preview_point(payload->edges[edge_i].b, preview);
        draw_segment(renderer, viewport, window_w, window_h, a, b, color);
    }
    return true;
}

static void draw_mesh_previews(SDL_Renderer *renderer,
                               const SceneEditorViewportState *viewport,
                               int window_w,
                               int window_h,
                               const PhysicsSimRuntimeMeshPreviewSet *previews,
                               bool slice_debug_enabled,
                               double slice_z) {
    if (!renderer || !viewport || !previews || !previews->valid_contract) return;
    for (int i = 0; i < previews->instance_count; ++i) {
        const PhysicsSimRuntimeMeshPreviewInstance *preview = &previews->instances[i];
        SDL_Color edge_color = COLOR_MESH_PREVIEW;
        SDL_Color bounds_color = COLOR_MESH_PREVIEW;
        bool slice_intersects = false;
        bool drew_edges = false;
        if (!preview->has_world_bounds) continue;
        if (slice_debug_enabled) {
            slice_intersects =
                slice_z >= preview->world_bounds_min.z &&
                slice_z <= preview->world_bounds_max.z;
            if (slice_intersects) {
                edge_color = lighten_color(edge_color, 0.18f);
                edge_color.a = 255;
                bounds_color.a = 150;
            } else {
                edge_color.a = 126;
                bounds_color.a = 76;
            }
        } else {
            edge_color.a = 238;
            bounds_color.a = 120;
        }
        drew_edges = draw_mesh_preview_edges(renderer,
                                             viewport,
                                             window_w,
                                             window_h,
                                             preview,
                                             edge_color);
        if (!drew_edges || slice_intersects) {
            draw_aabb(renderer,
                      viewport,
                      window_w,
                      window_h,
                      preview->world_bounds_min,
                      preview->world_bounds_max,
                      bounds_color);
        }
    }
}

bool retained_runtime_scene_overlay_active(const SceneState *scene) {
    return scene &&
           scene->runtime_visual.valid &&
           scene->runtime_visual.retained_scene.valid_contract &&
           scene->mode_route.requested_space_mode == SPACE_MODE_3D;
}

bool retained_runtime_scene_overlay_slice_debug_enabled(const SceneState *scene) {
    return retained_runtime_scene_overlay_active(scene) &&
           scene_runtime_slice_overlay_enabled(scene);
}

bool retained_runtime_scene_overlay_frame_view(SceneState *scene,
                                               int window_w,
                                               int window_h) {
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    bool have_bounds = false;
    if (!retained_runtime_scene_overlay_active(scene)) return false;

    scene_editor_viewport_init(&scene->runtime_view.viewport, SPACE_MODE_3D, SPACE_MODE_3D);
    if (scene->runtime_visual.scene_domain.enabled) {
        min = scene->runtime_visual.scene_domain.min;
        max = scene->runtime_visual.scene_domain.max;
        have_bounds = true;
    } else {
        have_bounds = retained_runtime_overlay_compute_visual_bounds(scene, &min, &max);
    }
    if (have_bounds) {
        scene_editor_viewport_frame_bounds(&scene->runtime_view.viewport,
                                           window_w,
                                           window_h,
                                           (float)min.x,
                                           (float)min.y,
                                           (float)min.z,
                                           (float)max.x,
                                           (float)max.y,
                                           (float)max.z);
    }
    return have_bounds;
}

void retained_runtime_scene_overlay_draw(const SceneState *scene,
                                         SDL_Renderer *renderer,
                                         int window_w,
                                         int window_h) {
    CoreObjectVec3 visual_min = {0};
    CoreObjectVec3 visual_max = {0};
    double slice_z = 0.0;
    bool slice_debug_enabled = false;
    if (!retained_runtime_scene_overlay_active(scene) || !renderer) return;
    if (!retained_runtime_overlay_compute_visual_bounds(scene, &visual_min, &visual_max)) return;
    slice_debug_enabled = retained_runtime_scene_overlay_slice_debug_enabled(scene);
    if (slice_debug_enabled) {
        slice_z = retained_runtime_overlay_slice_z(scene, visual_min, visual_max);
        draw_slice_stack_preview(renderer,
                                 &scene->runtime_view.viewport,
                                 window_w,
                                 window_h,
                                 scene,
                                 visual_min,
                                 visual_max);
        draw_slice_plane(renderer, &scene->runtime_view.viewport, window_w, window_h, scene);
    }
    retained_runtime_overlay_draw_volume_readout(renderer,
                                                 &scene->runtime_view.viewport,
                                                 window_w,
                                                 window_h,
                                                 scene);
    if (slice_debug_enabled) {
        draw_fluid_slice(renderer, scene, &scene->runtime_view.viewport, window_w, window_h);
    }
    draw_origin_axes(renderer, &scene->runtime_view.viewport, window_w, window_h);
    draw_domain_box(renderer, &scene->runtime_view.viewport, window_w, window_h, &scene->runtime_visual);
    draw_mesh_previews(renderer,
                       &scene->runtime_view.viewport,
                       window_w,
                       window_h,
                       &scene->runtime_visual.mesh_previews,
                       slice_debug_enabled,
                       slice_z);
    for (int i = 0; i < scene->runtime_visual.retained_scene.retained_object_count; ++i) {
        const CoreSceneObjectContract *object = &scene->runtime_visual.retained_scene.objects[i];
        SDL_Color color = object_color(scene, i);
        CoreObjectVec3 origin = sim_runtime_3d_anchor_retained_object_origin(object);
        bool slice_intersects = slice_debug_enabled &&
                                retained_runtime_overlay_object_slice_intersects(scene, object, slice_z);
        int origin_x = 0;
        int origin_y = 0;
        if (slice_debug_enabled && slice_intersects) {
            color = lighten_color(color, 0.08f);
            color.a = 255;
        } else if (slice_debug_enabled) {
            color.a = 112;
        } else {
            color.a = 224;
        }
        if (object->has_plane_primitive) {
            draw_plane(renderer, &scene->runtime_view.viewport, window_w, window_h, &object->plane_primitive, color);
        } else if (object->has_rect_prism_primitive) {
            draw_prism(renderer, &scene->runtime_view.viewport, window_w, window_h, &object->rect_prism_primitive, color);
        } else {
            int x = 0;
            int y = 0;
            project_point(&scene->runtime_view.viewport, window_w, window_h, object->object.transform.position, &x, &y);
            draw_circle(renderer, x, y, 4, color);
        }
        project_point(&scene->runtime_view.viewport, window_w, window_h, origin, &origin_x, &origin_y);
        draw_circle(renderer,
                    origin_x,
                    origin_y,
                    slice_intersects ? 3 : 2,
                    lighten_color(color, slice_intersects ? 0.22f : 0.08f));
    }
    draw_emitter_projection_guides(renderer,
                                   &scene->runtime_view.viewport,
                                   window_w,
                                   window_h,
                                   scene,
                                   slice_debug_enabled);
}
