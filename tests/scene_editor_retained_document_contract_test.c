#include "app/editor/scene_editor_retained_document.h"

#include <stdio.h>
#include <string.h>

static bool test_document_name_prefers_path_stem_over_scene_id(void) {
    char name[128];
    scene_editor_retained_document_name_from_path("config/samples/ps4d_runtime_scene_visual_test.json",
                                                  "scene_ps4d_visual_test",
                                                  name,
                                                  sizeof(name));
    return strcmp(name, "ps4d_runtime_scene_visual_test") == 0;
}

static bool test_save_path_prefers_document_name_over_provenance_scene_id(void) {
    char path[256];
    bool ok = scene_editor_retained_document_resolve_save_path("data/runtime/scenes",
                                                               "config/samples/ps4d_runtime_scene_visual_test.json",
                                                               "custom_scene_name",
                                                               "scene_ps4d_visual_test",
                                                               path,
                                                               sizeof(path));
    if (!ok) return false;
    return strcmp(path, "data/runtime/scenes/custom_scene_name.json") == 0;
}

static bool test_save_path_reuses_current_runtime_document_path(void) {
    char path[256];
    bool ok = scene_editor_retained_document_resolve_save_path("data/runtime/scenes",
                                                               "data/runtime/scenes/already_saved.json",
                                                               "renamed_scene",
                                                               "scene_ps4d_visual_test",
                                                               path,
                                                               sizeof(path));
    if (!ok) return false;
    return strcmp(path, "data/runtime/scenes/already_saved.json") == 0;
}

static bool test_runtime_user_path_detection_handles_absolute_paths(void) {
    return scene_editor_retained_document_is_runtime_user_path("/repo/data/runtime/scenes",
                                                               "/repo/data/runtime/scenes/custom_scene.json") &&
           !scene_editor_retained_document_is_runtime_user_path("/repo/data/runtime/scenes",
                                                                "/repo/config/samples/ps4d_runtime_scene_visual_test.json");
}

int main(void) {
    if (!test_document_name_prefers_path_stem_over_scene_id()) {
        fprintf(stderr, "scene_editor_retained_document_contract_test: document name seed failed\n");
        return 1;
    }
    if (!test_save_path_prefers_document_name_over_provenance_scene_id()) {
        fprintf(stderr, "scene_editor_retained_document_contract_test: save path resolution failed\n");
        return 1;
    }
    if (!test_save_path_reuses_current_runtime_document_path()) {
        fprintf(stderr, "scene_editor_retained_document_contract_test: runtime path reuse failed\n");
        return 1;
    }
    if (!test_runtime_user_path_detection_handles_absolute_paths()) {
        fprintf(stderr, "scene_editor_retained_document_contract_test: runtime path detection failed\n");
        return 1;
    }
    fprintf(stdout, "scene_editor_retained_document_contract_test: success\n");
    return 0;
}
