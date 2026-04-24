#include "app/editor/scene_editor_canvas_retained.h"

#include "app/editor/scene_editor_internal.h"

#include <math.h>

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
