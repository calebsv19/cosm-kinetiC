#include "render/retained_runtime_scene_overlay.h"

#include <math.h>
#include <string.h>

#include "app/editor/scene_editor_viewport.h"

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
static SDL_Color COLOR_SLICE_PLANE = {108, 144, 176, 124};
static SDL_Color COLOR_SLICE_PROJECTION = {176, 196, 214, 156};

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

static void draw_line(SDL_Renderer *renderer, int x0, int y0, int x1, int y1, SDL_Color color) {
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
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

static bool compute_retained_bounds(const PhysicsSimRetainedRuntimeScene *retained,
                                    CoreObjectVec3 *out_min,
                                    CoreObjectVec3 *out_max) {
    bool have = false;
    if (!retained || !out_min || !out_max) return false;
    if (retained->has_line_drawing_scene3d && retained->bounds.enabled) {
        *out_min = retained->bounds.min;
        *out_max = retained->bounds.max;
        return true;
    }
    for (int i = 0; i < retained->retained_object_count; ++i) {
        const CoreSceneObjectContract *object = &retained->objects[i];
        CoreObjectVec3 corners[8];
        int corner_count = 0;
        if (object->has_plane_primitive) {
            const CoreScenePlanePrimitive *plane = &object->plane_primitive;
            const double half_width = plane->width * 0.5;
            const double half_height = plane->height * 0.5;
            CoreObjectVec3 origin = plane->frame.origin;
            CoreObjectVec3 u_plus = vec3_add_scaled(origin, plane->frame.axis_u, half_width);
            CoreObjectVec3 u_minus = vec3_add_scaled(origin, plane->frame.axis_u, -half_width);
            corners[0] = vec3_add_scaled(u_minus, plane->frame.axis_v, -half_height);
            corners[1] = vec3_add_scaled(u_plus, plane->frame.axis_v, -half_height);
            corners[2] = vec3_add_scaled(u_plus, plane->frame.axis_v, half_height);
            corners[3] = vec3_add_scaled(u_minus, plane->frame.axis_v, half_height);
            corner_count = 4;
        } else if (object->has_rect_prism_primitive) {
            const CoreSceneRectPrismPrimitive *prism = &object->rect_prism_primitive;
            const double half_width = prism->width * 0.5;
            const double half_height = prism->height * 0.5;
            const double half_depth = prism->depth * 0.5;
            int corner_index = 0;
            for (int sx = -1; sx <= 1; sx += 2) {
                for (int sy = -1; sy <= 1; sy += 2) {
                    for (int sz = -1; sz <= 1; sz += 2) {
                        CoreObjectVec3 corner = prism->frame.origin;
                        corner = vec3_add_scaled(corner, prism->frame.axis_u, half_width * (double)sx);
                        corner = vec3_add_scaled(corner, prism->frame.axis_v, half_height * (double)sy);
                        corner = vec3_add_scaled(corner, prism->frame.normal, half_depth * (double)sz);
                        corners[corner_index++] = corner;
                    }
                }
            }
            corner_count = 8;
        } else {
            corners[0] = object->object.transform.position;
            corner_count = 1;
        }
        for (int c = 0; c < corner_count; ++c) {
            const CoreObjectVec3 point = corners[c];
            if (!have) {
                *out_min = point;
                *out_max = point;
                have = true;
            } else {
                if (point.x < out_min->x) out_min->x = point.x;
                if (point.y < out_min->y) out_min->y = point.y;
                if (point.z < out_min->z) out_min->z = point.z;
                if (point.x > out_max->x) out_max->x = point.x;
                if (point.y > out_max->y) out_max->y = point.y;
                if (point.z > out_max->z) out_max->z = point.z;
            }
        }
    }
    return have;
}

static bool compute_visual_bounds(const SceneState *scene,
                                  CoreObjectVec3 *out_min,
                                  CoreObjectVec3 *out_max) {
    if (!scene || !out_min || !out_max) return false;
    if (scene->runtime_visual.scene_domain.enabled) {
        *out_min = scene->runtime_visual.scene_domain.min;
        *out_max = scene->runtime_visual.scene_domain.max;
        return true;
    }
    return compute_retained_bounds(&scene->runtime_visual.retained_scene, out_min, out_max);
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

static void draw_cross(SDL_Renderer *renderer, int x, int y, int radius, SDL_Color color) {
    if (!renderer || radius <= 0) return;
    draw_line(renderer, x - radius, y, x + radius, y, color);
    draw_line(renderer, x, y - radius, x, y + radius, color);
}

static CoreObjectVec3 retained_object_origin(const CoreSceneObjectContract *object) {
    if (!object) return (CoreObjectVec3){0};
    if (object->has_plane_primitive) return object->plane_primitive.frame.origin;
    if (object->has_rect_prism_primitive) return object->rect_prism_primitive.frame.origin;
    return object->object.transform.position;
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

static void draw_slice_plane(SDL_Renderer *renderer,
                             const SceneEditorViewportState *viewport,
                             int window_w,
                             int window_h,
                             const SceneState *scene) {
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    CoreObjectVec3 corners[4];
    CoreObjectVec3 center_a;
    CoreObjectVec3 center_b;
    float slice_z = 0.0f;
    if (!renderer || !viewport || !scene) return;
    if (!compute_visual_bounds(scene, &min, &max)) return;

    slice_z = (float)(min.z + (max.z - min.z) * 0.5);
    corners[0] = (CoreObjectVec3){min.x, min.y, slice_z};
    corners[1] = (CoreObjectVec3){max.x, min.y, slice_z};
    corners[2] = (CoreObjectVec3){max.x, max.y, slice_z};
    corners[3] = (CoreObjectVec3){min.x, max.y, slice_z};
    center_a = (CoreObjectVec3){min.x, (min.y + max.y) * 0.5, slice_z};
    center_b = (CoreObjectVec3){max.x, (min.y + max.y) * 0.5, slice_z};

    draw_segment(renderer, viewport, window_w, window_h, corners[0], corners[1], COLOR_SLICE_PLANE);
    draw_segment(renderer, viewport, window_w, window_h, corners[1], corners[2], COLOR_SLICE_PLANE);
    draw_segment(renderer, viewport, window_w, window_h, corners[2], corners[3], COLOR_SLICE_PLANE);
    draw_segment(renderer, viewport, window_w, window_h, corners[3], corners[0], COLOR_SLICE_PLANE);
    draw_segment(renderer, viewport, window_w, window_h, corners[0], corners[2], COLOR_SLICE_PLANE);
    draw_segment(renderer, viewport, window_w, window_h, corners[1], corners[3], COLOR_SLICE_PLANE);
    draw_segment(renderer, viewport, window_w, window_h, center_a, center_b, COLOR_SLICE_PLANE);
}

static void draw_fluid_slice(SDL_Renderer *renderer,
                             const SceneState *scene,
                             const SceneEditorViewportState *viewport,
                             int window_w,
                             int window_h) {
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    float span_x = 1.0f;
    float span_y = 1.0f;
    float slice_z = 0.0f;
    int stride = 1;
    if (!renderer || !scene || !scene->smoke || !viewport) return;
    if (scene->smoke->w <= 0 || scene->smoke->h <= 0) return;
    if (!compute_visual_bounds(scene, &min, &max)) return;

    span_x = (float)(max.x - min.x);
    span_y = (float)(max.y - min.y);
    if (fabsf(span_x) < 0.0001f) span_x = 1.0f;
    if (fabsf(span_y) < 0.0001f) span_y = 1.0f;
    slice_z = (float)(min.z + (max.z - min.z) * 0.5);
    stride = scene->smoke->w > 160 || scene->smoke->h > 160 ? 3 : 2;
    if (scene->smoke->w <= 48 && scene->smoke->h <= 48) stride = 1;

    for (int y = 0; y < scene->smoke->h; y += stride) {
        for (int x = 0; x < scene->smoke->w; x += stride) {
            size_t idx = (size_t)y * (size_t)scene->smoke->w + (size_t)x;
            float density = scene->smoke->density[idx] * 0.05f;
            if (density <= 0.035f) continue;
            density = overlay_clampf(density, 0.0f, 1.0f);
            float u = (scene->smoke->w > 1) ? ((float)x / (float)(scene->smoke->w - 1)) : 0.5f;
            float v = (scene->smoke->h > 1) ? ((float)y / (float)(scene->smoke->h - 1)) : 0.5f;
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
        for (size_t i = 0; i < scene->preset->emitter_count; ++i) {
            const FluidEmitter *emitter = &scene->preset->emitters[i];
            if (emitter->attached_object != object_index) continue;
            switch (emitter->type) {
            case EMITTER_DENSITY_SOURCE:
                return COLOR_SOURCE;
            case EMITTER_VELOCITY_JET:
                return COLOR_JET;
            case EMITTER_SINK:
                return COLOR_SINK;
            default:
                break;
            }
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

static bool emitter_actual_and_slice_points(const SceneState *scene,
                                            const FluidEmitter *emitter,
                                            CoreObjectVec3 *out_actual,
                                            CoreObjectVec3 *out_slice) {
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    CoreObjectVec3 actual = {0};
    CoreObjectVec3 slice = {0};
    if (!scene || !scene->preset || !emitter || !out_actual || !out_slice) return false;
    if (!compute_visual_bounds(scene, &min, &max)) return false;

    if (emitter->attached_object >= 0 &&
        emitter->attached_object < scene->runtime_visual.retained_scene.retained_object_count) {
        actual = retained_object_origin(
            &scene->runtime_visual.retained_scene.objects[emitter->attached_object]);
    } else {
        actual.x = min.x + (max.x - min.x) * emitter->position_x;
        actual.y = min.y + (max.y - min.y) * emitter->position_y;
        actual.z = emitter->position_z;
    }
    slice = actual;
    slice.z = min.z + (max.z - min.z) * 0.5;
    *out_actual = actual;
    *out_slice = slice;
    return true;
}

static void draw_emitter_projection_guides(SDL_Renderer *renderer,
                                           const SceneEditorViewportState *viewport,
                                           int window_w,
                                           int window_h,
                                           const SceneState *scene) {
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
        if (!emitter_actual_and_slice_points(scene, emitter, &actual, &slice)) continue;
        project_point(viewport, window_w, window_h, actual, &actual_x, &actual_y);
        project_point(viewport, window_w, window_h, slice, &slice_x, &slice_y);
        if (fabs(actual.z - slice.z) > 0.001) {
            draw_segment(renderer, viewport, window_w, window_h, actual, slice, COLOR_SLICE_PROJECTION);
        }
        draw_circle(renderer, actual_x, actual_y, 3, color);
        draw_cross(renderer, slice_x, slice_y, 4, lighten_color(color, 0.22f));
    }
}

static void draw_plane(SDL_Renderer *renderer,
                       const SceneEditorViewportState *viewport,
                       int window_w,
                       int window_h,
                       const CoreScenePlanePrimitive *plane,
                       SDL_Color color) {
    CoreObjectVec3 origin = plane->frame.origin;
    CoreObjectVec3 u_plus = vec3_add_scaled(origin, plane->frame.axis_u, plane->width * 0.5);
    CoreObjectVec3 u_minus = vec3_add_scaled(origin, plane->frame.axis_u, -plane->width * 0.5);
    CoreObjectVec3 corners[4];
    corners[0] = vec3_add_scaled(u_minus, plane->frame.axis_v, -plane->height * 0.5);
    corners[1] = vec3_add_scaled(u_plus, plane->frame.axis_v, -plane->height * 0.5);
    corners[2] = vec3_add_scaled(u_plus, plane->frame.axis_v, plane->height * 0.5);
    corners[3] = vec3_add_scaled(u_minus, plane->frame.axis_v, plane->height * 0.5);
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
    const double half_width = prism->width * 0.5;
    const double half_height = prism->height * 0.5;
    const double half_depth = prism->depth * 0.5;
    int corner_index = 0;
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                CoreObjectVec3 corner = prism->frame.origin;
                corner = vec3_add_scaled(corner, prism->frame.axis_u, half_width * (double)sx);
                corner = vec3_add_scaled(corner, prism->frame.axis_v, half_height * (double)sy);
                corner = vec3_add_scaled(corner, prism->frame.normal, half_depth * (double)sz);
                corners[corner_index++] = corner;
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
                     color);
    }
}

bool retained_runtime_scene_overlay_active(const SceneState *scene) {
    return scene &&
           scene->runtime_visual.valid &&
           scene->runtime_visual.retained_scene.valid_contract &&
           scene->mode_route.requested_space_mode == SPACE_MODE_3D;
}

bool retained_runtime_scene_overlay_frame_view(SceneState *scene,
                                               int window_w,
                                               int window_h) {
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    bool have_bounds = false;
    if (!retained_runtime_scene_overlay_active(scene)) return false;

    scene_editor_viewport_init(&scene->runtime_viewport, SPACE_MODE_3D, SPACE_MODE_3D);
    if (scene->runtime_visual.scene_domain.enabled) {
        min = scene->runtime_visual.scene_domain.min;
        max = scene->runtime_visual.scene_domain.max;
        have_bounds = true;
    } else {
        have_bounds = compute_retained_bounds(&scene->runtime_visual.retained_scene, &min, &max);
    }
    if (have_bounds) {
        scene_editor_viewport_frame_bounds(&scene->runtime_viewport,
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
    if (!retained_runtime_scene_overlay_active(scene) || !renderer) return;

    draw_slice_plane(renderer, &scene->runtime_viewport, window_w, window_h, scene);
    draw_fluid_slice(renderer, scene, &scene->runtime_viewport, window_w, window_h);
    draw_origin_axes(renderer, &scene->runtime_viewport, window_w, window_h);
    draw_domain_box(renderer, &scene->runtime_viewport, window_w, window_h, &scene->runtime_visual);
    for (int i = 0; i < scene->runtime_visual.retained_scene.retained_object_count; ++i) {
        const CoreSceneObjectContract *object = &scene->runtime_visual.retained_scene.objects[i];
        SDL_Color color = lighten_color(object_color(scene, i), 0.04f);
        if (object->has_plane_primitive) {
            draw_plane(renderer, &scene->runtime_viewport, window_w, window_h, &object->plane_primitive, color);
        } else if (object->has_rect_prism_primitive) {
            draw_prism(renderer, &scene->runtime_viewport, window_w, window_h, &object->rect_prism_primitive, color);
        } else {
            int x = 0;
            int y = 0;
            project_point(&scene->runtime_viewport, window_w, window_h, object->object.transform.position, &x, &y);
            draw_circle(renderer, x, y, 4, color);
        }
    }
    draw_emitter_projection_guides(renderer, &scene->runtime_viewport, window_w, window_h, scene);
}
