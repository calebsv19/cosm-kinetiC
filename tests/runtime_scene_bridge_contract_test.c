#include "import/runtime_scene_bridge.h"
#include "core_scene_compile.h"

#include <stdbool.h>
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

int main(void) {
    if (!test_runtime_scene_bridge_preflight_accepts_runtime_fixture()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: runtime fixture preflight failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_rejects_authoring_variant()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: authoring variant rejection failed\n");
        return 1;
    }
    if (!test_runtime_scene_bridge_apply_fixture()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: fixture apply failed\n");
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
    if (!test_runtime_scene_bridge_trio_fixture_compile_writeback_apply()) {
        fprintf(stderr, "runtime_scene_bridge_contract_test: trio fixture roundtrip failed\n");
        return 1;
    }
    fprintf(stdout, "runtime_scene_bridge_contract_test: success\n");
    return 0;
}
