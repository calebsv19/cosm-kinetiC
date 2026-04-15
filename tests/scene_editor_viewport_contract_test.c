#include "app/editor/scene_editor_viewport.h"

#include <assert.h>
#include <math.h>

static void test_viewport_frames_to_expected_defaults(void) {
    SceneEditorViewportState viewport = {0};

    scene_editor_viewport_init(&viewport, SPACE_MODE_2D, SPACE_MODE_2D);

    assert(fabsf(viewport.center_x - 0.5f) < 0.0001f);
    assert(fabsf(viewport.center_y - 0.5f) < 0.0001f);
    assert(fabsf(scene_editor_viewport_active_zoom(&viewport) - 1.0f) < 0.0001f);
    assert(fabsf(viewport.orbit_distance - 3.5f) < 0.0001f);
}

static void test_viewport_2d_pan_and_zoom_are_bounded(void) {
    SceneEditorViewportState viewport = {0};
    float world_x = 0.0f;
    float world_y = 0.0f;

    scene_editor_viewport_init(&viewport, SPACE_MODE_2D, SPACE_MODE_2D);
    assert(scene_editor_viewport_apply_wheel(&viewport, 2));
    assert(scene_editor_viewport_active_zoom(&viewport) > 1.0f);
    assert(scene_editor_viewport_begin_navigation(&viewport,
                                                  SCENE_EDITOR_VIEWPORT_NAV_PAN,
                                                  200,
                                                  200));
    assert(scene_editor_viewport_update_navigation(&viewport, 280, 160, 640, 480));
    scene_editor_viewport_end_navigation(&viewport);
    assert(viewport.center_x < 0.5f);
    assert(viewport.center_y > 0.5f);

    scene_editor_viewport_screen_to_world(&viewport,
                                          0,
                                          0,
                                          640,
                                          480,
                                          320,
                                          240,
                                          &world_x,
                                          &world_y);
    assert(fabsf(world_x - viewport.center_x) < 0.0001f);
    assert(fabsf(world_y - viewport.center_y) < 0.0001f);
}

static void test_viewport_3d_orbit_and_distance_update(void) {
    SceneEditorViewportState viewport = {0};

    scene_editor_viewport_init(&viewport, SPACE_MODE_3D, SPACE_MODE_2D);
    assert(scene_editor_viewport_begin_navigation(&viewport,
                                                  SCENE_EDITOR_VIEWPORT_NAV_ORBIT,
                                                  100,
                                                  100));
    assert(scene_editor_viewport_update_navigation(&viewport, 140, 70, 640, 480));
    scene_editor_viewport_end_navigation(&viewport);
    assert(viewport.orbit_yaw_deg > -35.0f);
    assert(viewport.orbit_pitch_deg > 24.0f);

    assert(scene_editor_viewport_apply_wheel(&viewport, 1));
    assert(viewport.orbit_distance < 3.5f);
    assert(scene_editor_viewport_active_zoom(&viewport) > 1.0f);

    scene_editor_viewport_frame(&viewport);
    assert(fabsf(viewport.center_x - 0.5f) < 0.0001f);
    assert(fabsf(viewport.center_y - 0.5f) < 0.0001f);
    assert(fabsf(viewport.orbit_distance - 3.5f) < 0.0001f);
}

static void test_viewport_frame_bounds_centers_retained_scene(void) {
    SceneEditorViewportState viewport = {0};

    scene_editor_viewport_init(&viewport, SPACE_MODE_3D, SPACE_MODE_2D);
    scene_editor_viewport_frame_bounds(&viewport,
                                       900,
                                       640,
                                       -4.0f,
                                       -2.0f,
                                       -1.0f,
                                       6.0f,
                                       8.0f,
                                       3.0f);

    assert(fabsf(viewport.center_x - 1.0f) < 0.0001f);
    assert(fabsf(viewport.center_y - 3.0f) < 0.0001f);
    assert(fabsf(viewport.center_z - 1.0f) < 0.0001f);
    assert(viewport.orbit_distance > 0.75f);
    assert(viewport.has_scene_bounds);
    assert(fabsf(viewport.scene_min_x - (-4.0f)) < 0.0001f);
    assert(fabsf(viewport.scene_max_y - 8.0f) < 0.0001f);
}

static void test_viewport_frame_bounds_allows_large_scene_zoom_out(void) {
    SceneEditorViewportState viewport = {0};

    scene_editor_viewport_init(&viewport, SPACE_MODE_3D, SPACE_MODE_2D);
    scene_editor_viewport_frame_bounds(&viewport,
                                       900,
                                       640,
                                       -30.0f,
                                       -18.0f,
                                       -8.0f,
                                       36.0f,
                                       22.0f,
                                       12.0f);

    assert(viewport.orbit_distance > 24.0f);
    assert(viewport.orbit_distance <= 96.0f);
    assert(scene_editor_viewport_active_zoom(&viewport) < 0.15f);
}

static void test_viewport_frame_bounds_2d_respects_surface_aspect(void) {
    SceneEditorViewportState wide = {0};
    SceneEditorViewportState tall = {0};

    scene_editor_viewport_init(&wide, SPACE_MODE_2D, SPACE_MODE_2D);
    scene_editor_viewport_init(&tall, SPACE_MODE_2D, SPACE_MODE_2D);

    scene_editor_viewport_frame_bounds(&wide,
                                       1200,
                                       600,
                                       0.0f,
                                       0.0f,
                                       0.0f,
                                       2.0f,
                                       0.5f,
                                       0.0f);
    scene_editor_viewport_frame_bounds(&tall,
                                       600,
                                       1200,
                                       0.0f,
                                       0.0f,
                                       0.0f,
                                       2.0f,
                                       0.5f,
                                       0.0f);

    assert(fabsf(wide.center_x - 1.0f) < 0.0001f);
    assert(fabsf(tall.center_y - 0.25f) < 0.0001f);
    assert(scene_editor_viewport_active_zoom(&wide) > scene_editor_viewport_active_zoom(&tall));
}

int main(void) {
    test_viewport_frames_to_expected_defaults();
    test_viewport_2d_pan_and_zoom_are_bounded();
    test_viewport_3d_orbit_and_distance_update();
    test_viewport_frame_bounds_centers_retained_scene();
    test_viewport_frame_bounds_allows_large_scene_zoom_out();
    test_viewport_frame_bounds_2d_respects_surface_aspect();
    return 0;
}
