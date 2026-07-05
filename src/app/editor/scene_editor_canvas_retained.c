#include "app/editor/scene_editor_canvas_retained.h"

#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_wind_setup.h"
#include "render/retained_runtime_scene_overlay_geom.h"

#include <math.h>
#include <stddef.h>

static void retained_draw_circle(SDL_Renderer *renderer,
                                 int cx,
                                 int cy,
                                 int radius,
                                 SDL_Color color) {
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

static void retained_draw_line(SDL_Renderer *renderer,
                               int x0,
                               int y0,
                               int x1,
                               int y1,
                               SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
}

static void retained_draw_thick_line(SDL_Renderer *renderer,
                                     int x0,
                                     int y0,
                                     int x1,
                                     int y1,
                                     SDL_Color color) {
    retained_draw_line(renderer, x0, y0, x1, y1, color);
    retained_draw_line(renderer, x0 + 1, y0, x1 + 1, y1, color);
    retained_draw_line(renderer, x0, y0 + 1, x1, y1 + 1, color);
}

static SDL_Rect retained_viewport_rect(const SceneEditorState *state) {
    SDL_Rect rect = {0};
    if (!state) return rect;
    rect = editor_active_viewport_rect(state);
    if (rect.w <= 0 || rect.h <= 0) {
        rect.x = state->canvas_x;
        rect.y = state->canvas_y;
        rect.w = state->canvas_width;
        rect.h = state->canvas_height;
    }
    return rect;
}

static CoreObjectVec3 vec3_add_scaled(CoreObjectVec3 base,
                                      CoreObjectVec3 axis,
                                      double scale) {
    CoreObjectVec3 result = base;
    result.x += axis.x * scale;
    result.y += axis.y * scale;
    result.z += axis.z * scale;
    return result;
}

static void draw_retained_segment(SDL_Renderer *renderer,
                                  const SceneEditorState *state,
                                  CoreObjectVec3 a,
                                  CoreObjectVec3 b,
                                  SDL_Color color) {
    SDL_Rect rect = {0};
    int ax = 0;
    int ay = 0;
    int bx = 0;
    int by = 0;
    if (!renderer || !state) return;
    rect = retained_viewport_rect(state);
    scene_editor_viewport_project_point3(&state->viewport,
                                         rect.x,
                                         rect.y,
                                         rect.w,
                                         rect.h,
                                         (float)a.x,
                                         (float)a.y,
                                         (float)a.z,
                                         &ax,
                                         &ay);
    scene_editor_viewport_project_point3(&state->viewport,
                                         rect.x,
                                         rect.y,
                                         rect.w,
                                         rect.h,
                                         (float)b.x,
                                         (float)b.y,
                                         (float)b.z,
                                         &bx,
                                         &by);
    retained_draw_line(renderer, ax, ay, bx, by, color);
}

void scene_editor_canvas_draw_retained_origin_axes(SDL_Renderer *renderer,
                                                   const SceneEditorState *state) {
    CoreObjectVec3 origin = {0};
    CoreObjectVec3 axis_x = {0};
    CoreObjectVec3 axis_y = {0};
    CoreObjectVec3 axis_z = {0};
    float scene_dx = 0.0f;
    float scene_dy = 0.0f;
    float scene_dz = 0.0f;
    float scene_span = 0.0f;
    double axis_length = 1.0;
    SDL_Color x_color = {232, 84, 79, 255};
    SDL_Color y_color = {92, 194, 108, 255};
    SDL_Color z_color = {84, 156, 255, 255};
    SDL_Color origin_color = {236, 240, 245, 210};
    SDL_Rect rect = {0};
    int ox = 0;
    int oy = 0;
    if (!renderer || !state) return;

    if (state->viewport.has_scene_bounds) {
        scene_dx = fabsf(state->viewport.scene_max_x - state->viewport.scene_min_x);
        scene_dy = fabsf(state->viewport.scene_max_y - state->viewport.scene_min_y);
        scene_dz = fabsf(state->viewport.scene_max_z - state->viewport.scene_min_z);
        scene_span = fmaxf(scene_dx, fmaxf(scene_dy, scene_dz));
    }
    if (scene_span > 0.001f) {
        axis_length = (double)scene_span * 0.18;
    }
    if (axis_length < 0.75) axis_length = 0.75;
    if (axis_length > 4.0) axis_length = 4.0;

    axis_x.x = axis_length;
    axis_y.y = axis_length;
    axis_z.z = axis_length;

    draw_retained_segment(renderer, state, origin, axis_x, x_color);
    draw_retained_segment(renderer, state, origin, axis_y, y_color);
    draw_retained_segment(renderer, state, origin, axis_z, z_color);

    rect = retained_viewport_rect(state);
    scene_editor_viewport_project_point3(&state->viewport,
                                         rect.x,
                                         rect.y,
                                         rect.w,
                                         rect.h,
                                         0.0f,
                                         0.0f,
                                         0.0f,
                                         &ox,
                                         &oy);
    retained_draw_circle(renderer, ox, oy, 3, origin_color);
}

void scene_editor_canvas_draw_retained_domain_box(SDL_Renderer *renderer,
                                                  const SceneEditorState *state,
                                                  const PhysicsSimDomainOverlay *domain) {
    CoreObjectVec3 corners[8];
    SDL_Color edge_color = {132, 164, 188, 170};
    static const int edges[12][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0},
        {4, 5}, {5, 7}, {7, 6}, {6, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    int index = 0;
    if (!renderer || !state || !domain || !domain->active) return;
    if (!domain->seeded_from_retained_bounds) {
        edge_color = (SDL_Color){138, 198, 154, 192};
    }
    for (int sx = 0; sx <= 1; ++sx) {
        for (int sy = 0; sy <= 1; ++sy) {
            for (int sz = 0; sz <= 1; ++sz) {
                corners[index++] = (CoreObjectVec3){
                    sx ? domain->max.x : domain->min.x,
                    sy ? domain->max.y : domain->min.y,
                    sz ? domain->max.z : domain->min.z
                };
            }
        }
    }
    for (int i = 0; i < 12; ++i) {
        draw_retained_segment(renderer,
                              state,
                              corners[edges[i][0]],
                              corners[edges[i][1]],
                              edge_color);
    }
}

static void retained_domain_box_corners(const PhysicsSimDomainOverlay *domain,
                                        CoreObjectVec3 corners[8]) {
    int index = 0;
    if (!domain || !corners) return;
    for (int sx = 0; sx <= 1; ++sx) {
        for (int sy = 0; sy <= 1; ++sy) {
            for (int sz = 0; sz <= 1; ++sz) {
                corners[index++] = (CoreObjectVec3){
                    sx ? domain->max.x : domain->min.x,
                    sy ? domain->max.y : domain->min.y,
                    sz ? domain->max.z : domain->min.z
                };
            }
        }
    }
}

static bool retained_domain_face_indices(WindTunnel3DFace face, int out_indices[4]) {
    static const int left[4] = {0, 1, 3, 2};
    static const int right[4] = {4, 6, 7, 5};
    static const int top[4] = {2, 3, 7, 6};
    static const int bottom[4] = {0, 4, 5, 1};
    static const int front[4] = {0, 2, 6, 4};
    static const int back[4] = {1, 5, 7, 3};
    const int *src = NULL;
    if (!out_indices) return false;
    switch (face) {
        case WIND_TUNNEL_3D_FACE_LEFT: src = left; break;
        case WIND_TUNNEL_3D_FACE_RIGHT: src = right; break;
        case WIND_TUNNEL_3D_FACE_TOP: src = top; break;
        case WIND_TUNNEL_3D_FACE_BOTTOM: src = bottom; break;
        case WIND_TUNNEL_3D_FACE_FRONT: src = front; break;
        case WIND_TUNNEL_3D_FACE_BACK: src = back; break;
        case WIND_TUNNEL_3D_FACE_NONE:
        default: return false;
    }
    for (int i = 0; i < 4; ++i) {
        out_indices[i] = src[i];
    }
    return true;
}

static void retained_draw_domain_face(SDL_Renderer *renderer,
                                      const SceneEditorState *state,
                                      const CoreObjectVec3 corners[8],
                                      WindTunnel3DFace face,
                                      SDL_Color color) {
    SDL_Rect rect = {0};
    int indices[4] = {0};
    SDL_Point points[5];
    if (!renderer || !state || !corners) return;
    if (!retained_domain_face_indices(face, indices)) return;
    rect = retained_viewport_rect(state);
    for (int i = 0; i < 4; ++i) {
        scene_editor_viewport_project_point3(&state->viewport,
                                             rect.x,
                                             rect.y,
                                             rect.w,
                                             rect.h,
                                             (float)corners[indices[i]].x,
                                             (float)corners[indices[i]].y,
                                             (float)corners[indices[i]].z,
                                             &points[i].x,
                                             &points[i].y);
    }
    points[4] = points[0];
    for (int i = 1; i < 5; ++i) {
        retained_draw_thick_line(renderer,
                                 points[i - 1].x,
                                 points[i - 1].y,
                                 points[i].x,
                                 points[i].y,
                                 color);
    }
}

static bool retained_project_domain_face(const SceneEditorState *state,
                                         const CoreObjectVec3 corners[8],
                                         WindTunnel3DFace face,
                                         SDL_Point out_points[4]) {
    SDL_Rect rect = {0};
    int indices[4] = {0};
    if (!state || !corners || !out_points) return false;
    if (!retained_domain_face_indices(face, indices)) return false;
    rect = retained_viewport_rect(state);
    for (int i = 0; i < 4; ++i) {
        scene_editor_viewport_project_point3(&state->viewport,
                                             rect.x,
                                             rect.y,
                                             rect.w,
                                             rect.h,
                                             (float)corners[indices[i]].x,
                                             (float)corners[indices[i]].y,
                                             (float)corners[indices[i]].z,
                                             &out_points[i].x,
                                             &out_points[i].y);
    }
    return true;
}

static bool retained_point_in_projected_quad(const SDL_Point points[4], int x, int y) {
    bool inside = false;
    if (!points) return false;
    for (int i = 0, j = 3; i < 4; j = i++) {
        const int yi = points[i].y;
        const int yj = points[j].y;
        const int xi = points[i].x;
        const int xj = points[j].x;
        if (((yi > y) != (yj > y)) &&
            (x < (xj - xi) * (y - yi) / (double)(yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

static double retained_projected_quad_centroid_distance_sq(const SDL_Point points[4], int x, int y) {
    double cx = 0.0;
    double cy = 0.0;
    if (!points) return 0.0;
    for (int i = 0; i < 4; ++i) {
        cx += (double)points[i].x;
        cy += (double)points[i].y;
    }
    cx *= 0.25;
    cy *= 0.25;
    return (cx - (double)x) * (cx - (double)x) + (cy - (double)y) * (cy - (double)y);
}

bool scene_editor_canvas_retained_wind_face_at(const SceneEditorState *state,
                                               const PhysicsSimDomainOverlay *domain,
                                               int x,
                                               int y,
                                               WindTunnel3DFace *out_face) {
    static const WindTunnel3DFace faces[] = {
        WIND_TUNNEL_3D_FACE_LEFT,
        WIND_TUNNEL_3D_FACE_RIGHT,
        WIND_TUNNEL_3D_FACE_TOP,
        WIND_TUNNEL_3D_FACE_BOTTOM,
        WIND_TUNNEL_3D_FACE_FRONT,
        WIND_TUNNEL_3D_FACE_BACK
    };
    CoreObjectVec3 corners[8];
    bool found = false;
    double best_distance_sq = 0.0;
    WindTunnel3DFace best_face = WIND_TUNNEL_3D_FACE_NONE;
    if (out_face) *out_face = WIND_TUNNEL_3D_FACE_NONE;
    if (!state || !domain || !domain->active) return false;
    retained_domain_box_corners(domain, corners);
    for (size_t i = 0; i < sizeof(faces) / sizeof(faces[0]); ++i) {
        SDL_Point points[4];
        double distance_sq = 0.0;
        if (!retained_project_domain_face(state, corners, faces[i], points)) continue;
        if (!retained_point_in_projected_quad(points, x, y)) continue;
        distance_sq = retained_projected_quad_centroid_distance_sq(points, x, y);
        if (!found || distance_sq < best_distance_sq) {
            found = true;
            best_distance_sq = distance_sq;
            best_face = faces[i];
        }
    }
    if (!found) return false;
    if (out_face) *out_face = best_face;
    return true;
}

void scene_editor_canvas_draw_retained_wind_faces(SDL_Renderer *renderer,
                                                  const SceneEditorState *state,
                                                  const PhysicsSimDomainOverlay *domain) {
    SceneEditorWindSetupSummary wind_setup;
    CoreObjectVec3 corners[8];
    SDL_Color inlet_color = {88, 218, 126, 255};
    SDL_Color outlet_color = {235, 84, 86, 255};
    WindTunnel3DFace inlet_face = WIND_TUNNEL_3D_FACE_NONE;
    WindTunnel3DFace outlet_face = WIND_TUNNEL_3D_FACE_NONE;
    if (!renderer || !state || !domain || !domain->active) return;
    wind_setup = scene_editor_wind_setup_summary(&state->cfg, &state->session);
    if (!wind_setup.active) return;
    inlet_face = wind_setup.config.inlet_face;
    outlet_face = wind_setup.config.outlet_face;
    if (state->wind_face_hover != WIND_TUNNEL_3D_FACE_NONE) {
        WindTunnel3DFace preview_outlet = scene_editor_wind_setup_opposite_face(state->wind_face_hover);
        if (preview_outlet != WIND_TUNNEL_3D_FACE_NONE) {
            inlet_face = state->wind_face_hover;
            outlet_face = preview_outlet;
        }
    }
    retained_domain_box_corners(domain, corners);
    retained_draw_domain_face(renderer, state, corners, inlet_face, inlet_color);
    retained_draw_domain_face(renderer, state, corners, outlet_face, outlet_color);
}

static void draw_retained_plane(SDL_Renderer *renderer,
                                const SceneEditorState *state,
                                const CoreScenePlanePrimitive *plane,
                                SDL_Color color) {
    CoreObjectVec3 corners[4];
    CoreObjectVec3 origin = {0};
    CoreObjectVec3 u_plus = {0};
    CoreObjectVec3 u_minus = {0};
    double half_width = plane->width * 0.5;
    double half_height = plane->height * 0.5;
    if (!renderer || !state || !plane) return;

    origin = plane->frame.origin;
    u_plus = vec3_add_scaled(origin, plane->frame.axis_u, half_width);
    u_minus = vec3_add_scaled(origin, plane->frame.axis_u, -half_width);
    corners[0] = vec3_add_scaled(u_minus, plane->frame.axis_v, -half_height);
    corners[1] = vec3_add_scaled(u_plus, plane->frame.axis_v, -half_height);
    corners[2] = vec3_add_scaled(u_plus, plane->frame.axis_v, half_height);
    corners[3] = vec3_add_scaled(u_minus, plane->frame.axis_v, half_height);

    draw_retained_segment(renderer, state, corners[0], corners[1], color);
    draw_retained_segment(renderer, state, corners[1], corners[2], color);
    draw_retained_segment(renderer, state, corners[2], corners[3], color);
    draw_retained_segment(renderer, state, corners[3], corners[0], color);
}

static void draw_retained_prism(SDL_Renderer *renderer,
                                const SceneEditorState *state,
                                const CoreSceneRectPrismPrimitive *prism,
                                SDL_Color color) {
    CoreObjectVec3 corners[8];
    CoreObjectVec3 base = prism->frame.origin;
    double half_width = 0.0;
    double half_height = 0.0;
    double half_depth = 0.0;
    static const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    int i = 0;
    if (!renderer || !state || !prism) return;

    half_width = prism->width * 0.5;
    half_height = prism->height * 0.5;
    half_depth = prism->depth * 0.5;

    corners[0] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, -half_width), prism->frame.axis_v, -half_height), prism->frame.normal, -half_depth);
    corners[1] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, half_width), prism->frame.axis_v, -half_height), prism->frame.normal, -half_depth);
    corners[2] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, half_width), prism->frame.axis_v, half_height), prism->frame.normal, -half_depth);
    corners[3] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, -half_width), prism->frame.axis_v, half_height), prism->frame.normal, -half_depth);
    corners[4] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, -half_width), prism->frame.axis_v, -half_height), prism->frame.normal, half_depth);
    corners[5] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, half_width), prism->frame.axis_v, -half_height), prism->frame.normal, half_depth);
    corners[6] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, half_width), prism->frame.axis_v, half_height), prism->frame.normal, half_depth);
    corners[7] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, -half_width), prism->frame.axis_v, half_height), prism->frame.normal, half_depth);

    for (i = 0; i < 12; ++i) {
        draw_retained_segment(renderer, state, corners[edges[i][0]], corners[edges[i][1]], color);
    }
}

static void draw_retained_aabb(SDL_Renderer *renderer,
                               const SceneEditorState *state,
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
        draw_retained_segment(renderer, state, corners[edges[i][0]], corners[edges[i][1]], color);
    }
}

void scene_editor_canvas_draw_retained_mesh_previews(SDL_Renderer *renderer,
                                                     const SceneEditorState *state,
                                                     const PhysicsSimRuntimeMeshPreviewSet *previews) {
    SDL_Color color = {188, 128, 255, 214};
    if (!renderer || !state || !previews || !previews->valid_contract) return;
    for (int i = 0; i < previews->instance_count; ++i) {
        const PhysicsSimRuntimeMeshPreviewInstance *preview = &previews->instances[i];
        if (!preview->preview_metadata_valid || !preview->has_world_bounds) continue;
        draw_retained_aabb(renderer,
                           state,
                           preview->world_bounds_min,
                           preview->world_bounds_max,
                           color);
    }
}

void scene_editor_canvas_draw_retained_object_overlay(SDL_Renderer *renderer,
                                                      const SceneEditorState *state,
                                                      const CoreSceneObjectContract *object,
                                                      SDL_Color color) {
    SDL_Rect rect = {0};
    CoreObjectVec3 position = {0};
    int x = 0;
    int y = 0;
    if (!renderer || !state || !object) return;
    rect = retained_viewport_rect(state);

    switch (object->kind) {
        case CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE:
            if (object->has_plane_primitive) {
                draw_retained_plane(renderer, state, &object->plane_primitive, color);
            }
            break;
        case CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE:
            if (object->has_rect_prism_primitive) {
                draw_retained_prism(renderer, state, &object->rect_prism_primitive, color);
            }
            break;
        default:
            position = object->object.transform.position;
            scene_editor_viewport_project_point3(&state->viewport,
                                                 rect.x,
                                                 rect.y,
                                                 rect.w,
                                                 rect.h,
                                                 (float)position.x,
                                                 (float)position.y,
                                                 (float)position.z,
                                                 &x,
                                                 &y);
            retained_draw_circle(renderer, x, y, 5, color);
            break;
    }
}
