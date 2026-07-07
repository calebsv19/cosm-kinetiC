#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_view_packet_readout.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;

static void expect_true(const char *name, bool condition) {
    if (!condition) {
        fprintf(stderr, "FAIL %-64s condition=false\n", name);
        g_failures += 1;
    }
}

static const char *valid_packet_json(void) {
    return "{"
           "\"schema_family\":\"codework_scene_view\","
           "\"schema_variant\":\"ray_tracing_scene_view_packet_v0\","
           "\"focused_object_index\":2,"
           "\"preview_quality\":\"material_preview\","
           "\"degraded_reason\":\"none\","
           "\"projected\":true,"
           "\"complete\":true,"
           "\"triangle_count\":2,"
           "\"face_group_count\":2,"
           "\"triangles\":["
             "{"
               "\"rgba\":[20,40,60,128],"
               "\"display_flags\":9,"
               "\"pick_id\":{"
                 "\"scene_object_index\":2,"
                 "\"primitive_index\":0,"
                 "\"triangle_index\":0,"
                 "\"local_triangle_index\":0,"
                 "\"face_group_index\":3"
               "}"
             "},"
             "{"
               "\"rgba\":[80,100,120,255],"
               "\"display_flags\":0,"
               "\"pick_id\":{"
                 "\"scene_object_index\":2,"
                 "\"primitive_index\":0,"
                 "\"triangle_index\":1,"
                 "\"local_triangle_index\":1,"
                 "\"face_group_index\":4"
               "}"
             "}"
           "]"
           "}";
}

static bool test_readout_consumes_core_scene_view_packet(void) {
    PhysicsSimSceneViewPacketReadout readout;
    char diagnostics[160];

    if (!physics_sim_scene_view_packet_readout_from_json(valid_packet_json(),
                                                         &readout,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        fprintf(stderr, "diagnostics: %s\n", diagnostics);
        return false;
    }

    expect_true("scene_view_readout_valid", readout.valid);
    expect_true("scene_view_readout_read_only", readout.read_only);
    expect_true("scene_view_readout_material", readout.material_preview);
    expect_true("scene_view_readout_transparent", readout.transparent_preview);
    expect_true("scene_view_readout_textured", readout.textured_preview);
    expect_true("scene_view_readout_not_emissive", !readout.emissive_preview);
    expect_true("scene_view_readout_not_mirror", !readout.mirror_preview);
    expect_true("scene_view_readout_projected", readout.projected);
    expect_true("scene_view_readout_complete", readout.complete);
    expect_true("scene_view_readout_focused_index", readout.focused_object_index == 2);
    expect_true("scene_view_readout_triangle_count", readout.triangle_count == 2);
    expect_true("scene_view_readout_face_group_count", readout.face_group_count == 2);
    expect_true("scene_view_readout_first_face_group", readout.first_face_group_index == 3);
    expect_true("scene_view_readout_last_face_group", readout.last_face_group_index == 4);
    expect_true("scene_view_readout_alpha", readout.first_alpha == 128u);
    expect_true("scene_view_readout_core_valid", readout.core_readback.valid);
    expect_true("scene_view_readout_diagnostics",
                strcmp(diagnostics, "scene-view packet read-only") == 0);
    return g_failures == 0;
}

static bool test_invalid_packet_does_not_leave_stale_readout(void) {
    PhysicsSimSceneViewPacketReadout readout;
    char diagnostics[160];
    const char *invalid_packet =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\""
        "}";

    memset(&readout, 0x7f, sizeof(readout));
    if (physics_sim_scene_view_packet_readout_from_json(invalid_packet,
                                                        &readout,
                                                        diagnostics,
                                                        sizeof(diagnostics))) {
        return false;
    }

    expect_true("invalid_packet_clears_valid", !readout.valid);
    expect_true("invalid_packet_keeps_read_only", readout.read_only);
    expect_true("invalid_packet_clears_triangle_count", readout.triangle_count == 0);
    expect_true("invalid_packet_reports_schema",
                strcmp(diagnostics, "unsupported scene-view packet schema") == 0);
    return g_failures == 0;
}

static bool test_readout_does_not_mutate_retained_runtime_scene(void) {
    PhysicsSimSceneViewPacketReadout readout;
    PhysicsSimRetainedRuntimeScene before;
    PhysicsSimRetainedRuntimeScene after;
    RuntimeSceneBridgePreflight summary;
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    char diagnostics[160];
    int before_count = 0;
    char before_scene_id[64];

    if (!runtime_scene_bridge_apply_file("tests/fixtures/runtime_scene_primitive_retained.json",
                                         &cfg,
                                         &preset,
                                         &summary)) {
        return false;
    }

    runtime_scene_bridge_get_last_retained_scene(&before);
    if (!before.valid_contract) return false;
    before_count = before.retained_object_count;
    snprintf(before_scene_id, sizeof(before_scene_id), "%s", before.root.scene_id);

    if (!physics_sim_scene_view_packet_readout_from_json(valid_packet_json(),
                                                         &readout,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        return false;
    }

    runtime_scene_bridge_get_last_retained_scene(&after);
    expect_true("retained_scene_contract_stable", after.valid_contract);
    expect_true("retained_scene_count_stable",
                after.retained_object_count == before_count);
    expect_true("retained_scene_id_stable",
                strcmp(after.root.scene_id, before_scene_id) == 0);
    expect_true("readout_kept_out_of_retained_apply", readout.valid && readout.read_only);
    return g_failures == 0;
}

int main(void) {
    if (!test_readout_consumes_core_scene_view_packet()) {
        fprintf(stderr, "runtime_scene_view_packet_readout_contract_test: readout failed\n");
        return 1;
    }
    if (!test_invalid_packet_does_not_leave_stale_readout()) {
        fprintf(stderr, "runtime_scene_view_packet_readout_contract_test: invalid gate failed\n");
        return 1;
    }
    if (!test_readout_does_not_mutate_retained_runtime_scene()) {
        fprintf(stderr, "runtime_scene_view_packet_readout_contract_test: retained state changed\n");
        return 1;
    }
    fprintf(stdout, "runtime_scene_view_packet_readout_contract_test: success\n");
    return 0;
}
