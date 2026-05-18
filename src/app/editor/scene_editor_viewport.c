#include "app/editor/scene_editor_viewport.h"

#include "core_viewport2d.h"

#include <math.h>
#include <string.h>

#define VIEWPORT_2D_MIN_ZOOM 0.5f
#define VIEWPORT_2D_MAX_ZOOM 8.0f
#define VIEWPORT_3D_BASE_DISTANCE 3.5f
#define VIEWPORT_3D_MIN_DISTANCE 0.75f
#define VIEWPORT_3D_MAX_DISTANCE 96.0f
#define VIEWPORT_3D_SCENE_DISTANCE_FACTOR 6.0f
#define VIEWPORT_3D_MIN_ZOOM 0.0025f
#define VIEWPORT_3D_MAX_ZOOM 4.0f

static float viewport_clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float viewport_wrap_degrees(float value) {
    while (value > 180.0f) value -= 360.0f;
    while (value < -180.0f) value += 360.0f;
    return value;
}

static float scene_editor_viewport_canvas_min_dim(int canvas_w, int canvas_h) {
    float min_dim = (float)((canvas_w < canvas_h) ? canvas_w : canvas_h);
    if (min_dim <= 0.0f || !isfinite(min_dim)) {
        min_dim = 1.0f;
    }
    return min_dim;
}

static float scene_editor_viewport_2d_min_core_zoom(float min_dim) {
    float core_zoom = min_dim * VIEWPORT_2D_MIN_ZOOM;
    if (core_zoom <= 0.0f || !isfinite(core_zoom)) {
        core_zoom = 0.0001f;
    }
    return core_zoom;
}

static float scene_editor_viewport_2d_max_core_zoom(float min_dim) {
    float min_core_zoom = scene_editor_viewport_2d_min_core_zoom(min_dim);
    float max_core_zoom = min_dim * VIEWPORT_2D_MAX_ZOOM;
    if (!isfinite(max_core_zoom) || max_core_zoom < min_core_zoom) {
        max_core_zoom = min_core_zoom;
    }
    return max_core_zoom;
}

static bool scene_editor_viewport_2d_core_from_state(const SceneEditorViewportState *state,
                                                     int canvas_w,
                                                     int canvas_h,
                                                     CoreViewport2D *out_core) {
    CoreResult result = {0};
    float min_dim = 0.0f;
    float zoom = 1.0f;
    float center_x = 0.5f;
    float center_y = 0.5f;
    if (!state || !out_core || canvas_w <= 0 || canvas_h <= 0) return false;

    result = core_viewport2d_init(out_core);
    if (result.code != CORE_OK) return false;

    min_dim = scene_editor_viewport_canvas_min_dim(canvas_w, canvas_h);
    out_core->min_zoom = scene_editor_viewport_2d_min_core_zoom(min_dim);
    out_core->max_zoom = scene_editor_viewport_2d_max_core_zoom(min_dim);
    zoom = viewport_clampf(state->orthographic_zoom, VIEWPORT_2D_MIN_ZOOM, VIEWPORT_2D_MAX_ZOOM);
    out_core->zoom = core_viewport2d_clamp_zoom(out_core, zoom * min_dim);
    center_x = isfinite(state->center_x) ? state->center_x : 0.5f;
    center_y = isfinite(state->center_y) ? state->center_y : 0.5f;
    out_core->pan_x = ((float)canvas_w * 0.5f) - center_x * out_core->zoom;
    out_core->pan_y = ((float)canvas_h * 0.5f) - center_y * out_core->zoom;
    return core_viewport2d_validate(out_core).code == CORE_OK;
}

static bool scene_editor_viewport_2d_state_from_core(SceneEditorViewportState *state,
                                                     int canvas_w,
                                                     int canvas_h,
                                                     const CoreViewport2D *core) {
    float min_dim = 0.0f;
    if (!state || !core || canvas_w <= 0 || canvas_h <= 0) return false;
    if (!isfinite(core->zoom) || core->zoom <= 0.0f) return false;

    min_dim = scene_editor_viewport_canvas_min_dim(canvas_w, canvas_h);
    state->center_x = (((float)canvas_w * 0.5f) - core->pan_x) / core->zoom;
    state->center_y = (((float)canvas_h * 0.5f) - core->pan_y) / core->zoom;
    state->orthographic_zoom = viewport_clampf(core->zoom / min_dim, VIEWPORT_2D_MIN_ZOOM, VIEWPORT_2D_MAX_ZOOM);
    return true;
}

static bool scene_editor_viewport_2d_reset_to_fit(SceneEditorViewportState *state,
                                                  int surface_w,
                                                  int surface_h,
                                                  float min_x,
                                                  float min_y,
                                                  float max_x,
                                                  float max_y,
                                                  float fit) {
    CoreViewport2D core = {0};
    CoreResult result = {0};
    float min_dim = 0.0f;
    float content_width = fmaxf(fabsf(max_x - min_x), 0.001f);
    float content_height = fmaxf(fabsf(max_y - min_y), 0.001f);
    float fit_w = (float)surface_w;
    float fit_h = (float)surface_h;
    float pad_x = 0.0f;
    float pad_y = 0.0f;
    if (!state || surface_w <= 0 || surface_h <= 0) return false;

    if (fit > 0.0f && fit < 1.0f) {
        fit_w *= fit;
        fit_h *= fit;
        pad_x = ((float)surface_w - fit_w) * 0.5f;
        pad_y = ((float)surface_h - fit_h) * 0.5f;
    }

    result = core_viewport2d_init(&core);
    if (result.code != CORE_OK) return false;

    min_dim = scene_editor_viewport_canvas_min_dim(surface_w, surface_h);
    core.min_zoom = scene_editor_viewport_2d_min_core_zoom(min_dim);
    core.max_zoom = scene_editor_viewport_2d_max_core_zoom(min_dim);
    result = core_viewport2d_reset_to_fit(&core, fit_w, fit_h, content_width, content_height);
    if (result.code != CORE_OK) return false;

    core.pan_x += pad_x - min_x * core.zoom;
    core.pan_y += pad_y - min_y * core.zoom;
    return scene_editor_viewport_2d_state_from_core(state, surface_w, surface_h, &core);
}

static float scene_editor_viewport_orbit_distance_limit(const SceneEditorViewportState *state) {
    float max_distance = VIEWPORT_3D_MAX_DISTANCE;
    float span_x = 0.0f;
    float span_y = 0.0f;
    float span_z = 0.0f;
    float max_span = 0.0f;
    if (!state || !state->has_scene_bounds) return max_distance;

    span_x = fabsf(state->scene_max_x - state->scene_min_x);
    span_y = fabsf(state->scene_max_y - state->scene_min_y);
    span_z = fabsf(state->scene_max_z - state->scene_min_z);
    max_span = fmaxf(span_x, fmaxf(span_y, span_z));
    if (max_span <= 0.001f) return max_distance;

    max_distance = fmaxf(max_distance, max_span * VIEWPORT_3D_SCENE_DISTANCE_FACTOR);
    return max_distance;
}

static float scene_editor_viewport_min_zoom(const SceneEditorViewportState *state) {
    float min_zoom = VIEWPORT_3D_MIN_ZOOM;
    float distance_limit = scene_editor_viewport_orbit_distance_limit(state);
    if (distance_limit > VIEWPORT_3D_MIN_DISTANCE) {
        min_zoom = fmaxf(min_zoom, VIEWPORT_3D_BASE_DISTANCE / distance_limit);
    }
    return min_zoom;
}

static void scene_editor_viewport_set_default_scene_bounds(SceneEditorViewportState *state) {
    if (!state) return;
    state->has_scene_bounds = true;
    state->scene_min_x = 0.0f;
    state->scene_min_y = 0.0f;
    state->scene_min_z = 0.0f;
    state->scene_max_x = 1.0f;
    state->scene_max_y = 1.0f;
    state->scene_max_z = 0.0f;
}

static void scene_editor_viewport_store_scene_bounds(SceneEditorViewportState *state,
                                                     float min_x,
                                                     float min_y,
                                                     float min_z,
                                                     float max_x,
                                                     float max_y,
                                                     float max_z) {
    if (!state) return;
    state->has_scene_bounds = true;
    state->scene_min_x = min_x;
    state->scene_min_y = min_y;
    state->scene_min_z = min_z;
    state->scene_max_x = max_x;
    state->scene_max_y = max_y;
    state->scene_max_z = max_z;
}

float scene_editor_viewport_active_zoom(const SceneEditorViewportState *state) {
    float zoom = 1.0f;
    if (!state) return zoom;
    if (state->requested_mode == SPACE_MODE_3D) {
        float distance = viewport_clampf(state->orbit_distance,
                                         VIEWPORT_3D_MIN_DISTANCE,
                                         scene_editor_viewport_orbit_distance_limit(state));
        zoom = VIEWPORT_3D_BASE_DISTANCE / distance;
        return viewport_clampf(zoom, scene_editor_viewport_min_zoom(state), VIEWPORT_3D_MAX_ZOOM);
    }
    return viewport_clampf(state->orthographic_zoom, VIEWPORT_2D_MIN_ZOOM, VIEWPORT_2D_MAX_ZOOM);
}

static void scene_editor_viewport_clamp_center(SceneEditorViewportState *state) {
    float min_x = 0.0f;
    float max_x = 1.0f;
    float min_y = 0.0f;
    float max_y = 1.0f;
    if (!state) return;
    if (state->has_scene_bounds) {
        min_x = state->scene_min_x;
        max_x = state->scene_max_x;
        min_y = state->scene_min_y;
        max_y = state->scene_max_y;
    }

    if (state->requested_mode == SPACE_MODE_3D) {
        state->center_x = viewport_clampf(state->center_x, min_x, max_x);
        state->center_y = viewport_clampf(state->center_y, min_y, max_y);
        if (state->has_scene_bounds) {
            state->center_z = viewport_clampf(state->center_z, state->scene_min_z, state->scene_max_z);
        }
        return;
    }

    if ((max_x - min_x) <= 0.0001f) {
        state->center_x = min_x;
    } else {
        state->center_x = viewport_clampf(state->center_x, min_x, max_x);
    }
    if ((max_y - min_y) <= 0.0001f) {
        state->center_y = min_y;
    } else {
        state->center_y = viewport_clampf(state->center_y, min_y, max_y);
    }
}

void scene_editor_viewport_frame(SceneEditorViewportState *state) {
    if (!state) return;
    scene_editor_viewport_set_default_scene_bounds(state);
    state->center_x = 0.5f;
    state->center_y = 0.5f;
    state->center_z = 0.0f;
    state->orthographic_zoom = 1.0f;
    state->orbit_yaw_deg = -35.0f;
    state->orbit_pitch_deg = 24.0f;
    state->orbit_distance = VIEWPORT_3D_BASE_DISTANCE;
    state->navigation_active = false;
    state->navigation_mode = SCENE_EDITOR_VIEWPORT_NAV_NONE;
    scene_editor_viewport_clamp_center(state);
}

static void viewport_project_bounds_extents(const SceneEditorViewportState *state,
                                            float min_x,
                                            float min_y,
                                            float min_z,
                                            float max_x,
                                            float max_y,
                                            float max_z,
                                            float *out_view_width,
                                            float *out_view_height) {
    float yaw_rad = 0.0f;
    float pitch_rad = 0.0f;
    float min_view_x = 0.0f;
    float min_view_y = 0.0f;
    float max_view_x = 0.0f;
    float max_view_y = 0.0f;
    bool seeded = false;
    int i = 0;
    if (!state || !out_view_width || !out_view_height) return;
    yaw_rad = state->orbit_yaw_deg * (float)M_PI / 180.0f;
    pitch_rad = state->orbit_pitch_deg * (float)M_PI / 180.0f;
    for (i = 0; i < 8; ++i) {
        float x = (i & 1) ? max_x : min_x;
        float y = (i & 2) ? max_y : min_y;
        float z = (i & 4) ? max_z : min_z;
        float dx = x - state->center_x;
        float dy = y - state->center_y;
        float dz = z - state->center_z;
        float yaw_x = dx * cosf(yaw_rad) - dy * sinf(yaw_rad);
        float yaw_y = dx * sinf(yaw_rad) + dy * cosf(yaw_rad);
        float view_x = yaw_x;
        float view_y = yaw_y * cosf(pitch_rad) - dz * sinf(pitch_rad);
        if (!seeded) {
            min_view_x = max_view_x = view_x;
            min_view_y = max_view_y = view_y;
            seeded = true;
        } else {
            if (view_x < min_view_x) min_view_x = view_x;
            if (view_x > max_view_x) max_view_x = view_x;
            if (view_y < min_view_y) min_view_y = view_y;
            if (view_y > max_view_y) max_view_y = view_y;
        }
    }
    *out_view_width = max_view_x - min_view_x;
    *out_view_height = max_view_y - min_view_y;
}

void scene_editor_viewport_frame_bounds(SceneEditorViewportState *state,
                                        int surface_w,
                                        int surface_h,
                                        float min_x,
                                        float min_y,
                                        float min_z,
                                        float max_x,
                                        float max_y,
                                        float max_z) {
    float span_x = 0.0f;
    float span_y = 0.0f;
    float span_z = 0.0f;
    float max_span = 0.0f;
    float fit = 0.72f;
    float min_dim = 0.0f;
    if (!state) return;

    if (surface_w <= 0) surface_w = 1;
    if (surface_h <= 0) surface_h = 1;
    min_dim = scene_editor_viewport_canvas_min_dim(surface_w, surface_h);

    scene_editor_viewport_store_scene_bounds(state,
                                             min_x,
                                             min_y,
                                             min_z,
                                             max_x,
                                             max_y,
                                             max_z);
    state->center_x = 0.5f * (min_x + max_x);
    state->center_y = 0.5f * (min_y + max_y);
    state->center_z = 0.5f * (min_z + max_z);
    span_x = fabsf(max_x - min_x);
    span_y = fabsf(max_y - min_y);
    span_z = fabsf(max_z - min_z);
    max_span = fmaxf(span_x, fmaxf(span_y, span_z));
    if (max_span < 0.001f) max_span = 1.0f;

    if (state->requested_mode == SPACE_MODE_3D) {
        float projected_width = 0.0f;
        float projected_height = 0.0f;
        float zoom_x = 0.0f;
        float zoom_y = 0.0f;
        float fit_zoom = 0.0f;
        viewport_project_bounds_extents(state,
                                        min_x,
                                        min_y,
                                        min_z,
                                        max_x,
                                        max_y,
                                        max_z,
                                        &projected_width,
                                        &projected_height);
        if (projected_width < 0.001f) projected_width = max_span;
        if (projected_height < 0.001f) projected_height = max_span;
        zoom_x = ((float)surface_w * fit) / (projected_width * min_dim);
        zoom_y = ((float)surface_h * fit) / (projected_height * min_dim);
        fit_zoom = fminf(zoom_x, zoom_y);
        if (fit_zoom <= 0.0f || !isfinite(fit_zoom)) {
            fit_zoom = VIEWPORT_3D_BASE_DISTANCE /
                       viewport_clampf(max_span * 1.8f + 1.0f,
                                       VIEWPORT_3D_MIN_DISTANCE,
                                       scene_editor_viewport_orbit_distance_limit(state));
        }
        state->orbit_distance = viewport_clampf(VIEWPORT_3D_BASE_DISTANCE / fit_zoom,
                                                VIEWPORT_3D_MIN_DISTANCE,
                                                scene_editor_viewport_orbit_distance_limit(state));
    } else {
        if (!scene_editor_viewport_2d_reset_to_fit(state, surface_w, surface_h, min_x, min_y, max_x, max_y, fit)) {
            float zoom_x = ((float)surface_w * fit) / (fmaxf(span_x, 0.001f) * min_dim);
            float zoom_y = ((float)surface_h * fit) / (fmaxf(span_y, 0.001f) * min_dim);
            state->orthographic_zoom =
                viewport_clampf(fminf(zoom_x, zoom_y), VIEWPORT_2D_MIN_ZOOM, VIEWPORT_2D_MAX_ZOOM);
        }
        scene_editor_viewport_clamp_center(state);
    }
}

void scene_editor_viewport_init(SceneEditorViewportState *state,
                                SpaceMode requested_mode,
                                SpaceMode projection_mode) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->requested_mode = requested_mode;
    state->projection_mode = projection_mode;
    state->last_pointer_x = -1;
    state->last_pointer_y = -1;
    scene_editor_viewport_frame(state);
}

void scene_editor_viewport_set_modes(SceneEditorViewportState *state,
                                     SpaceMode requested_mode,
                                     SpaceMode projection_mode) {
    if (!state) return;
    state->requested_mode = requested_mode;
    state->projection_mode = projection_mode;
    scene_editor_viewport_clamp_center(state);
}

bool scene_editor_viewport_begin_navigation(SceneEditorViewportState *state,
                                            SceneEditorViewportNavigationMode mode,
                                            int pointer_x,
                                            int pointer_y) {
    if (!state || mode == SCENE_EDITOR_VIEWPORT_NAV_NONE) return false;
    state->navigation_active = true;
    state->navigation_mode = mode;
    state->last_pointer_x = pointer_x;
    state->last_pointer_y = pointer_y;
    return true;
}

bool scene_editor_viewport_update_navigation(SceneEditorViewportState *state,
                                             int pointer_x,
                                             int pointer_y,
                                             int canvas_w,
                                             int canvas_h) {
    float zoom = 1.0f;
    float min_dim = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    if (!state || !state->navigation_active) return false;
    dx = (float)(pointer_x - state->last_pointer_x);
    dy = (float)(pointer_y - state->last_pointer_y);
    state->last_pointer_x = pointer_x;
    state->last_pointer_y = pointer_y;
    if (fabsf(dx) < 0.001f && fabsf(dy) < 0.001f) return false;

    if (state->navigation_mode == SCENE_EDITOR_VIEWPORT_NAV_ORBIT) {
        state->orbit_yaw_deg = viewport_wrap_degrees(state->orbit_yaw_deg + dx * 0.45f);
        state->orbit_pitch_deg = viewport_clampf(state->orbit_pitch_deg - dy * 0.35f, -85.0f, 85.0f);
        return true;
    }

    if (state->requested_mode != SPACE_MODE_3D) {
        CoreViewport2D core = {0};
        if (!scene_editor_viewport_2d_core_from_state(state, canvas_w, canvas_h, &core)) return false;
        if (core_viewport2d_pan_by(&core, dx, dy).code != CORE_OK) return false;
        if (!scene_editor_viewport_2d_state_from_core(state, canvas_w, canvas_h, &core)) return false;
    } else {
        min_dim = scene_editor_viewport_canvas_min_dim(canvas_w, canvas_h);
        zoom = scene_editor_viewport_active_zoom(state);
        if (zoom <= 0.0f) zoom = 1.0f;
        state->center_x -= dx / (min_dim * zoom);
        state->center_y -= dy / (min_dim * zoom);
    }
    scene_editor_viewport_clamp_center(state);
    return true;
}

void scene_editor_viewport_end_navigation(SceneEditorViewportState *state) {
    if (!state) return;
    state->navigation_active = false;
    state->navigation_mode = SCENE_EDITOR_VIEWPORT_NAV_NONE;
}

bool scene_editor_viewport_apply_wheel(SceneEditorViewportState *state,
                                       int wheel_y,
                                       int screen_x,
                                       int screen_y,
                                       int canvas_x,
                                       int canvas_y,
                                       int canvas_w,
                                       int canvas_h) {
    if (!state || wheel_y == 0) return false;
    if (state->requested_mode == SPACE_MODE_3D) {
        float next_distance = state->orbit_distance * powf(0.9f, (float)wheel_y);
        next_distance = viewport_clampf(next_distance,
                                        VIEWPORT_3D_MIN_DISTANCE,
                                        scene_editor_viewport_orbit_distance_limit(state));
        if (fabsf(next_distance - state->orbit_distance) <= 0.0001f) return false;
        state->orbit_distance = next_distance;
    } else {
        CoreViewport2D core = {0};
        float zoom_factor = 0.0f;
        float local_x = 0.0f;
        float local_y = 0.0f;
        if (canvas_w <= 0 || canvas_h <= 0) return false;
        if (!scene_editor_viewport_2d_core_from_state(state, canvas_w, canvas_h, &core)) return false;
        zoom_factor = powf(1.1f, (float)wheel_y);
        if (!isfinite(zoom_factor) || zoom_factor <= 0.0f) return false;
        local_x = (float)(screen_x - canvas_x);
        local_y = (float)(screen_y - canvas_y);
        if (core_viewport2d_zoom_at_screen_anchor(&core, local_x, local_y, zoom_factor).code != CORE_OK) {
            return false;
        }
        if (!scene_editor_viewport_2d_state_from_core(state, canvas_w, canvas_h, &core)) return false;
    }
    scene_editor_viewport_clamp_center(state);
    return true;
}

void scene_editor_viewport_world_to_screen(const SceneEditorViewportState *state,
                                           int canvas_x,
                                           int canvas_y,
                                           int canvas_w,
                                           int canvas_h,
                                           float world_x,
                                           float world_y,
                                           int *screen_x,
                                           int *screen_y) {
    float zoom = 1.0f;
    float min_dim = 0.0f;
    float center_px_x = 0.0f;
    float center_px_y = 0.0f;
    SceneEditorViewportState fallback = {0};
    if (!screen_x || !screen_y) return;
    if (canvas_w <= 0 || canvas_h <= 0) {
        *screen_x = canvas_x;
        *screen_y = canvas_y;
        return;
    }
    if (!state) {
        scene_editor_viewport_init(&fallback, SPACE_MODE_2D, SPACE_MODE_2D);
        state = &fallback;
    }
    if (state->requested_mode != SPACE_MODE_3D) {
        CoreViewport2D core = {0};
        float local_x = 0.0f;
        float local_y = 0.0f;
        if (scene_editor_viewport_2d_core_from_state(state, canvas_w, canvas_h, &core) &&
            core_viewport2d_content_to_screen(&core, world_x, world_y, &local_x, &local_y).code == CORE_OK) {
            *screen_x = canvas_x + (int)lroundf(local_x);
            *screen_y = canvas_y + (int)lroundf(local_y);
            return;
        }
    }
    zoom = scene_editor_viewport_active_zoom(state);
    min_dim = scene_editor_viewport_canvas_min_dim(canvas_w, canvas_h);
    center_px_x = (float)canvas_x + (float)canvas_w * 0.5f;
    center_px_y = (float)canvas_y + (float)canvas_h * 0.5f;
    *screen_x = (int)lroundf(center_px_x + (world_x - state->center_x) * min_dim * zoom);
    *screen_y = (int)lroundf(center_px_y + (world_y - state->center_y) * min_dim * zoom);
}

void scene_editor_viewport_screen_to_world(const SceneEditorViewportState *state,
                                           int canvas_x,
                                           int canvas_y,
                                           int canvas_w,
                                           int canvas_h,
                                           int screen_x,
                                           int screen_y,
                                           float *world_x,
                                           float *world_y) {
    float zoom = 1.0f;
    float min_dim = 0.0f;
    float center_px_x = 0.0f;
    float center_px_y = 0.0f;
    SceneEditorViewportState fallback = {0};
    if (!world_x || !world_y || canvas_w <= 0 || canvas_h <= 0) return;
    if (!state) {
        scene_editor_viewport_init(&fallback, SPACE_MODE_2D, SPACE_MODE_2D);
        state = &fallback;
    }
    if (state->requested_mode != SPACE_MODE_3D) {
        CoreViewport2D core = {0};
        float local_x = (float)(screen_x - canvas_x);
        float local_y = (float)(screen_y - canvas_y);
        if (scene_editor_viewport_2d_core_from_state(state, canvas_w, canvas_h, &core) &&
            core_viewport2d_screen_to_content(&core, local_x, local_y, world_x, world_y).code == CORE_OK) {
            return;
        }
    }
    zoom = scene_editor_viewport_active_zoom(state);
    if (zoom <= 0.0f) zoom = 1.0f;
    min_dim = scene_editor_viewport_canvas_min_dim(canvas_w, canvas_h);
    center_px_x = (float)canvas_x + (float)canvas_w * 0.5f;
    center_px_y = (float)canvas_y + (float)canvas_h * 0.5f;
    *world_x = state->center_x + ((float)screen_x - center_px_x) / (min_dim * zoom);
    *world_y = state->center_y + ((float)screen_y - center_px_y) / (min_dim * zoom);
}

void scene_editor_viewport_project_point3(const SceneEditorViewportState *state,
                                          int canvas_x,
                                          int canvas_y,
                                          int canvas_w,
                                          int canvas_h,
                                          float world_x,
                                          float world_y,
                                          float world_z,
                                          int *screen_x,
                                          int *screen_y) {
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    float view_x = 0.0f;
    float view_y = 0.0f;
    float yaw_rad = 0.0f;
    float pitch_rad = 0.0f;
    float min_dim = 0.0f;
    float zoom = 1.0f;
    float center_px_x = 0.0f;
    float center_px_y = 0.0f;
    SceneEditorViewportState fallback = {0};
    if (!screen_x || !screen_y || canvas_w <= 0 || canvas_h <= 0) return;
    if (!state) {
        scene_editor_viewport_init(&fallback, SPACE_MODE_2D, SPACE_MODE_2D);
        state = &fallback;
    }

    dx = world_x - state->center_x;
    dy = world_y - state->center_y;
    dz = world_z - state->center_z;
    if (state->requested_mode == SPACE_MODE_3D) {
        float yaw_x = 0.0f;
        float yaw_y = 0.0f;
        yaw_rad = state->orbit_yaw_deg * (float)M_PI / 180.0f;
        pitch_rad = state->orbit_pitch_deg * (float)M_PI / 180.0f;
        yaw_x = dx * cosf(yaw_rad) - dy * sinf(yaw_rad);
        yaw_y = dx * sinf(yaw_rad) + dy * cosf(yaw_rad);
        view_x = yaw_x;
        view_y = yaw_y * cosf(pitch_rad) - dz * sinf(pitch_rad);
    } else {
        view_x = dx;
        view_y = dy;
    }

    min_dim = scene_editor_viewport_canvas_min_dim(canvas_w, canvas_h);
    zoom = scene_editor_viewport_active_zoom(state);
    center_px_x = (float)canvas_x + (float)canvas_w * 0.5f;
    center_px_y = (float)canvas_y + (float)canvas_h * 0.5f;
    *screen_x = (int)lroundf(center_px_x + view_x * min_dim * zoom);
    *screen_y = (int)lroundf(center_px_y + view_y * min_dim * zoom);
}
