#include "import/runtime_scene_bridge.h"
#include "core_scene_compile.h"

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

static bool test_runtime_scene_bridge_writeback_overlay_preserves_non_physics_state(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_1\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[{\"object_id\":\"obj_base\"}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{\"seed\":1},"
          "\"custom_tool\":{\"foo\":1}"
        "},"
        "\"compile_meta\":{\"compiler\":\"core_scene_compile\"}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":10},"
        "\"space_mode_default\":\"3d\","
        "\"extensions\":{"
          "\"physics_sim\":{"
            "\"solver_mode\":\"semi_implicit\","
            "\"iterations\":12"
          "}"
        "}"
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    if (!ok || !merged) {
        free(merged);
        return false;
    }

    ok = strstr(merged, "\"ray_tracing\"") != NULL &&
         strstr(merged, "\"custom_tool\"") != NULL &&
         strstr(merged, "\"physics_sim\"") != NULL &&
         strstr(merged, "\"space_mode_default\":\"3d\"") != NULL;
    free(merged);
    return ok;
}

static bool test_runtime_scene_bridge_writeback_rejects_foreign_extension_namespace(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_2\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":11},"
        "\"extensions\":{"
          "\"ray_tracing\":{\"seed\":2}"
        "}"
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    free(merged);
    if (ok) return false;
    if (strstr(diagnostics, "namespace not allowed") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_writeback_rejects_forbidden_top_level_overlay_key(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_3\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":12},"
        "\"objects\":[{\"object_id\":\"hack\"}]"
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    free(merged);
    if (ok) return false;
    if (strstr(diagnostics, "overlay key not allowed") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_writeback_rejects_invalid_space_mode_value(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_4\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":13},"
        "\"space_mode_default\":\"4d\""
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    free(merged);
    if (ok) return false;
    if (strstr(diagnostics, "space_mode_default must be '2d' or '3d'") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_writeback_rejects_non_object_physics_extension_payload(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_5\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":14},"
        "\"extensions\":{"
          "\"physics_sim\":[1,2]"
        "}"
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    free(merged);
    if (ok) return false;
    if (strstr(diagnostics, "payload must be object") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_writeback_rejects_missing_overlay_meta(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_6\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"extensions\":{\"physics_sim\":{\"iterations\":8}}"
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    free(merged);
    if (ok) return false;
    if (strstr(diagnostics, "overlay_meta object is required") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_writeback_rejects_stale_logical_clock(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_7\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"overlay_merge\":{"
            "\"producer_clocks\":{\"physics_sim\":20}"
          "}"
        "}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":20},"
        "\"extensions\":{\"physics_sim\":{\"iterations\":16}}"
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    free(merged);
    if (ok) return false;
    if (strstr(diagnostics, "logical_clock is stale") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_writeback_rejects_wrong_overlay_producer(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_8\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":1},"
        "\"extensions\":{\"physics_sim\":{\"iterations\":8}}"
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    free(merged);
    if (ok) return false;
    if (strstr(diagnostics, "producer not allowed") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_writeback_rejects_negative_logical_clock(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_9\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":-1},"
        "\"extensions\":{\"physics_sim\":{\"iterations\":8}}"
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    free(merged);
    if (ok) return false;
    if (strstr(diagnostics, "logical_clock must be >= 0") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_writeback_rejects_runtime_core_unit_system_overlay(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_10\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":15},"
        "\"unit_system\":\"centimeters\""
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    free(merged);
    if (ok) return false;
    if (strstr(diagnostics, "overlay key not allowed") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_writeback_rejects_runtime_core_world_scale_overlay(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_11\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":16},"
        "\"world_scale\":2.0"
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    free(merged);
    if (ok) return false;
    if (strstr(diagnostics, "overlay key not allowed") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_writeback_space_mode_tiebreak_rejects_lexically_larger_producer(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_12\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"overlay_merge\":{"
            "\"space_mode_default\":{\"producer\":\"line_drawing\",\"logical_clock\":50}"
          "}"
        "}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":50},"
        "\"space_mode_default\":\"3d\","
        "\"extensions\":{\"physics_sim\":{\"iterations\":8}}"
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    free(merged);
    if (ok) return false;
    if (strstr(diagnostics, "tie-break lost") == NULL) return false;
    return true;
}

static bool test_runtime_scene_bridge_writeback_space_mode_tiebreak_accepts_newer_clock(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_ps_13\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"overlay_merge\":{"
            "\"space_mode_default\":{\"producer\":\"physics_sim\",\"logical_clock\":50}"
          "}"
        "}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":51},"
        "\"space_mode_default\":\"3d\","
        "\"extensions\":{\"physics_sim\":{\"iterations\":32}}"
        "}";
    char diagnostics[256];
    char *merged = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    if (!ok || !merged) {
        free(merged);
        return false;
    }
    ok = (strstr(merged, "\"space_mode_default\":\"3d\"") != NULL);
    free(merged);
    return ok;
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
    if (!test_runtime_scene_bridge_apply_compile_output_sets_3d()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: compile/apply mapping failed\n");
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
