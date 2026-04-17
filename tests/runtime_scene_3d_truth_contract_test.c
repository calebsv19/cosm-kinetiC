#include "app/scene_state.h"
#include "app/sim_runtime_3d_space.h"
#include "app/sim_runtime_backend.h"
#include "app/sim_runtime_backend_3d_emitter_shapes.h"
#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "app/sim_runtime_emitter.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_solver_projection.h"
#include "render/retained_runtime_scene_overlay_space.h"

#include <json-c/json.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

SimRuntimeBackend *sim_runtime_backend_2d_create(const AppConfig *cfg,
                                                 const FluidScenePreset *preset,
                                                 const SimModeRoute *mode_route,
                                                 const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    (void)cfg;
    (void)preset;
    (void)mode_route;
    (void)runtime_visual;
    return NULL;
}

static bool nearly_equal(double a, double b, double tolerance) {
    return fabs(a - b) <= tolerance;
}

static bool fail_with_message(const char *message) {
    fprintf(stderr, "runtime_scene_3d_truth_contract_test: %s\n", message);
    return false;
}

static double cell_center_world(double world_min, double voxel_size, int index) {
    return world_min + ((double)index + 0.5) * voxel_size;
}

static bool test_projection_backend_and_overlay_share_attached_emitter_world_truth(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_solver_projection_3d_truth_lock\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"physics_sim\":{"
            "\"scene_domain\":{"
              "\"active\":true,"
              "\"shape\":\"box\","
              "\"min\":{\"x\":-2.5,\"y\":-4.0,\"z\":-1.0},"
              "\"max\":{\"x\":2.5,\"y\":4.0,\"z\":1.0}"
            "},"
            "\"object_overlays\":["
              "{"
                "\"object_id\":\"obj_emitter\","
                "\"motion_mode\":\"Dynamic\","
                "\"emitter\":{"
                  "\"active\":true,"
                  "\"radius\":0.25,"
                  "\"strength\":18.0"
                "}"
              "}"
            "]"
          "}"
        "}"
        "}";
    json_object *root = json_tokener_parse(runtime_json);
    RuntimeSceneBridgePreflight summary = {0};
    PhysicsSimRetainedRuntimeScene retained = {0};
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    AppConfig cfg = app_config_default();
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    SceneState scene = {0};
    CoreObjectVec3 expected_world = {1.5, -2.0, 0.75};
    CoreObjectVec3 projection_world = {0};
    CoreObjectVec3 placement_world = {0};
    CoreObjectVec3 overlay_actual = {0};
    CoreObjectVec3 overlay_slice = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffold *state = NULL;
    SimRuntimeBackendReport report = {0};
    SimRuntimeEmitterResolved resolved = {0};
    SimRuntimeEmitterPlacement3D placement = {0};
    double report_slice_world_z = 0.0;
    bool ok = false;

    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }

    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.quality_index = 1;
    cfg.emitter_density_multiplier = 1.0f;

    retained.valid_contract = true;
    retained.root.space_mode_default = CORE_SCENE_SPACE_MODE_3D;
    retained.root.world_scale = 1.0;
    retained.retained_object_count = 1;
    retained.has_line_drawing_scene3d = true;
    retained.bounds.enabled = true;
    retained.bounds.min = (CoreObjectVec3){-2.5, -4.0, -1.0};
    retained.bounds.max = (CoreObjectVec3){2.5, 4.0, 1.0};
    snprintf(retained.objects[0].object.object_id,
             sizeof(retained.objects[0].object.object_id),
             "obj_emitter");
    retained.objects[0].kind = CORE_SCENE_OBJECT_KIND_UNKNOWN;
    snprintf(retained.objects[0].object.object_type,
             sizeof(retained.objects[0].object.object_type),
             "circle");
    retained.objects[0].object.transform.position = expected_world;
    retained.objects[0].object.transform.scale = (CoreObjectVec3){0.5, 0.5, 0.5};

    ok = runtime_scene_solver_projection_apply_runtime(&retained, root, &cfg, &preset, &summary);
    json_object_put(root);
    if (!ok) return fail_with_message("projection apply failed");

    if (cfg.space_mode != SPACE_MODE_3D) return fail_with_message("space mode not 3d");
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_3D) {
        return fail_with_message("preset dimension mode not 3d");
    }
    if (preset.object_count != 1 || preset.emitter_count != 1) {
        fprintf(stderr,
                "runtime_scene_3d_truth_contract_test: projected counts object=%zu emitter=%zu\n",
                preset.object_count,
                preset.emitter_count);
        return false;
    }
    if (preset.emitters[0].attached_object != 0 || preset.emitters[0].attached_import != -1) {
        fprintf(stderr,
                "runtime_scene_3d_truth_contract_test: attachment mismatch obj=%d import=%d\n",
                preset.emitters[0].attached_object,
                preset.emitters[0].attached_import);
        return false;
    }

    projection_world.x = sim_runtime_3d_space_resolve_world_axis(preset.emitters[0].position_x,
                                                                 retained.bounds.min.x,
                                                                 retained.bounds.max.x);
    projection_world.y = sim_runtime_3d_space_resolve_world_axis(preset.emitters[0].position_y,
                                                                 retained.bounds.min.y,
                                                                 retained.bounds.max.y);
    projection_world.z = preset.emitters[0].position_z;
    if (!nearly_equal(projection_world.x, expected_world.x, 0.000001) ||
        !nearly_equal(projection_world.y, expected_world.y, 0.000001) ||
        !nearly_equal(projection_world.z, expected_world.z, 0.000001)) {
        fprintf(stderr,
                "runtime_scene_3d_truth_contract_test: projection world mismatch got=(%.6f, %.6f, %.6f)\n",
                projection_world.x,
                projection_world.y,
                projection_world.z);
        return false;
    }

    visual.valid = true;
    visual.retained_scene = retained;
    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min = retained.bounds.min;
    visual.scene_domain.max = retained.bounds.max;

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return fail_with_message("backend create failed");
    state = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!state) return fail_with_message("backend impl missing");

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;
    scene.runtime_visual = visual;

    if (!sim_runtime_emitter_resolve(&preset, 0, &resolved)) {
        return fail_with_message("resolved emitter missing");
    }
    if (!backend_3d_scaffold_resolve_emitter_placement(&scene, &state->volume.desc, &resolved, &placement)) {
        return fail_with_message("backend placement resolve failed");
    }

    placement_world.x = cell_center_world(state->volume.desc.world_min_x,
                                          state->volume.desc.voxel_size,
                                          placement.center_x);
    placement_world.y = cell_center_world(state->volume.desc.world_min_y,
                                          state->volume.desc.voxel_size,
                                          placement.center_y);
    placement_world.z = cell_center_world(state->volume.desc.world_min_z,
                                          state->volume.desc.voxel_size,
                                          placement.center_z);
    if (!nearly_equal(placement_world.x, expected_world.x, state->volume.desc.voxel_size) ||
        !nearly_equal(placement_world.y, expected_world.y, state->volume.desc.voxel_size) ||
        !nearly_equal(placement_world.z, expected_world.z, state->volume.desc.voxel_size)) {
        fprintf(stderr,
                "runtime_scene_3d_truth_contract_test: backend placement mismatch world=(%.6f, %.6f, %.6f) voxel=%.6f\n",
                placement_world.x,
                placement_world.y,
                placement_world.z,
                state->volume.desc.voxel_size);
        goto fail;
    }

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);

    if (!retained_runtime_overlay_emitter_actual_and_slice_points(
            &scene, &preset.emitters[0], &overlay_actual, &overlay_slice)) {
        return fail_with_message("overlay emitter points failed");
    }
    if (!nearly_equal(overlay_actual.x, expected_world.x, 0.000001) ||
        !nearly_equal(overlay_actual.y, expected_world.y, 0.000001) ||
        !nearly_equal(overlay_actual.z, expected_world.z, 0.000001)) {
        fprintf(stderr,
                "runtime_scene_3d_truth_contract_test: overlay actual mismatch got=(%.6f, %.6f, %.6f)\n",
                overlay_actual.x,
                overlay_actual.y,
                overlay_actual.z);
        goto fail;
    }
    if (!nearly_equal(overlay_slice.x, expected_world.x, 0.000001) ||
        !nearly_equal(overlay_slice.y, expected_world.y, 0.000001)) {
        fprintf(stderr,
                "runtime_scene_3d_truth_contract_test: overlay slice xy mismatch got=(%.6f, %.6f)\n",
                overlay_slice.x,
                overlay_slice.y);
        goto fail;
    }

    if (!sim_runtime_backend_get_report(backend, &report)) return fail_with_message("backend report failed");
    if (!report.compatibility_view_2d_derived || !report.world_bounds_valid || !(report.voxel_size > 0.0f)) {
        return fail_with_message("backend report missing 3d derived bounds");
    }
    report_slice_world_z = sim_runtime_3d_space_slice_world_z(report.world_min_z,
                                                              report.voxel_size,
                                                              report.domain_d,
                                                              report.compatibility_slice_z);
    if (!nearly_equal(overlay_slice.z, report_slice_world_z, 0.000001)) {
        fprintf(stderr,
                "runtime_scene_3d_truth_contract_test: overlay slice z mismatch overlay=%.6f report=%.6f\n",
                overlay_slice.z,
                report_slice_world_z);
        goto fail;
    }

    sim_runtime_backend_destroy(backend);
    return true;

fail:
    sim_runtime_backend_destroy(backend);
    return false;
}

int main(void) {
    if (!test_projection_backend_and_overlay_share_attached_emitter_world_truth()) {
        fprintf(stderr, "runtime_scene_3d_truth_contract_test: cross-boundary proof lock failed\n");
        return 1;
    }
    fprintf(stdout, "runtime_scene_3d_truth_contract_test: success\n");
    return 0;
}
