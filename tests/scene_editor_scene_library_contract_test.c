#include "app/editor/scene_editor_scene_library.h"
#include "app/data_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool test_scene_library_splits_legacy_and_retained_catalogs(void) {
    PhysicsSimEditorSceneLibrary library = {0};
    PhysicsSimEditorSession session = {0};
    FluidScenePreset preset = {0};
    const PhysicsSimSceneLibraryEntry *legacy = NULL;
    const PhysicsSimSceneLibraryEntry *retained_entry = NULL;

    preset.name = "Tunnel Draft";
    session.has_retained_scene = true;
    snprintf(session.retained_scene.root.scene_id,
             sizeof(session.retained_scene.root.scene_id),
             "%s",
             "ps4d_runtime_scene_visual_test");

    physics_sim_editor_scene_library_refresh(&library,
                                             &preset,
                                             &session,
                                             "config/samples",
                                             "config/samples/ps4d_runtime_scene_visual_test.json");

    if (library.mode != PHYSICS_SIM_SCENE_LIBRARY_MODE_3D) return false;
    legacy = physics_sim_editor_scene_library_selected_legacy(&library);
    retained_entry = physics_sim_editor_scene_library_selected_retained(&library);
    if (!legacy) return false;
    if (!retained_entry) return false;
    if (strcmp(legacy->display_name, "Tunnel Draft") != 0) return false;
    if (strcmp(retained_entry->display_name, "ps4d_runtime_scene_visual_test") != 0) return false;
    if (!retained_entry->active) return false;
    if (!physics_sim_editor_scene_library_has_retained_entries(&library)) return false;
    return true;
}

static bool test_scene_library_prefers_saved_runtime_scene_for_active_duplicate(void) {
    PhysicsSimEditorSceneLibrary library = {0};
    PhysicsSimEditorSession session = {0};
    FluidScenePreset preset = {0};
    const PhysicsSimSceneLibraryEntry *retained_entry = NULL;
    const char *user_dir = physics_sim_default_runtime_scene_user_dir();
    char saved_path[512];
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"ps4d_runtime_scene_visual_test\","
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

    if (!physics_sim_ensure_runtime_dirs()) return false;
    if (snprintf(saved_path,
                 sizeof(saved_path),
                 "%s/%s",
                 user_dir,
                 "ps4d_runtime_scene_visual_test.json") >= (int)sizeof(saved_path)) {
        return false;
    }
    if (!write_text_file(saved_path, runtime_json)) {
        remove(saved_path);
        return false;
    }

    preset.name = "Tunnel Draft";
    session.has_retained_scene = true;
    snprintf(session.retained_scene.root.scene_id,
             sizeof(session.retained_scene.root.scene_id),
             "%s",
             "ps4d_runtime_scene_visual_test");

    physics_sim_editor_scene_library_refresh(&library,
                                             &preset,
                                             &session,
                                             "config/samples",
                                             saved_path);
    if (library.retained_scenes.count < 2) {
        remove(saved_path);
        return false;
    }
    retained_entry = physics_sim_editor_scene_library_selected_retained(&library);
    if (!retained_entry) {
        remove(saved_path);
        return false;
    }
    if (!retained_entry->active) {
        remove(saved_path);
        return false;
    }
    if (strcmp(retained_entry->source_path, saved_path) != 0) {
        remove(saved_path);
        return false;
    }
    {
        bool found_sample = false;
        bool found_saved = false;
        for (int i = 0; i < library.retained_scenes.count; ++i) {
            const PhysicsSimSceneLibraryEntry *entry = &library.retained_scenes.entries[i];
            if (strcmp(entry->source_path, saved_path) == 0) found_saved = true;
            if (strcmp(entry->source_path, "config/samples/ps4d_runtime_scene_visual_test.json") == 0) {
                found_sample = true;
            }
        }
        if (!found_sample || !found_saved) {
            remove(saved_path);
            return false;
        }
    }

    remove(saved_path);
    return true;
}

static bool test_scene_library_can_find_retained_row_by_exact_path(void) {
    PhysicsSimEditorSceneLibrary library = {0};
    PhysicsSimEditorSession session = {0};
    FluidScenePreset preset = {0};
    const char *saved_path = "config/samples/ps4d_runtime_scene_visual_test.json";
    int index = -1;

    preset.name = "Tunnel Draft";
    session.has_retained_scene = true;
    snprintf(session.retained_scene.root.scene_id,
             sizeof(session.retained_scene.root.scene_id),
             "%s",
             "ps4d_runtime_scene_visual_test");

    physics_sim_editor_scene_library_refresh(&library,
                                             &preset,
                                             &session,
                                             "config/samples",
                                             saved_path);
    index = physics_sim_editor_scene_library_find_retained_index_by_path(&library, saved_path);
    if (index < 0) return false;
    if (strcmp(library.retained_scenes.entries[index].source_path, saved_path) != 0) return false;
    return true;
}

int main(void) {
    if (!test_scene_library_splits_legacy_and_retained_catalogs()) {
        fprintf(stderr, "scene_editor_scene_library_contract_test: catalog split failed\n");
        return 1;
    }
    if (!test_scene_library_prefers_saved_runtime_scene_for_active_duplicate()) {
        fprintf(stderr, "scene_editor_scene_library_contract_test: saved runtime scene preference failed\n");
        return 1;
    }
    if (!test_scene_library_can_find_retained_row_by_exact_path()) {
        fprintf(stderr, "scene_editor_scene_library_contract_test: path lookup failed\n");
        return 1;
    }
    fprintf(stdout, "scene_editor_scene_library_contract_test: success\n");
    return 0;
}
