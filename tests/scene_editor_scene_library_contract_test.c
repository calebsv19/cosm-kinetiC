#include "app/editor/scene_editor_scene_library.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool write_text_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    size_t len = 0;
    if (!f) return false;
    len = strlen(text);
    if (fwrite(text, 1, len, f) != len) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

static bool ensure_dir(const char *path) {
    if (!path || !path[0]) return false;
    if (mkdir(path, 0755) == 0) return true;
    return errno == EEXIST;
}

static void remove_if_exists(const char *path) {
    if (!path || !path[0]) return;
    (void)remove(path);
}

static void remove_dir_if_exists(const char *path) {
    if (!path || !path[0]) return;
    (void)rmdir(path);
}

static bool create_scene_contract_dir(const char *root_dir,
                                      const char *scene_name,
                                      const char *runtime_json,
                                      const char *authoring_json,
                                      char *out_scene_dir,
                                      size_t out_scene_dir_size,
                                      char *out_runtime_path,
                                      size_t out_runtime_path_size,
                                      char *out_authoring_path,
                                      size_t out_authoring_path_size) {
    if (!ensure_dir(root_dir)) return false;
    if (snprintf(out_scene_dir, out_scene_dir_size, "%s/%s", root_dir, scene_name) >= (int)out_scene_dir_size) {
        return false;
    }
    if (!ensure_dir(out_scene_dir)) return false;
    if (snprintf(out_runtime_path,
                 out_runtime_path_size,
                 "%s/%s",
                 out_scene_dir,
                 "scene_runtime.json") >= (int)out_runtime_path_size) {
        return false;
    }
    if (snprintf(out_authoring_path,
                 out_authoring_path_size,
                 "%s/%s",
                 out_scene_dir,
                 "scene_authoring.json") >= (int)out_authoring_path_size) {
        return false;
    }
    return write_text_file(out_runtime_path, runtime_json) &&
           write_text_file(out_authoring_path,
                           (authoring_json && authoring_json[0])
                               ? authoring_json
                               : "{\n  \"scene_name\": \"contract\"\n}\n");
}

static void cleanup_scene_contract_dir(const char *scene_dir,
                                       const char *runtime_path,
                                       const char *authoring_path) {
    remove_if_exists(runtime_path);
    remove_if_exists(authoring_path);
    remove_dir_if_exists(scene_dir);
}

static bool test_scene_library_splits_legacy_and_retained_catalogs(void) {
    PhysicsSimEditorSceneLibrary library = {0};
    PhysicsSimEditorSession session = {0};
    FluidScenePreset preset = {0};
    const PhysicsSimSceneLibraryEntry *legacy = NULL;
    const PhysicsSimSceneLibraryEntry *retained_entry = NULL;
    char root_dir[512];
    char scene_dir[512];
    char runtime_path[512];
    char authoring_path[512];
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"library_split_scene\","
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

    snprintf(root_dir, sizeof(root_dir), "%s", "data/runtime/scene_library_contract_split_root");
    if (!create_scene_contract_dir(root_dir,
                                   "Split Scene",
                                   runtime_json,
                                   "{\n  \"scene_name\": \"Split Scene\"\n}\n",
                                   scene_dir,
                                   sizeof(scene_dir),
                                   runtime_path,
                                   sizeof(runtime_path),
                                   authoring_path,
                                   sizeof(authoring_path))) {
        cleanup_scene_contract_dir(scene_dir, runtime_path, authoring_path);
        remove_dir_if_exists(root_dir);
        return false;
    }

    preset.name = "Tunnel Draft";
    session.has_retained_scene = true;
    snprintf(session.retained_scene.root.scene_id,
             sizeof(session.retained_scene.root.scene_id),
             "%s",
             "library_split_scene");

    physics_sim_editor_scene_library_refresh(&library,
                                             &preset,
                                             &session,
                                             root_dir,
                                             runtime_path);

    legacy = physics_sim_editor_scene_library_selected_legacy(&library);
    retained_entry = physics_sim_editor_scene_library_selected_retained(&library);

    cleanup_scene_contract_dir(scene_dir, runtime_path, authoring_path);
    remove_dir_if_exists(root_dir);

    if (library.mode != PHYSICS_SIM_SCENE_LIBRARY_MODE_3D) return false;
    if (!legacy || !retained_entry) return false;
    if (strcmp(legacy->display_name, "Tunnel Draft") != 0) return false;
    if (strcmp(retained_entry->display_name, "Split Scene") != 0) return false;
    if (!retained_entry->active) return false;
    return physics_sim_editor_scene_library_has_retained_entries(&library);
}

static bool test_scene_library_requires_authoring_and_runtime_pair(void) {
    PhysicsSimEditorSceneLibrary library = {0};
    PhysicsSimEditorSession session = {0};
    FluidScenePreset preset = {0};
    char root_dir[512];
    char scene_dir[512];
    char runtime_path[512];
    char authoring_path[512];
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"missing_authoring_scene\","
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

    snprintf(root_dir, sizeof(root_dir), "%s", "data/runtime/scene_library_contract_missing_authoring_root");
    snprintf(scene_dir, sizeof(scene_dir), "%s/%s", root_dir, "Missing Authoring");
    snprintf(runtime_path, sizeof(runtime_path), "%s/%s", scene_dir, "scene_runtime.json");
    snprintf(authoring_path, sizeof(authoring_path), "%s/%s", scene_dir, "scene_authoring.json");
    if (!ensure_dir(root_dir) || !ensure_dir(scene_dir) || !write_text_file(runtime_path, runtime_json)) {
        cleanup_scene_contract_dir(scene_dir, runtime_path, authoring_path);
        remove_dir_if_exists(root_dir);
        return false;
    }

    preset.name = "Tunnel Draft";
    physics_sim_editor_scene_library_refresh(&library,
                                             &preset,
                                             &session,
                                             root_dir,
                                             runtime_path);

    cleanup_scene_contract_dir(scene_dir, runtime_path, authoring_path);
    remove_dir_if_exists(root_dir);

    return library.retained_scenes.count == 0 &&
           physics_sim_editor_scene_library_selected_retained(&library) == NULL;
}

static bool test_scene_library_can_find_retained_row_by_exact_path(void) {
    PhysicsSimEditorSceneLibrary library = {0};
    PhysicsSimEditorSession session = {0};
    FluidScenePreset preset = {0};
    char root_dir[512];
    char scene_dir[512];
    char runtime_path[512];
    char authoring_path[512];
    int index = -1;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"lookup_scene_contract\","
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

    snprintf(root_dir, sizeof(root_dir), "%s", "data/runtime/scene_library_contract_lookup_root");
    if (!create_scene_contract_dir(root_dir,
                                   "Lookup Scene",
                                   runtime_json,
                                   "{\n  \"scene_name\": \"Lookup Scene\"\n}\n",
                                   scene_dir,
                                   sizeof(scene_dir),
                                   runtime_path,
                                   sizeof(runtime_path),
                                   authoring_path,
                                   sizeof(authoring_path))) {
        cleanup_scene_contract_dir(scene_dir, runtime_path, authoring_path);
        remove_dir_if_exists(root_dir);
        return false;
    }

    physics_sim_editor_scene_library_refresh(&library,
                                             &preset,
                                             &session,
                                             root_dir,
                                             runtime_path);
    index = physics_sim_editor_scene_library_find_retained_index_by_path(&library, runtime_path);

    cleanup_scene_contract_dir(scene_dir, runtime_path, authoring_path);
    remove_dir_if_exists(root_dir);

    if (index < 0) return false;
    return strcmp(library.retained_scenes.entries[index].source_path, runtime_path) == 0;
}

static bool test_scene_library_prefers_exact_current_path_for_duplicate_scene_ids(void) {
    PhysicsSimEditorSceneLibrary library = {0};
    PhysicsSimEditorSession session = {0};
    FluidScenePreset preset = {0};
    char root_dir[512];
    char scene_dir_a[512];
    char scene_dir_b[512];
    char runtime_path_a[512];
    char runtime_path_b[512];
    char authoring_path_a[512];
    char authoring_path_b[512];
    int matching_scene_id_count = 0;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"duplicate_scene_id_contract\","
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

    snprintf(root_dir, sizeof(root_dir), "%s", "data/runtime/scene_library_contract_duplicate_root");
    if (!create_scene_contract_dir(root_dir,
                                   "Duplicate A",
                                   runtime_json,
                                   "{\n  \"scene_name\": \"Duplicate A\"\n}\n",
                                   scene_dir_a,
                                   sizeof(scene_dir_a),
                                   runtime_path_a,
                                   sizeof(runtime_path_a),
                                   authoring_path_a,
                                   sizeof(authoring_path_a)) ||
        !create_scene_contract_dir(root_dir,
                                   "Duplicate B",
                                   runtime_json,
                                   "{\n  \"scene_name\": \"Duplicate B\"\n}\n",
                                   scene_dir_b,
                                   sizeof(scene_dir_b),
                                   runtime_path_b,
                                   sizeof(runtime_path_b),
                                   authoring_path_b,
                                   sizeof(authoring_path_b))) {
        cleanup_scene_contract_dir(scene_dir_a, runtime_path_a, authoring_path_a);
        cleanup_scene_contract_dir(scene_dir_b, runtime_path_b, authoring_path_b);
        remove_dir_if_exists(root_dir);
        return false;
    }

    physics_sim_editor_scene_library_refresh(&library,
                                             &preset,
                                             &session,
                                             root_dir,
                                             runtime_path_b);
    for (int i = 0; i < library.retained_scenes.count; ++i) {
        const PhysicsSimSceneLibraryEntry *entry = &library.retained_scenes.entries[i];
        if (strcmp(entry->scene_id, "duplicate_scene_id_contract") == 0) {
            matching_scene_id_count++;
            if (strcmp(entry->source_path, runtime_path_b) != 0) {
                cleanup_scene_contract_dir(scene_dir_a, runtime_path_a, authoring_path_a);
                cleanup_scene_contract_dir(scene_dir_b, runtime_path_b, authoring_path_b);
                remove_dir_if_exists(root_dir);
                return false;
            }
        }
    }

    cleanup_scene_contract_dir(scene_dir_a, runtime_path_a, authoring_path_a);
    cleanup_scene_contract_dir(scene_dir_b, runtime_path_b, authoring_path_b);
    remove_dir_if_exists(root_dir);
    return matching_scene_id_count == 1;
}

static bool test_scene_library_ignores_flat_runtime_json_in_input_root(void) {
    PhysicsSimEditorSceneLibrary library = {0};
    PhysicsSimEditorSession session = {0};
    FluidScenePreset preset = {0};
    char root_dir[512];
    char runtime_path[512];
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"flat_runtime_scene\","
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

    snprintf(root_dir, sizeof(root_dir), "%s", "data/runtime/scene_library_contract_flat_runtime_root");
    snprintf(runtime_path, sizeof(runtime_path), "%s/%s", root_dir, "flat_runtime_scene.json");
    if (!ensure_dir(root_dir) || !write_text_file(runtime_path, runtime_json)) {
        remove_if_exists(runtime_path);
        remove_dir_if_exists(root_dir);
        return false;
    }

    physics_sim_editor_scene_library_refresh(&library,
                                             &preset,
                                             &session,
                                             root_dir,
                                             runtime_path);

    remove_if_exists(runtime_path);
    remove_dir_if_exists(root_dir);
    return library.retained_scenes.count == 0;
}

static bool test_scene_library_discovers_grouped_scene_directories(void) {
    PhysicsSimEditorSceneLibrary library = {0};
    PhysicsSimEditorSession session = {0};
    FluidScenePreset preset = {0};
    char root_dir[512];
    char group_dir[512];
    char grouped_scene_dir[512];
    char grouped_runtime_path[512];
    char grouped_authoring_path[512];
    const char *runtime_json_grouped =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"grouped_scene_contract\","
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
    const char *authoring_json_grouped =
        "{"
        "  \"scene_name\": \"Harbor Setup\"\n"
        "}";

    snprintf(root_dir, sizeof(root_dir), "%s", "data/runtime/scene_library_contract_group_root");
    snprintf(group_dir, sizeof(group_dir), "%s/%s", root_dir, "Harbor");
    if (!ensure_dir(root_dir) ||
        !ensure_dir(group_dir) ||
        !create_scene_contract_dir(group_dir,
                                   "harbor_setup_scene",
                                   runtime_json_grouped,
                                   authoring_json_grouped,
                                   grouped_scene_dir,
                                   sizeof(grouped_scene_dir),
                                   grouped_runtime_path,
                                   sizeof(grouped_runtime_path),
                                   grouped_authoring_path,
                                   sizeof(grouped_authoring_path))) {
        cleanup_scene_contract_dir(grouped_scene_dir, grouped_runtime_path, grouped_authoring_path);
        remove_dir_if_exists(group_dir);
        remove_dir_if_exists(root_dir);
        return false;
    }

    physics_sim_editor_scene_library_refresh(&library,
                                             &preset,
                                             &session,
                                             root_dir,
                                             grouped_runtime_path);

    cleanup_scene_contract_dir(grouped_scene_dir, grouped_runtime_path, grouped_authoring_path);
    remove_dir_if_exists(group_dir);
    remove_dir_if_exists(root_dir);

    if (library.retained_scenes.count != 1) return false;
    if (strcmp(library.retained_scenes.entries[0].source_path, grouped_runtime_path) != 0) return false;
    if (strcmp(library.retained_scenes.entries[0].display_name, "Harbor Setup") != 0) return false;
    return strcmp(library.retained_scenes.entries[0].group_name, "Harbor") == 0;
}

int main(void) {
    if (!test_scene_library_splits_legacy_and_retained_catalogs()) {
        fprintf(stderr, "scene_editor_scene_library_contract_test: catalog split failed\n");
        return 1;
    }
    if (!test_scene_library_requires_authoring_and_runtime_pair()) {
        fprintf(stderr, "scene_editor_scene_library_contract_test: paired scene contract failed\n");
        return 1;
    }
    if (!test_scene_library_can_find_retained_row_by_exact_path()) {
        fprintf(stderr, "scene_editor_scene_library_contract_test: path lookup failed\n");
        return 1;
    }
    if (!test_scene_library_prefers_exact_current_path_for_duplicate_scene_ids()) {
        fprintf(stderr, "scene_editor_scene_library_contract_test: duplicate scene-id preference failed\n");
        return 1;
    }
    if (!test_scene_library_ignores_flat_runtime_json_in_input_root()) {
        fprintf(stderr, "scene_editor_scene_library_contract_test: flat runtime json filtering failed\n");
        return 1;
    }
    if (!test_scene_library_discovers_grouped_scene_directories()) {
        fprintf(stderr, "scene_editor_scene_library_contract_test: grouped discovery failed\n");
        return 1;
    }
    fprintf(stdout, "scene_editor_scene_library_contract_test: success\n");
    return 0;
}
