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
    if (fabsf(preset.objects[0].size_x - 2.25f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_y - 1.25f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_z - 0.01f) > 1e-6f) return false;
    if (fabsf(preset.objects[1].size_x - 1.5f) > 1e-6f) return false;
    if (fabsf(preset.objects[1].size_z - 0.75f) > 1e-6f) return false;
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
    if (fabsf(preset.objects[0].initial_velocity_x - 1.0f) > 1e-6f) return false;
    if (fabsf(preset.objects[1].initial_velocity_y - 2.0f) > 1e-6f) return false;
    if (fabsf(preset.objects[1].initial_velocity_z - 0.0f) > 1e-6f) return false;
    return true;
}

static bool test_solver_projection_retained_emitter_overlay_projects_to_solver_emitters(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_solver_projection_emitter_overlay\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":9.0,\"y\":9.0,\"z\":9.0},\"intensity\":99.0}],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"physics_sim\":{"
            "\"object_overlays\":["
              "{"
                "\"object_id\":\"obj_emitter\","
                "\"motion_mode\":\"Dynamic\","
                "\"emitter\":{"
                  "\"active\":true,"
                  "\"type\":\"Jet\","
                  "\"radius\":0.25,"
                  "\"strength\":18.0,"
                  "\"direction\":{\"x\":1.0,\"y\":0.0,\"z\":-0.5}"
                "}"
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
    retained.has_line_drawing_scene3d = true;
    retained.bounds.enabled = true;
    retained.bounds.min = (CoreObjectVec3){-2.5, -4.0, -1.0};
    retained.bounds.max = (CoreObjectVec3){2.5, 4.0, 1.0};
    retained.retained_object_count = 1;
    snprintf(retained.objects[0].object.object_id,
             sizeof(retained.objects[0].object.object_id),
             "obj_emitter");
    retained.objects[0].kind = CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE;
    retained.objects[0].has_rect_prism_primitive = true;
    retained.objects[0].rect_prism_primitive.width = 1.0;
    retained.objects[0].rect_prism_primitive.height = 1.0;
    retained.objects[0].rect_prism_primitive.depth = 1.0;
    retained.objects[0].rect_prism_primitive.frame.origin = (CoreObjectVec3){1.5, -2.0, 0.75};

    ok = runtime_scene_solver_projection_apply_runtime(&retained, root, &cfg, &preset, &summary);
    json_object_put(root);
    if (!ok) return false;

    if (preset.emitter_count != 1) return false;
    if (preset.emitters[0].type != EMITTER_VELOCITY_JET) return false;
    if (fabsf(preset.emitters[0].position_x - 0.8f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].position_y - 0.25f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].position_z - 0.75f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].radius - 0.05f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].strength - 18.0f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].dir_x - 1.0f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].dir_z - (-0.5f)) > 1e-6f) return false;
    if (preset.emitters[0].attached_object != 0) return false;
    if (preset.emitters[0].attached_import != -1) return false;
    return true;
}

static bool test_solver_projection_retained_legacy_sideways_emitter_uses_object_normal(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_solver_projection_legacy_emitter_dir\","
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
                "\"object_id\":\"obj_emitter\","
                "\"emitter\":{"
                  "\"active\":true,"
                  "\"type\":\"Jet\","
                  "\"radius\":0.25,"
                  "\"strength\":18.0,"
                  "\"direction\":{\"x\":0.0,\"y\":-1.0,\"z\":0.0}"
                "}"
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
    retained.retained_object_count = 1;
    snprintf(retained.objects[0].object.object_id,
             sizeof(retained.objects[0].object.object_id),
             "obj_emitter");
    retained.objects[0].kind = CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE;
    retained.objects[0].has_rect_prism_primitive = true;
    retained.objects[0].rect_prism_primitive.width = 1.0;
    retained.objects[0].rect_prism_primitive.height = 1.0;
    retained.objects[0].rect_prism_primitive.depth = 1.0;
    retained.objects[0].rect_prism_primitive.frame.origin = (CoreObjectVec3){0.0, 0.0, 0.25};
    retained.objects[0].rect_prism_primitive.frame.normal = (CoreObjectVec3){0.0, 0.0, 1.0};

    ok = runtime_scene_solver_projection_apply_runtime(&retained, root, &cfg, &preset, &summary);
    json_object_put(root);
    if (!ok) return false;

    if (preset.emitter_count != 1) return false;
    if (fabsf(preset.emitters[0].dir_x - 0.0f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].dir_y - 0.0f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].dir_z - 1.0f) > 1e-6f) return false;
    return true;
}

static bool test_solver_projection_retained_prism_preserves_orientation_basis(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_solver_projection_orientation_basis\","
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
    retained.retained_object_count = 1;
    retained.objects[0].kind = CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE;
    retained.objects[0].has_rect_prism_primitive = true;
    retained.objects[0].rect_prism_primitive.width = 2.0;
    retained.objects[0].rect_prism_primitive.height = 4.0;
    retained.objects[0].rect_prism_primitive.depth = 6.0;
    retained.objects[0].rect_prism_primitive.frame.origin = (CoreObjectVec3){0.5, 0.5, 0.5};
    retained.objects[0].rect_prism_primitive.frame.axis_u = (CoreObjectVec3){0.0, 1.0, 0.0};
    retained.objects[0].rect_prism_primitive.frame.axis_v = (CoreObjectVec3){0.0, 0.0, 1.0};
    retained.objects[0].rect_prism_primitive.frame.normal = (CoreObjectVec3){1.0, 0.0, 0.0};

    ok = runtime_scene_solver_projection_apply_runtime(&retained, root, &cfg, &preset, &summary);
    json_object_put(root);
    if (!ok) return false;

    if (preset.object_count != 1) return false;
    if (!preset.objects[0].orientation_basis_valid) return false;
    if (fabsf(preset.objects[0].orientation_u_x - 0.0f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].orientation_u_y - 1.0f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].orientation_v_z - 1.0f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].orientation_w_x - 1.0f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].orientation_w_z - 0.0f) > 1e-6f) return false;
    return true;
}

static bool test_solver_projection_runtime_root_legacy_sideways_emitter_defaults_up_in_3d(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_solver_projection_runtime_root_legacy_dir\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":["
          "{"
            "\"object_id\":\"obj_emitter\","
            "\"object_type\":\"box\","
            "\"transform\":{"
              "\"position\":{\"x\":1.0,\"y\":-2.0,\"z\":0.5},"
              "\"scale\":{\"x\":0.5,\"y\":0.5,\"z\":0.5}"
            "},"
            "\"flags\":{\"locked\":false}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"physics_sim\":{"
            "\"object_overlays\":["
              "{"
                "\"object_id\":\"obj_emitter\","
                "\"emitter\":{"
                  "\"active\":true,"
                  "\"type\":\"Jet\","
                  "\"radius\":0.25,"
                  "\"strength\":18.0,"
                  "\"direction\":{\"x\":0.0,\"y\":-1.0,\"z\":0.0}"
                "}"
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

    ok = runtime_scene_solver_projection_apply_runtime(&retained, root, &cfg, &preset, &summary);
    json_object_put(root);
    if (!ok) return false;

    if (preset.dimension_mode != SCENE_DIMENSION_MODE_3D) return false;
    if (preset.emitter_count != 1) return false;
    if (fabsf(preset.emitters[0].dir_x - 0.0f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].dir_y - 0.0f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].dir_z - 1.0f) > 1e-6f) return false;
    return true;
}

static bool test_solver_projection_scene_domain_overlay_overrides_retained_bounds(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_solver_projection_scene_domain_overlay\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":2.0,"
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
              "\"min\":{\"x\":-3.0,\"y\":-2.0,\"z\":-1.0},"
              "\"max\":{\"x\":4.0,\"y\":5.0,\"z\":6.0}"
            "}"
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
    retained.root.world_scale = 2.0;
    retained.has_line_drawing_scene3d = true;
    retained.bounds.enabled = true;
    retained.bounds.min = (CoreObjectVec3){-10.0, -10.0, -10.0};
    retained.bounds.max = (CoreObjectVec3){10.0, 10.0, 10.0};

    ok = runtime_scene_solver_projection_apply_runtime(&retained, root, &cfg, &preset, &summary);
    json_object_put(root);
    if (!ok) return false;

    if (preset.domain != SCENE_DOMAIN_STRUCTURAL) return false;
    if (fabsf(preset.domain_width - 14.0f) > 1e-6f) return false;
    if (fabsf(preset.domain_height - 14.0f) > 1e-6f) return false;
    return true;
}

static bool test_solver_projection_scene_domain_falls_back_to_retained_bounds(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_solver_projection_scene_domain_bounds\","
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
    retained.has_line_drawing_scene3d = true;
    retained.bounds.enabled = true;
    retained.bounds.min = (CoreObjectVec3){-6.0, -4.0, -2.0};
    retained.bounds.max = (CoreObjectVec3){8.0, 7.0, 3.0};

    ok = runtime_scene_solver_projection_apply_runtime(&retained, root, &cfg, &preset, &summary);
    json_object_put(root);
    if (!ok) return false;

    if (preset.domain != SCENE_DOMAIN_STRUCTURAL) return false;
    if (fabsf(preset.domain_width - 14.0f) > 1e-6f) return false;
    if (fabsf(preset.domain_height - 11.0f) > 1e-6f) return false;
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
    if (!test_solver_projection_retained_emitter_overlay_projects_to_solver_emitters()) {
        fprintf(stderr, "runtime_scene_solver_projection_contract_test: retained emitter overlay projection failed\n");
        return 1;
    }
    if (!test_solver_projection_retained_legacy_sideways_emitter_uses_object_normal()) {
        fprintf(stderr, "runtime_scene_solver_projection_contract_test: legacy emitter direction remap failed\n");
        return 1;
    }
    if (!test_solver_projection_retained_prism_preserves_orientation_basis()) {
        fprintf(stderr, "runtime_scene_solver_projection_contract_test: retained orientation basis failed\n");
        return 1;
    }
    if (!test_solver_projection_runtime_root_legacy_sideways_emitter_defaults_up_in_3d()) {
        fprintf(stderr, "runtime_scene_solver_projection_contract_test: runtime-root 3d emitter default failed\n");
        return 1;
    }
    if (!test_solver_projection_scene_domain_overlay_overrides_retained_bounds()) {
        fprintf(stderr, "runtime_scene_solver_projection_contract_test: scene-domain overlay precedence failed\n");
        return 1;
    }
    if (!test_solver_projection_scene_domain_falls_back_to_retained_bounds()) {
        fprintf(stderr, "runtime_scene_solver_projection_contract_test: retained-bounds fallback failed\n");
        return 1;
    }
    fprintf(stdout, "runtime_scene_solver_projection_contract_test: success\n");
    return 0;
}
