#include "import/runtime_scene_bridge.h"
#include "app/scene_objects.h"
#include "app/editor/scene_editor_session.h"
#include "core_scene_compile.h"
#include "runtime_scene_bridge_contract_session_suite.h"
#include "runtime_scene_bridge_contract_writeback_suite.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file_alloc(const char *path, size_t *out_size) {
    FILE *f = NULL;
    long sz = 0;
    size_t got = 0;
    char *buf = NULL;
    if (out_size) *out_size = 0;
    if (!path) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) {
        free(buf);
        return NULL;
    }
    buf[sz] = '\0';
    if (out_size) *out_size = (size_t)sz;
    return buf;
}

static bool test_runtime_scene_bridge_preflight_accepts_runtime_fixture(void) {
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_file("../shared/assets/scenes/trio_contract/scene_runtime_min.json",
                                                  &preflight);
    if (!ok) return false;
    if (!preflight.valid_contract) return false;
    if (strcmp(preflight.scene_id, "scene_trio_min") != 0) return false;
    if (preflight.object_count != 1) return false;
    return true;
}

static bool test_runtime_scene_bridge_rejects_authoring_variant(void) {
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_file("../shared/assets/scenes/trio_contract/scene_authoring_min.json",
                                                  &preflight);
    if (ok) return false;
    if (strstr(preflight.diagnostics, "scene_runtime_v1") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_rejects_malformed_runtime_payload(void) {
    const char *runtime_json_missing_variant =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_missing_variant\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_json(runtime_json_missing_variant, &preflight);
    if (ok) return false;
    if (strstr(preflight.diagnostics, "missing schema_variant") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_rejects_noncanonical_unit_system(void) {
    const char *runtime_json_bad_units =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_bad_units\","
        "\"unit_system\":\"centimeters\","
        "\"world_scale\":1.0,"
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_json(runtime_json_bad_units, &preflight);
    if (ok) return false;
    if (strstr(preflight.diagnostics, "unit_system must be meters") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_apply_fixture(void) {
    RuntimeSceneBridgePreflight summary;
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    bool ok = runtime_scene_bridge_apply_file("../shared/assets/scenes/trio_contract/scene_runtime_min.json",
                                              &cfg,
                                              &preset,
                                              &summary);
    if (!ok) return false;
    if (!summary.valid_contract) return false;
    if (cfg.space_mode != SPACE_MODE_2D) return false;
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_2D) return false;
    if (preset.object_count != 1) return false;
    if (preset.emitter_count != 1) return false;
    return true;
}

static bool test_runtime_scene_bridge_apply_uses_world_scale_mapping(void) {
    const char *runtime_json_scaled =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_scaled_ps\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":2.0,"
        "\"objects\":[{"
          "\"object_id\":\"obj_scaled\","
          "\"object_type\":\"box\","
          "\"transform\":{"
             "\"position\":{\"x\":0.2,\"y\":0.3,\"z\":0.4},"
             "\"scale\":{\"x\":0.12,\"y\":0.13,\"z\":0.14}"
          "}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.4,\"y\":0.5,\"z\":0.6},\"intensity\":12.0}],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    cfg.sim_mode = SIM_MODE_WIND_TUNNEL;
    if (!runtime_scene_bridge_apply_json(runtime_json_scaled, &cfg, &preset, &summary)) return false;
    if (cfg.space_mode != SPACE_MODE_3D) return false;
    if (cfg.sim_mode != SIM_MODE_WIND_TUNNEL) return false;
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_3D) return false;
    if (preset.object_count != 1 || preset.emitter_count != 1) return false;
    if (fabsf(preset.objects[0].position_x - 0.4f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].position_y - 0.6f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].position_z - 0.8f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_x - 0.24f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_y - 0.26f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_z - 0.28f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].position_x - 0.8f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].position_y - 1.0f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].position_z - 1.2f) > 1e-6f) return false;
    return true;
}

static bool test_runtime_scene_bridge_overlay_velocity_bootstraps_runtime_body(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_runtime_velocity_bootstrap\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{"
          "\"object_id\":\"obj_runtime_body\","
          "\"object_type\":\"box\","
          "\"transform\":{"
             "\"position\":{\"x\":0.25,\"y\":0.40,\"z\":0.0},"
             "\"scale\":{\"x\":0.10,\"y\":0.20,\"z\":0.05}"
          "},"
          "\"flags\":{\"locked\":false}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"physics_sim\":{"
            "\"object_overlays\":[{"
              "\"object_id\":\"obj_runtime_body\","
              "\"motion_mode\":\"Dynamic\","
              "\"initial_velocity\":{\"x\":0.50,\"y\":-0.25,\"z\":1.50}"
            "}]"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary;
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};

    cfg.window_w = 800;
    cfg.window_h = 600;

    if (!runtime_scene_bridge_apply_json(runtime_json, &cfg, &preset, &summary)) return false;
    if (!summary.valid_contract) return false;
    if (cfg.space_mode != SPACE_MODE_3D) return false;
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_3D) return false;
    if (preset.object_count != 1) return false;
    if (preset.objects[0].is_static) return false;
    if (!preset.objects[0].gravity_enabled) return false;
    if (fabsf(preset.objects[0].initial_velocity_x - 0.50f) > 1e-6f) return false;
    if (fabsf(preset.objects[0].initial_velocity_y - (-0.25f)) > 1e-6f) return false;
    if (fabsf(preset.objects[0].initial_velocity_z - 1.50f) > 1e-6f) return false;

    scene.config = &cfg;
    scene.preset = &preset;
    scene_objects_init(&scene);
    scene_objects_add_presets(&scene);

    if (scene.objects.count != 1) {
        scene_objects_shutdown(&scene);
        return false;
    }
    if (fabsf(scene.objects.objects[0].body.position.x - 200.0f) > 1e-6f) {
        scene_objects_shutdown(&scene);
        return false;
    }
    if (fabsf(scene.objects.objects[0].body.position.y - 240.0f) > 1e-6f) {
        scene_objects_shutdown(&scene);
        return false;
    }
    if (fabsf(scene.objects.objects[0].body.velocity.x - 400.0f) > 1e-6f) {
        scene_objects_shutdown(&scene);
        return false;
    }
    if (fabsf(scene.objects.objects[0].body.velocity.y - (-150.0f)) > 1e-6f) {
        scene_objects_shutdown(&scene);
        return false;
    }

    scene_objects_shutdown(&scene);
    return true;
}

static bool test_runtime_scene_bridge_apply_compile_output_sets_3d(void) {
    const char *authoring_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_apply_compile_ps\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{"
          "\"object_id\":\"obj_apply\","
          "\"object_type\":\"circle\","
          "\"transform\":{"
             "\"position\":{\"x\":0.2,\"y\":0.3,\"z\":0.4},"
             "\"rotation\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
             "\"scale\":{\"x\":0.12,\"y\":0.13,\"z\":0.14}"
          "},"
          "\"flags\":{\"locked\":true}"
        "}],"
        "\"hierarchy\":[],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.4,\"y\":0.5,\"z\":0.6},\"intensity\":12.0}],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    RuntimeSceneBridgePreflight summary;
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    CoreResult r = core_scene_compile_authoring_to_runtime(authoring_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code != CORE_OK || !runtime_json) return false;
    {
        bool ok = runtime_scene_bridge_apply_json(runtime_json, &cfg, &preset, &summary);
        free(runtime_json);
        if (!ok) return false;
    }
    if (cfg.space_mode != SPACE_MODE_3D) return false;
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_3D) return false;
    if (preset.object_count != 1) return false;
    if (preset.objects[0].type != PRESET_OBJECT_CIRCLE) return false;
    if (!preset.objects[0].is_static) return false;
    if (preset.objects[0].gravity_enabled) return false;
    if (preset.emitter_count != 1) return false;
    return true;
}

static bool test_runtime_scene_bridge_apply_retains_canonical_primitives(void) {
    RuntimeSceneBridgePreflight summary;
    PhysicsSimRetainedRuntimeScene retained;
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    bool ok = runtime_scene_bridge_apply_file("tests/fixtures/runtime_scene_primitive_retained.json",
                                              &cfg,
                                              &preset,
                                              &summary);
    if (!ok) return false;

    runtime_scene_bridge_get_last_retained_scene(&retained);
    if (!retained.valid_contract) return false;
    if (strcmp(retained.root.scene_id, "scene_physics_retained_primitives") != 0) return false;
    if (retained.root.space_mode_default != CORE_SCENE_SPACE_MODE_3D) return false;
    if (retained.root.unit_kind != CORE_UNIT_METER) return false;
    if (fabs(retained.root.world_scale - 1.0) > 1e-9) return false;
    if (retained.object_count != 2) return false;
    if (retained.retained_object_count != 2) return false;
    if (retained.primitive_object_count != 2) return false;
    if (!retained.has_line_drawing_scene3d) return false;
    if (!retained.bounds.enabled || !retained.bounds.clamp_on_edit) return false;
    if (fabs(retained.bounds.min.x - (-12.0)) > 1e-9) return false;
    if (fabs(retained.bounds.max.z - 8.0) > 1e-9) return false;
    if (!retained.construction_plane.valid) return false;
    if (retained.construction_plane.axis_plane != CORE_OBJECT_PLANE_YZ) return false;
    if (fabs(retained.construction_plane.offset - 1.25) > 1e-9) return false;

    if (retained.objects[0].kind != CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE) return false;
    if (retained.objects[0].object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED) return false;
    if (retained.objects[0].object.locked_plane != CORE_OBJECT_PLANE_YZ) return false;
    if (!retained.objects[0].has_plane_primitive) return false;
    if (fabs(retained.objects[0].plane_primitive.width - 4.5) > 1e-9) return false;
    if (fabs(retained.objects[0].plane_primitive.height - 2.5) > 1e-9) return false;
    if (!retained.objects[0].plane_primitive.lock_to_construction_plane) return false;
    if (retained.objects[1].kind != CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE) return false;
    if (retained.objects[1].object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D) return false;
    if (!retained.objects[1].has_rect_prism_primitive) return false;
    if (fabs(retained.objects[1].rect_prism_primitive.width - 3.0) > 1e-9) return false;
    if (fabs(retained.objects[1].rect_prism_primitive.depth - 1.5) > 1e-9) return false;

    if (cfg.space_mode != SPACE_MODE_3D) return false;
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_3D) return false;
    if (preset.object_count != 2) return false;
    if (fabsf(preset.objects[0].position_y - ((0.5f + 10.0f) / 20.0f)) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_x - (2.25f / 24.0f)) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_z - 0.01f) > 1e-6f) return false;
    if (fabsf(preset.objects[1].position_x - ((2.75f + 12.0f) / 24.0f)) > 1e-6f) return false;
    if (fabsf(preset.objects[1].size_x - (1.5f / 24.0f)) > 1e-6f) return false;
    if (fabsf(preset.objects[1].size_z - 0.75f) > 1e-6f) return false;
    if (preset.emitter_count != 0) return false;
    return true;
}

static bool test_runtime_scene_bridge_apply_visual_test_scene_fixture(void) {
    RuntimeSceneBridgePreflight summary;
    PhysicsSimRetainedRuntimeScene retained;
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    bool ok = runtime_scene_bridge_apply_file("config/samples/ps4d_runtime_scene_visual_test.json",
                                              &cfg,
                                              &preset,
                                              &summary);
    if (!ok) return false;

    runtime_scene_bridge_get_last_retained_scene(&retained);
    if (!retained.valid_contract) return false;
    if (strcmp(retained.root.scene_id, "scene_ps4d_visual_test") != 0) return false;
    if (retained.root.space_mode_default != CORE_SCENE_SPACE_MODE_3D) return false;
    if (retained.retained_object_count != 3) return false;
    if (!retained.has_line_drawing_scene3d) return false;
    if (!retained.bounds.enabled) return false;
    if (fabs(retained.bounds.min.x - (-6.0)) > 1e-9) return false;
    if (fabs(retained.bounds.max.z - 4.0) > 1e-9) return false;
    if (cfg.space_mode != SPACE_MODE_3D) return false;
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_3D) return false;
    if (preset.domain != SCENE_DOMAIN_STRUCTURAL) return false;
    if (fabsf(preset.domain_width - 12.0f) > 1e-6f) return false;
    if (fabsf(preset.domain_height - 10.0f) > 1e-6f) return false;
    if (preset.object_count != 3) return false;
    if (fabsf(preset.objects[0].size_x - (4.0f / 12.0f)) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_y - (3.0f / 10.0f)) > 1e-6f) return false;
    if (fabsf(preset.objects[0].size_z - 0.01f) > 1e-6f) return false;
    if (fabsf(preset.objects[1].size_x - (1.0f / 12.0f)) > 1e-6f) return false;
    if (fabsf(preset.objects[1].size_z - 1.25f) > 1e-6f) return false;
    return true;
}

static bool test_runtime_scene_bridge_visual_bootstrap_uses_authored_scene_domain(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_runtime_visual_bootstrap_authored\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{"
          "\"object_id\":\"obj_runtime_visual\","
          "\"object_type\":\"rect_prism\","
          "\"primitive\":{"
             "\"kind\":\"rect_prism\","
             "\"width\":2.0,"
             "\"height\":3.0,"
             "\"depth\":4.0,"
             "\"frame\":{"
                "\"origin\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
                "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
                "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
                "\"normal\":{\"x\":0.0,\"y\":0.0,\"z\":1.0}"
             "}"
          "}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"line_drawing\":{"
            "\"scene3d\":{"
              "\"bounds\":{"
                "\"enabled\":true,"
                "\"min\":{\"x\":-4.0,\"y\":-4.0,\"z\":-4.0},"
                "\"max\":{\"x\":4.0,\"y\":4.0,\"z\":4.0}"
              "}"
            "}"
          "},"
          "\"physics_sim\":{"
            "\"scene_domain\":{"
              "\"active\":true,"
              "\"shape\":\"box\","
              "\"min\":{\"x\":-7.0,\"y\":-5.0,\"z\":-3.0},"
              "\"max\":{\"x\":7.0,\"y\":5.0,\"z\":3.0}"
            "}"
          "}"
        "}"
        "}";
    PhysicsSimRuntimeVisualBootstrap bootstrap = {0};
    char diagnostics[256];
    if (!runtime_scene_bridge_load_visual_bootstrap_json(runtime_json,
                                                         &bootstrap,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        return false;
    }
    if (!bootstrap.valid) return false;
    if (!bootstrap.retained_scene.valid_contract) return false;
    if (!bootstrap.scene_domain.enabled) return false;
    if (!bootstrap.scene_domain_authored) return false;
    if (!bootstrap.scene_up.valid) return false;
    if (bootstrap.scene_up.source != PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z) return false;
    if (fabs(bootstrap.scene_up.direction.x) > 1e-9) return false;
    if (fabs(bootstrap.scene_up.direction.y) > 1e-9) return false;
    if (fabs(bootstrap.scene_up.direction.z - 1.0) > 1e-9) return false;
    if (fabs(bootstrap.scene_domain.min.x - (-7.0)) > 1e-9) return false;
    if (fabs(bootstrap.scene_domain.max.z - 3.0) > 1e-9) return false;
    return true;
}

static bool test_runtime_scene_bridge_visual_bootstrap_falls_back_to_retained_bounds(void) {
    PhysicsSimRuntimeVisualBootstrap bootstrap = {0};
    char diagnostics[256];
    if (!runtime_scene_bridge_load_visual_bootstrap_file("config/samples/ps4d_runtime_scene_visual_test.json",
                                                         &bootstrap,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        return false;
    }
    if (!bootstrap.valid) return false;
    if (!bootstrap.retained_scene.valid_contract) return false;
    if (!bootstrap.scene_domain.enabled) return false;
    if (bootstrap.scene_domain_authored) return false;
    if (!bootstrap.scene_up.valid) return false;
    if (bootstrap.scene_up.source != PHYSICS_SIM_RUNTIME_SCENE_UP_CONSTRUCTION_PLANE_FRAME) return false;
    if (fabs(bootstrap.scene_up.direction.x) > 1e-9) return false;
    if (fabs(bootstrap.scene_up.direction.y) > 1e-9) return false;
    if (fabs(bootstrap.scene_up.direction.z - 1.0) > 1e-9) return false;
    if (fabs(bootstrap.scene_domain.min.x - (-6.0)) > 1e-9) return false;
    if (fabs(bootstrap.scene_domain.max.z - 4.0) > 1e-9) return false;
    return true;
}

static bool test_runtime_scene_bridge_visual_bootstrap_falls_back_to_axis_plane_scene_up(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_bootstrap_axis_up\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"line_drawing\":{"
            "\"scene3d\":{"
              "\"construction_plane\":{"
                "\"axis\":\"xz\","
                "\"offset\":0.0"
              "},"
              "\"bounds\":{"
                "\"enabled\":true,"
                "\"min\":{\"x\":-1.0,\"y\":-1.0,\"z\":-1.0},"
                "\"max\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
              "}"
            "}"
          "}"
        "}"
        "}";
    PhysicsSimRuntimeVisualBootstrap bootstrap = {0};
    char diagnostics[256];
    if (!runtime_scene_bridge_load_visual_bootstrap_json(runtime_json,
                                                         &bootstrap,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        return false;
    }
    if (!bootstrap.scene_up.valid) return false;
    if (bootstrap.scene_up.source != PHYSICS_SIM_RUNTIME_SCENE_UP_CONSTRUCTION_PLANE_AXIS) return false;
    if (fabs(bootstrap.scene_up.direction.x) > 1e-9) return false;
    if (fabs(bootstrap.scene_up.direction.y - 1.0) > 1e-9) return false;
    if (fabs(bootstrap.scene_up.direction.z) > 1e-9) return false;
    return true;
}



static bool test_runtime_scene_bridge_trio_fixture_compile_writeback_apply(void) {
    size_t authoring_size = 0;
    size_t overlay_size = 0;
    char *authoring_json = read_text_file_alloc("../shared/assets/scenes/trio_contract/scene_authoring_interop_min.json",
                                                &authoring_size);
    char *overlay_json = read_text_file_alloc("../shared/assets/scenes/trio_contract/physics_overlay_min.json",
                                              &overlay_size);
    char diagnostics[256];
    char *runtime_json = NULL;
    char *merged_json = NULL;
    RuntimeSceneBridgePreflight summary;
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    CoreResult r;
    bool ok = false;

    if (!authoring_json || !overlay_json || authoring_size == 0 || overlay_size == 0) {
        free(authoring_json);
        free(overlay_json);
        return false;
    }

    r = core_scene_compile_authoring_to_runtime(authoring_json,
                                                &runtime_json,
                                                diagnostics,
                                                sizeof(diagnostics));
    if (r.code != CORE_OK || !runtime_json) {
        free(authoring_json);
        free(overlay_json);
        free(runtime_json);
        return false;
    }

    ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                             overlay_json,
                                                             &merged_json,
                                                             diagnostics,
                                                             sizeof(diagnostics));
    if (!ok || !merged_json) {
        free(authoring_json);
        free(overlay_json);
        free(runtime_json);
        free(merged_json);
        return false;
    }

    if (strstr(merged_json, "\"line_drawing\"") == NULL ||
        strstr(merged_json, "\"ray_tracing\"") == NULL ||
        strstr(merged_json, "\"physics_sim\"") == NULL) {
        free(authoring_json);
        free(overlay_json);
        free(runtime_json);
        free(merged_json);
        return false;
    }

    ok = runtime_scene_bridge_apply_json(merged_json, &cfg, &preset, &summary);
    free(authoring_json);
    free(overlay_json);
    free(runtime_json);
    free(merged_json);
    if (!ok) return false;
    if (cfg.space_mode != SPACE_MODE_2D) return false;
    if (preset.dimension_mode != SCENE_DIMENSION_MODE_2D) return false;
    return true;
}

static int run_bridge_apply_file_mode(const char *runtime_scene_path) {
    RuntimeSceneBridgePreflight preflight;
    RuntimeSceneBridgePreflight summary;
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    bool ok = false;

    if (!runtime_scene_path || !runtime_scene_path[0]) {
        fprintf(stderr, "runtime_scene_bridge_apply_file: missing path\n");
        return 1;
    }

    ok = runtime_scene_bridge_preflight_file(runtime_scene_path, &preflight);
    if (!ok) {
        fprintf(stderr, "runtime_scene_bridge_preflight_file failed: %s\n", preflight.diagnostics);
        return 1;
    }

    ok = runtime_scene_bridge_apply_file(runtime_scene_path, &cfg, &preset, &summary);
    if (!ok) {
        fprintf(stderr, "runtime_scene_bridge_apply_file failed: %s\n", summary.diagnostics);
        return 1;
    }

    fprintf(stdout,
            "runtime_scene_bridge_apply_file: PASS scene_id=%s objects=%d materials=%d lights=%d cameras=%d space_mode=%d dimension_mode=%d\n",
            summary.scene_id,
            summary.object_count,
            summary.material_count,
            summary.light_count,
            summary.camera_count,
            cfg.space_mode,
            preset.dimension_mode);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--bridge-apply-file") == 0) {
        return run_bridge_apply_file_mode(argv[2]);
    }
    if (argc != 1) {
        fprintf(stderr, "usage: %s [--bridge-apply-file <scene_runtime.json>]\n", argv[0]);
        return 1;
    }

    if (!test_runtime_scene_bridge_preflight_accepts_runtime_fixture()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: runtime fixture preflight failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_rejects_authoring_variant()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: authoring variant rejection failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_rejects_malformed_runtime_payload()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: malformed runtime rejection failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_rejects_noncanonical_unit_system()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: noncanonical unit-system rejection failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_apply_fixture()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: fixture apply failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_apply_uses_world_scale_mapping()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: world-scale mapping failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_overlay_velocity_bootstraps_runtime_body()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: runtime-body velocity bootstrap failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_apply_compile_output_sets_3d()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: compile/apply mapping failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_apply_retains_canonical_primitives()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: retained primitive capture failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_apply_visual_test_scene_fixture()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: visual test scene fixture apply failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_visual_bootstrap_uses_authored_scene_domain()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: visual bootstrap authored domain failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_visual_bootstrap_falls_back_to_retained_bounds()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: visual bootstrap retained-bounds fallback failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_visual_bootstrap_falls_back_to_axis_plane_scene_up()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: visual bootstrap scene-up axis fallback failed\n");
        return 1;
    }
    if (!test_scene_editor_session_bootstrap_preserves_retained_scene()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: editor session bootstrap failed\n");
        return 1;
    }
    if (!test_scene_editor_session_overlay_defaults_respect_locked_objects()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: editor session overlay defaults failed\n");
        return 1;
    }
    if (!test_scene_editor_session_overlay_mutation_updates_selected_object()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: editor session overlay mutation failed\n");
        return 1;
    }
    if (!test_scene_editor_session_retained_emitter_defaults_to_object_normal()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: editor session emitter normal default failed\n");
        return 1;
    }
    if (!test_scene_editor_session_overlay_json_build_and_merge()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: editor session overlay build/merge failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_apply_merged_overlay_affects_solver_mapping()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: merged overlay apply mapping failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_apply_merged_scene_domain_affects_solver_domain()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: merged scene-domain apply failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_apply_merged_emitter_overlay_affects_solver_emitters()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: merged emitter overlay apply failed\n");
        return 1;
    }
    if (!test_scene_editor_session_roundtrip_reopen_hydrates_saved_overlay()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: roundtrip reopen hydrate failed\n");
        return 1;
    }
    if (!test_scene_editor_session_tracks_legacy_selection()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: editor session legacy selection failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_overlay_preserves_non_physics_state()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback preserve check failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_rejects_foreign_extension_namespace()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback namespace gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_rejects_forbidden_top_level_overlay_key()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback forbidden key gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_rejects_invalid_space_mode_value()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback invalid space-mode gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_rejects_non_object_physics_extension_payload()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback payload-type gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_rejects_missing_overlay_meta()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback missing-metadata gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_rejects_stale_logical_clock()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback stale-logical-clock gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_rejects_wrong_overlay_producer()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback producer gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_rejects_negative_logical_clock()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback negative-clock gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_rejects_runtime_core_unit_system_overlay()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback unit-system core-field gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_rejects_runtime_core_world_scale_overlay()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback world-scale core-field gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_space_mode_tiebreak_rejects_lexically_larger_producer()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback tiebreak reject gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_writeback_space_mode_tiebreak_accepts_newer_clock()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: writeback tiebreak accept gate failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_trio_fixture_compile_writeback_apply()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: trio fixture roundtrip failed\n");
        return 1;
    }
    fprintf(stdout, "runtime_scene_bridge_contract_test: success\n");
    return 0;
}
