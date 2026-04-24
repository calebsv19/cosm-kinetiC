#include "runtime_scene_bridge_contract_writeback_suite.h"
#include "import/runtime_scene_bridge.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

bool test_runtime_scene_bridge_writeback_overlay_preserves_non_physics_state(void) {
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

bool test_runtime_scene_bridge_writeback_rejects_foreign_extension_namespace(void) {
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

bool test_runtime_scene_bridge_writeback_rejects_forbidden_top_level_overlay_key(void) {
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

bool test_runtime_scene_bridge_writeback_rejects_invalid_space_mode_value(void) {
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

bool test_runtime_scene_bridge_writeback_rejects_non_object_physics_extension_payload(void) {
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

bool test_runtime_scene_bridge_writeback_rejects_missing_overlay_meta(void) {
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

bool test_runtime_scene_bridge_writeback_rejects_stale_logical_clock(void) {
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

bool test_runtime_scene_bridge_writeback_rejects_wrong_overlay_producer(void) {
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

bool test_runtime_scene_bridge_writeback_rejects_negative_logical_clock(void) {
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

bool test_runtime_scene_bridge_writeback_rejects_runtime_core_unit_system_overlay(void) {
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

bool test_runtime_scene_bridge_writeback_rejects_runtime_core_world_scale_overlay(void) {
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

bool test_runtime_scene_bridge_writeback_space_mode_tiebreak_rejects_lexically_larger_producer(void) {
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

bool test_runtime_scene_bridge_writeback_space_mode_tiebreak_accepts_newer_clock(void) {
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
