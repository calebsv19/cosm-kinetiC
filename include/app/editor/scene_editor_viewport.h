#ifndef SCENE_EDITOR_VIEWPORT_H
#define SCENE_EDITOR_VIEWPORT_H

#include <stdbool.h>

#include "app/app_config.h"

typedef enum SceneEditorViewportNavigationMode {
    SCENE_EDITOR_VIEWPORT_NAV_NONE = 0,
    SCENE_EDITOR_VIEWPORT_NAV_PAN,
    SCENE_EDITOR_VIEWPORT_NAV_ORBIT
} SceneEditorViewportNavigationMode;

typedef struct SceneEditorViewportState {
    SpaceMode requested_mode;
    SpaceMode projection_mode;
    float center_x;
    float center_y;
    float center_z;
    float orthographic_zoom;
    float orbit_yaw_deg;
    float orbit_pitch_deg;
    float orbit_distance;
    bool has_scene_bounds;
    float scene_min_x;
    float scene_min_y;
    float scene_min_z;
    float scene_max_x;
    float scene_max_y;
    float scene_max_z;
    bool alt_modifier_down;
    bool navigation_active;
    SceneEditorViewportNavigationMode navigation_mode;
    int last_pointer_x;
    int last_pointer_y;
} SceneEditorViewportState;

void scene_editor_viewport_init(SceneEditorViewportState *state,
                                SpaceMode requested_mode,
                                SpaceMode projection_mode);
void scene_editor_viewport_set_modes(SceneEditorViewportState *state,
                                     SpaceMode requested_mode,
                                     SpaceMode projection_mode);
void scene_editor_viewport_frame(SceneEditorViewportState *state);
void scene_editor_viewport_frame_bounds(SceneEditorViewportState *state,
                                        int surface_w,
                                        int surface_h,
                                        float min_x,
                                        float min_y,
                                        float min_z,
                                        float max_x,
                                        float max_y,
                                        float max_z);
float scene_editor_viewport_active_zoom(const SceneEditorViewportState *state);

bool scene_editor_viewport_begin_navigation(SceneEditorViewportState *state,
                                            SceneEditorViewportNavigationMode mode,
                                            int pointer_x,
                                            int pointer_y);
bool scene_editor_viewport_update_navigation(SceneEditorViewportState *state,
                                             int pointer_x,
                                             int pointer_y,
                                             int canvas_w,
                                             int canvas_h);
void scene_editor_viewport_end_navigation(SceneEditorViewportState *state);
bool scene_editor_viewport_apply_wheel(SceneEditorViewportState *state, int wheel_y);

void scene_editor_viewport_world_to_screen(const SceneEditorViewportState *state,
                                           int canvas_x,
                                           int canvas_y,
                                           int canvas_w,
                                           int canvas_h,
                                           float world_x,
                                           float world_y,
                                           int *screen_x,
                                           int *screen_y);
void scene_editor_viewport_screen_to_world(const SceneEditorViewportState *state,
                                           int canvas_x,
                                           int canvas_y,
                                           int canvas_w,
                                           int canvas_h,
                                           int screen_x,
                                           int screen_y,
                                           float *world_x,
                                           float *world_y);
void scene_editor_viewport_project_point3(const SceneEditorViewportState *state,
                                          int canvas_x,
                                          int canvas_y,
                                          int canvas_w,
                                          int canvas_h,
                                          float world_x,
                                          float world_y,
                                          float world_z,
                                          int *screen_x,
                                          int *screen_y);

#endif // SCENE_EDITOR_VIEWPORT_H
