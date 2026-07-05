#include "app/editor/scene_editor_wind_setup.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

const char *physics_sim_editor_session_motion_mode_label(PhysicsSimOverlayMotionMode mode) {
    (void)mode;
    return "dynamic";
}

const char *physics_sim_editor_session_emitter_type_label(FluidEmitterType type) {
    (void)type;
    return "density";
}

const char *physics_sim_editor_session_emitter_source_mode_3d_label(FluidEmitter3DSourceMode mode) {
    (void)mode;
    return "surface";
}

const char *physics_sim_editor_session_emitter_surface_3d_label(FluidEmitter3DSurface surface) {
    (void)surface;
    return "top";
}

const char *physics_sim_editor_session_emitter_obstacle_mode_3d_label(FluidEmitter3DObstacleMode mode) {
    (void)mode;
    return "attached";
}

const char *physics_sim_editor_session_runtime_mesh_role_label(PhysicsSimRuntimeMeshEditorRole role) {
    (void)role;
    return "mesh";
}

static AppConfig wind_3d_config(void) {
    AppConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.sim_mode = SIM_MODE_WIND_TUNNEL;
    cfg.space_mode = SPACE_MODE_3D;
    cfg.tunnel_inflow_speed = 41.0f;
    cfg.tunnel_inflow_density = 12.0f;
    return cfg;
}

static PhysicsSimEditorSession retained_session_with_wind(bool authored) {
    PhysicsSimEditorSession session;
    FluidScenePreset preset;
    SceneEditorBootstrap bootstrap;
    memset(&session, 0, sizeof(session));
    memset(&preset, 0, sizeof(preset));
    memset(&bootstrap, 0, sizeof(bootstrap));

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene.valid_contract = true;
    bootstrap.wind_tunnel_authored = authored;
    bootstrap.wind_tunnel = wind_tunnel_3d_config_default(NULL);
    bootstrap.wind_tunnel.inlet_face = WIND_TUNNEL_3D_FACE_FRONT;
    bootstrap.wind_tunnel.outlet_face = WIND_TUNNEL_3D_FACE_BACK;
    bootstrap.wind_tunnel.inflow_speed = 22.0f;
    bootstrap.wind_tunnel.inflow_density = 0.5f;
    bootstrap.wind_tunnel.outlet_policy = WIND_TUNNEL_3D_OUTLET_CLEAR;
    bootstrap.wind_tunnel.wall_policy = WIND_TUNNEL_3D_WALL_SLIP;
    physics_sim_editor_session_init(&session, &preset, &bootstrap);
    return session;
}

static void test_wind_setup_requires_wind_3d_retained_context(void) {
    AppConfig cfg = wind_3d_config();
    PhysicsSimEditorSession session = retained_session_with_wind(true);
    SceneEditorWindSetupSummary summary = scene_editor_wind_setup_summary(&cfg, &session);
    assert(summary.active);
    assert(summary.wind_route_active);
    assert(summary.retained_scene_active);

    cfg.space_mode = SPACE_MODE_2D;
    summary = scene_editor_wind_setup_summary(&cfg, &session);
    assert(!summary.active);
    assert(!summary.wind_route_active);
    assert(summary.retained_scene_active);

    cfg = wind_3d_config();
    summary = scene_editor_wind_setup_summary(&cfg, NULL);
    assert(!summary.active);
    assert(summary.wind_route_active);
    assert(!summary.retained_scene_active);
}

static void test_wind_setup_uses_authored_scene_extension_when_present(void) {
    AppConfig cfg = wind_3d_config();
    PhysicsSimEditorSession session = retained_session_with_wind(true);
    SceneEditorWindSetupSummary summary = scene_editor_wind_setup_summary(&cfg, &session);
    assert(summary.active);
    assert(summary.authored_config);
    assert(strcmp(summary.source_label, "scene extension") == 0);
    assert(strcmp(summary.inlet_label, "front") == 0);
    assert(strcmp(summary.outlet_label, "back") == 0);
    assert(strcmp(summary.outlet_policy_label, "clear") == 0);
    assert(strcmp(summary.wall_policy_label, "slip") == 0);
    assert(fabsf(summary.config.inflow_speed - 22.0f) < 0.0001f);
    assert(fabsf(summary.config.inflow_density - 0.5f) < 0.0001f);
}

static void test_wind_setup_falls_back_to_menu_defaults_without_authored_extension(void) {
    AppConfig cfg = wind_3d_config();
    PhysicsSimEditorSession session = retained_session_with_wind(false);
    SceneEditorWindSetupSummary summary = scene_editor_wind_setup_summary(&cfg, &session);
    assert(summary.active);
    assert(!summary.authored_config);
    assert(strcmp(summary.source_label, "menu defaults") == 0);
    assert(strcmp(summary.inlet_label, "left") == 0);
    assert(strcmp(summary.outlet_label, "right") == 0);
    assert(fabsf(summary.config.inflow_speed - 41.0f) < 0.0001f);
    assert(fabsf(summary.config.inflow_density - 12.0f) < 0.0001f);
}

static void test_wind_setup_pairs_inlet_with_opposite_outlet(void) {
    AppConfig cfg = wind_3d_config();
    PhysicsSimEditorSession session = retained_session_with_wind(false);
    SceneEditorWindSetupSummary summary;
    assert(scene_editor_wind_setup_opposite_face(WIND_TUNNEL_3D_FACE_LEFT) ==
           WIND_TUNNEL_3D_FACE_RIGHT);
    assert(scene_editor_wind_setup_opposite_face(WIND_TUNNEL_3D_FACE_TOP) ==
           WIND_TUNNEL_3D_FACE_BOTTOM);
    assert(scene_editor_wind_setup_opposite_face(WIND_TUNNEL_3D_FACE_FRONT) ==
           WIND_TUNNEL_3D_FACE_BACK);
    assert(physics_sim_editor_session_set_wind_tunnel_faces(
        &session,
        &cfg,
        WIND_TUNNEL_3D_FACE_TOP,
        scene_editor_wind_setup_opposite_face(WIND_TUNNEL_3D_FACE_TOP)));
    summary = scene_editor_wind_setup_summary(&cfg, &session);
    assert(summary.config.inlet_face == WIND_TUNNEL_3D_FACE_TOP);
    assert(summary.config.outlet_face == WIND_TUNNEL_3D_FACE_BOTTOM);
    assert(strcmp(summary.source_label, "session config") == 0);
}

int main(void) {
    test_wind_setup_requires_wind_3d_retained_context();
    test_wind_setup_uses_authored_scene_extension_when_present();
    test_wind_setup_falls_back_to_menu_defaults_without_authored_extension();
    test_wind_setup_pairs_inlet_with_opposite_outlet();
    fprintf(stdout, "scene_editor_wind_setup_contract_test: success\n");
    return 0;
}
