#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_solver_projection_internal.h"
#include "import/runtime_scene_view_packet_readout.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;

static void expect_true(const char *name, bool condition) {
    if (!condition) {
        fprintf(stderr, "FAIL %-64s condition=false\n", name);
        g_failures += 1;
    }
}

static void expect_near(const char *name, double actual, double expected) {
    if (fabs(actual - expected) > 1e-9) {
        fprintf(stderr, "FAIL %-64s actual=%f expected=%f\n", name, actual, expected);
        g_failures += 1;
    }
}

static const char *authored_runtime_scene_json(void) {
    return "{"
           "\"schema_family\":\"codework_scene\","
           "\"schema_variant\":\"scene_runtime_v1\","
           "\"schema_version\":1,"
           "\"scene_id\":\"scene_view_metadata_authority\","
           "\"space_mode_default\":\"3d\","
           "\"unit_system\":\"meters\","
           "\"world_scale\":1.0,"
           "\"objects\":[{"
             "\"object_id\":\"obj_dynamic\","
             "\"object_type\":\"rect_prism\","
             "\"primitive\":{"
               "\"kind\":\"rect_prism\","
               "\"width\":2.0,"
               "\"height\":2.0,"
               "\"depth\":2.0,"
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
                 "\"min\":{\"x\":-6.0,\"y\":-5.0,\"z\":-4.0},"
                 "\"max\":{\"x\":6.0,\"y\":5.0,\"z\":4.0}"
               "},"
               "\"object_overlays\":[{"
                 "\"object_id\":\"obj_dynamic\","
                 "\"motion_mode\":\"Dynamic\","
                 "\"initial_velocity\":{\"x\":1.0,\"y\":2.0,\"z\":3.0}"
               "}]"
             "}"
           "}"
           "}";
}

static const char *scene_view_packet_json(void) {
    return "{"
           "\"schema_family\":\"codework_scene_view\","
           "\"schema_variant\":\"ray_tracing_scene_view_packet_v0\","
           "\"focused_object_index\":0,"
           "\"preview_quality\":\"material_preview\","
           "\"degraded_reason\":\"none\","
           "\"projected\":true,"
           "\"complete\":true,"
           "\"triangle_count\":1,"
           "\"face_group_count\":1,"
           "\"triangles\":[{"
             "\"rgba\":[200,120,80,255],"
             "\"display_flags\":1,"
             "\"pick_id\":{"
               "\"scene_object_index\":0,"
               "\"primitive_index\":0,"
               "\"triangle_index\":0,"
               "\"local_triangle_index\":0,"
               "\"face_group_index\":99"
             "}"
           "}]"
           "}";
}

static const char *physics_overlay_update_json(void) {
    return "{"
           "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":7},"
           "\"extensions\":{"
             "\"physics_sim\":{"
               "\"scene_domain\":{"
                 "\"active\":true,"
                 "\"shape\":\"box\","
                 "\"min\":{\"x\":-8.0,\"y\":-7.0,\"z\":-6.0},"
                 "\"max\":{\"x\":8.0,\"y\":7.0,\"z\":6.0}"
               "},"
               "\"object_overlays\":[{"
                 "\"object_id\":\"obj_dynamic\","
                 "\"motion_mode\":\"Static\","
                 "\"initial_velocity\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}"
               "}]"
             "}"
           "}"
           "}";
}

static bool read_object_overlay(const char *runtime_json,
                                const char *object_id,
                                SolverProjectionPhysicsOverlay *out_overlay) {
    json_object *root = json_tokener_parse(runtime_json);
    bool ok = false;
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }
    ok = runtime_scene_solver_projection_overlay_for_object(root, object_id, out_overlay);
    json_object_put(root);
    return ok;
}

static bool test_scene_view_readout_does_not_own_physics_metadata(void) {
    PhysicsSimSceneViewPacketReadout readout;
    PhysicsSimRuntimeVisualBootstrap before = {0};
    PhysicsSimRuntimeVisualBootstrap after = {0};
    SolverProjectionPhysicsOverlay before_overlay = {0};
    SolverProjectionPhysicsOverlay after_overlay = {0};
    char diagnostics[256];

    if (!runtime_scene_bridge_load_visual_bootstrap_json(authored_runtime_scene_json(),
                                                         &before,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        fprintf(stderr, "bootstrap before diagnostics: %s\n", diagnostics);
        return false;
    }
    if (!read_object_overlay(authored_runtime_scene_json(), "obj_dynamic", &before_overlay)) {
        return false;
    }
    if (!physics_sim_scene_view_packet_readout_from_json(scene_view_packet_json(),
                                                         &readout,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        fprintf(stderr, "scene-view readout diagnostics: %s\n", diagnostics);
        return false;
    }
    if (!runtime_scene_bridge_load_visual_bootstrap_json(authored_runtime_scene_json(),
                                                         &after,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        fprintf(stderr, "bootstrap after diagnostics: %s\n", diagnostics);
        return false;
    }
    if (!read_object_overlay(authored_runtime_scene_json(), "obj_dynamic", &after_overlay)) {
        return false;
    }

    expect_true("readout_valid", readout.valid);
    expect_true("readout_read_only", readout.read_only);
    expect_true("readout_face_group_is_not_metadata_authority", readout.first_face_group_index == 99);
    expect_true("authored_scene_domain_before", before.scene_domain_authored);
    expect_true("authored_scene_domain_after", after.scene_domain_authored);
    expect_near("scene_domain_min_x_stable", after.scene_domain.min.x, before.scene_domain.min.x);
    expect_near("scene_domain_max_z_stable", after.scene_domain.max.z, before.scene_domain.max.z);
    expect_true("object_overlay_before_found", before_overlay.found);
    expect_true("object_overlay_after_found", after_overlay.found);
    expect_true("object_overlay_motion_stable",
                before_overlay.has_motion_mode &&
                after_overlay.has_motion_mode &&
                before_overlay.is_static == after_overlay.is_static);
    expect_near("object_overlay_velocity_x_stable",
                after_overlay.initial_velocity_x,
                before_overlay.initial_velocity_x);
    expect_near("object_overlay_velocity_z_stable",
                after_overlay.initial_velocity_z,
                before_overlay.initial_velocity_z);
    return g_failures == 0;
}

static bool test_physics_writeback_remains_metadata_authority(void) {
    PhysicsSimRuntimeVisualBootstrap merged_bootstrap = {0};
    SolverProjectionPhysicsOverlay merged_overlay = {0};
    char diagnostics[256];
    char *merged_json = NULL;
    bool ok = runtime_scene_bridge_writeback_physics_overlay_json(authored_runtime_scene_json(),
                                                                  physics_overlay_update_json(),
                                                                  &merged_json,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
    if (!ok || !merged_json) {
        fprintf(stderr, "writeback diagnostics: %s\n", diagnostics);
        free(merged_json);
        return false;
    }

    expect_true("merged_has_physics_namespace", strstr(merged_json, "\"physics_sim\"") != NULL);
    expect_true("merged_has_scene_domain", strstr(merged_json, "\"scene_domain\"") != NULL);
    expect_true("merged_has_object_overlays", strstr(merged_json, "\"object_overlays\"") != NULL);
    expect_true("merged_not_scene_view_packet", strstr(merged_json, "codework_scene_view") == NULL);
    expect_true("merged_not_face_group_writeback", strstr(merged_json, "face_group_index") == NULL);

    if (!runtime_scene_bridge_load_visual_bootstrap_json(merged_json,
                                                         &merged_bootstrap,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        fprintf(stderr, "merged bootstrap diagnostics: %s\n", diagnostics);
        free(merged_json);
        return false;
    }
    if (!read_object_overlay(merged_json, "obj_dynamic", &merged_overlay)) {
        free(merged_json);
        return false;
    }

    expect_true("merged_scene_domain_authored", merged_bootstrap.scene_domain_authored);
    expect_near("merged_scene_domain_min_x", merged_bootstrap.scene_domain.min.x, -8.0);
    expect_near("merged_scene_domain_max_z", merged_bootstrap.scene_domain.max.z, 6.0);
    expect_true("merged_object_overlay_found", merged_overlay.found);
    expect_true("merged_object_overlay_static", merged_overlay.has_motion_mode && merged_overlay.is_static);
    expect_near("merged_object_overlay_velocity_x", merged_overlay.initial_velocity_x, 0.0);
    expect_near("merged_object_overlay_velocity_z", merged_overlay.initial_velocity_z, 0.0);

    free(merged_json);
    return g_failures == 0;
}

int main(void) {
    if (!test_scene_view_readout_does_not_own_physics_metadata()) {
        fprintf(stderr, "runtime_scene_view_metadata_authority_contract_test: readout authority failed\n");
        return 1;
    }
    if (!test_physics_writeback_remains_metadata_authority()) {
        fprintf(stderr, "runtime_scene_view_metadata_authority_contract_test: writeback authority failed\n");
        return 1;
    }
    fprintf(stdout, "runtime_scene_view_metadata_authority_contract_test: success\n");
    return 0;
}
