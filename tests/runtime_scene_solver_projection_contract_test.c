#include "import/runtime_scene_solver_projection.h"

#include <json-c/json.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool test_solver_projection_maps_scaled_runtime_scene(void) {
    const char *runtime_json_with_light =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_solver_projection_scaled\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":2.0,"
        "\"objects\":[{"
          "\"object_id\":\"obj_scaled\","
          "\"object_type\":\"circle\","
          "\"transform\":{"
             "\"position\":{\"x\":0.2,\"y\":0.3,\"z\":0.4},"
             "\"scale\":{\"x\":0.12,\"y\":0.13,\"z\":0.14}"
          "},"
          "\"flags\":{\"locked\":true}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.4,\"y\":0.5,\"z\":0.6},\"intensity\":12.0}],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    json_object *root = json_tokener_parse(runtime_json_with_light);
    RuntimeSceneBridgePreflight summary = {0};
    PhysicsSimRetainedRuntimeScene retained = {0};
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    bool ok = false;
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }

    retained.root.space_mode_default = CORE_SCENE_SPACE_MODE_3D;
    retained.root.world_scale = 2.0;
    retained.retained_object_count = 1;
    retained.objects[0].kind = CORE_SCENE_OBJECT_KIND_UNKNOWN;
    snprintf(retained.objects[0].object.object_type,
             sizeof(retained.objects[0].object.object_type),
             "circle");
    retained.objects[0].object.transform.position = (CoreObjectVec3){0.2, 0.3, 0.4};
    retained.objects[0].object.transform.scale = (CoreObjectVec3){0.12, 0.13, 0.14};
    retained.objects[0].object.flags.locked = true;

    cfg.sim_mode = SIM_MODE_WIND_TUNNEL;
    ok = runtime_scene_solver_projection_apply_runtime(&retained, root, &cfg, &preset, &summary);
    json_object_put(root);
    if (!ok) return false;

    if (cfg.space_mode != SPACE_MODE_3D) return false;
    if (cfg.sim_mode != SIM_MODE_WIND_TUNNEL) return false;
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_3D) return false;
    if (preset.object_count != 1 || preset.emitter_count != 1) return false;
    if (preset.objects[0].type != PRESET_OBJECT_CIRCLE) return false;
    if (!preset.objects[0].is_static) return false;
    if (preset.objects[0].gravity_enabled) return false;
    if (fabsf(preset.objects[0].position_x - 0.4f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_z - 0.28f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].position_z - 1.2f) > 1e-6f) return false;
    if (summary.object_count != 1) return false;
    return true;
}

static bool test_solver_projection_maps_runtime_fixture_primitives_into_legacy_solver_lane(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_solver_projection_primitives\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    json_object *root = json_tokener_parse(runtime_json);
    RuntimeSceneBridgePreflight summary = {0};
    PhysicsSimRetainedRuntimeScene retained = {0};
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    bool ok = false;
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }

    retained.root.space_mode_default = CORE_SCENE_SPACE_MODE_3D;
    retained.root.world_scale = 1.0;
    retained.retained_object_count = 2;

    retained.objects[0].kind = CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE;
    retained.objects[0].has_plane_primitive = true;
    retained.objects[0].plane_primitive.width = 4.5;
    retained.objects[0].plane_primitive.height = 2.5;
    retained.objects[0].plane_primitive.frame.origin = (CoreObjectVec3){0.0, 0.5, -0.75};

    retained.objects[1].kind = CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE;
    retained.objects[1].has_rect_prism_primitive = true;
    retained.objects[1].rect_prism_primitive.width = 3.0;
    retained.objects[1].rect_prism_primitive.height = 2.0;
    retained.objects[1].rect_prism_primitive.depth = 1.5;
    retained.objects[1].rect_prism_primitive.frame.origin = (CoreObjectVec3){2.75, -1.5, 1.0};

    ok = runtime_scene_solver_projection_apply_runtime(&retained, root, &cfg, &preset, &summary);
    json_object_put(root);
    if (!ok) return false;

    if (cfg.space_mode != SPACE_MODE_3D) return false;
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_3D) return false;
    if (preset.object_count != 2) return false;
    if (preset.objects[0].type != PRESET_OBJECT_BOX) return false;
    if (preset.objects[1].type != PRESET_OBJECT_BOX) return false;
    if (fabsf(preset.objects[0].position_x - 0.0f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].position_y - 0.5f) > 1e-6f) return false;
    if (fabsf(preset.objects[1].position_x - 2.75f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_x - 4.5f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_y - 2.5f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_z - 0.02f) > 1e-6f) return false;
    if (fabsf(preset.objects[1].size_x - 3.0f) > 1e-6f) return false;
    if (fabsf(preset.objects[1].size_z - 1.5f) > 1e-6f) return false;
    if (preset.objects[0].is_static) return false;
    if (!preset.objects[0].gravity_enabled) return false;
    if (preset.emitter_count != 0) return false;
    if (summary.object_count != 2) return false;
    return true;
}

static bool test_solver_projection_overlay_motion_mode_overrides_solver_object_state(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_solver_projection_overlay\","
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
            "\"object_overlays\":["
              "{"
                "\"object_id\":\"obj_plane\","
                "\"motion_mode\":\"Static\","
                "\"initial_velocity\":{\"x\":1.0,\"y\":0.0,\"z\":0.0}"
              "},"
              "{"
                "\"object_id\":\"obj_prism\","
                "\"motion_mode\":\"Dynamic\","
                "\"initial_velocity\":{\"x\":0.0,\"y\":2.0,\"z\":0.0}"
              "}"
            "]"
          "}"
        "}"
        "}";
    json_object *root = json_tokener_parse(runtime_json);
    RuntimeSceneBridgePreflight summary = {0};
    PhysicsSimRetainedRuntimeScene retained = {0};
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    bool ok = false;
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }

    retained.root.space_mode_default = CORE_SCENE_SPACE_MODE_3D;
    retained.root.world_scale = 1.0;
    retained.retained_object_count = 2;

    snprintf(retained.objects[0].object.object_id,
             sizeof(retained.objects[0].object.object_id),
             "obj_plane");
    retained.objects[0].object.flags.locked = false;
    retained.objects[0].kind = CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE;
    retained.objects[0].has_plane_primitive = true;
    retained.objects[0].plane_primitive.width = 4.0;
    retained.objects[0].plane_primitive.height = 4.0;
    retained.objects[0].plane_primitive.frame.origin = (CoreObjectVec3){0.0, 0.0, 0.0};

    snprintf(retained.objects[1].object.object_id,
             sizeof(retained.objects[1].object.object_id),
             "obj_prism");
    retained.objects[1].object.flags.locked = true;
    retained.objects[1].kind = CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE;
    retained.objects[1].has_rect_prism_primitive = true;
    retained.objects[1].rect_prism_primitive.width = 1.0;
    retained.objects[1].rect_prism_primitive.height = 2.0;
    retained.objects[1].rect_prism_primitive.depth = 3.0;
    retained.objects[1].rect_prism_primitive.frame.origin = (CoreObjectVec3){1.0, 2.0, 3.0};

    ok = runtime_scene_solver_projection_apply_runtime(&retained, root, &cfg, &preset, &summary);
    json_object_put(root);
    if (!ok) return false;

    if (preset.object_count != 2) return false;
    if (!preset.objects[0].is_static) return false;
    if (preset.objects[0].gravity_enabled) return false;
    if (preset.objects[1].is_static) return false;
    if (!preset.objects[1].gravity_enabled) return false;
    return true;
}

int main(void) {
    if (!test_solver_projection_maps_scaled_runtime_scene()) {
        fprintf(stderr, "runtime_scene_solver_projection_contract_test: scaled runtime mapping failed\n");
        return 1;
    }
    if (!test_solver_projection_maps_runtime_fixture_primitives_into_legacy_solver_lane()) {
        fprintf(stderr, "runtime_scene_solver_projection_contract_test: primitive fixture mapping failed\n");
        return 1;
    }
    if (!test_solver_projection_overlay_motion_mode_overrides_solver_object_state()) {
        fprintf(stderr, "runtime_scene_solver_projection_contract_test: overlay motion-mode override failed\n");
        return 1;
    }
    fprintf(stdout, "runtime_scene_solver_projection_contract_test: success\n");
    return 0;
}
