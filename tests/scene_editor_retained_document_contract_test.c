#include "app/editor/scene_editor_retained_document.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

static bool write_text_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    if (fwrite(text, 1, strlen(text), f) != strlen(text)) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

static bool read_text_file(const char *path, char *out, size_t out_size) {
    FILE *f = NULL;
    size_t size = 0;
    if (!path || !out || out_size == 0) return false;
    f = fopen(path, "rb");
    if (!f) return false;
    size = fread(out, 1, out_size - 1u, f);
    fclose(f);
    out[size] = '\0';
    return true;
}

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
    bool ok = scene_editor_retained_document_resolve_save_path("/tmp/physics_sim_retained_doc_empty",
                                                               "config/samples/ps4d_runtime_scene_visual_test.json",
                                                               "custom_scene_name",
                                                               "scene_unique_contract_test",
                                                               path,
                                                               sizeof(path));
    if (!ok) return false;
    return strcmp(path, "/tmp/physics_sim_retained_doc_empty/custom_scene_name.json") == 0;
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

static bool test_save_path_reuses_existing_runtime_scene_id_file(void) {
    char temp_dir[256];
    char existing_path[320];
    char resolved_path[320];
    int pid = (int)getpid();
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/physics_sim_retained_doc_%d", pid);
    (void)mkdir(temp_dir, 0755);
    snprintf(existing_path, sizeof(existing_path), "%s/%s", temp_dir, "older_scene_name.json");
    if (!write_text_file(existing_path, "{\n  \"scene_id\": \"scene_ps4d_visual_test\"\n}\n")) {
        return false;
    }
    if (!scene_editor_retained_document_resolve_save_path(temp_dir,
                                                          "config/samples/ps4d_runtime_scene_visual_test.json",
                                                          "ps4d_runtime_scene_visual_test",
                                                          "scene_ps4d_visual_test",
                                                          resolved_path,
                                                          sizeof(resolved_path))) {
        remove(existing_path);
        rmdir(temp_dir);
        return false;
    }
    remove(existing_path);
    rmdir(temp_dir);
    return strcmp(resolved_path, existing_path) == 0;
}

static bool test_duplicate_scene_file_creates_copy_with_new_scene_id(void) {
    char temp_dir[256];
    char source_path[320];
    char duplicate_path[320];
    char duplicate_text[512];
    char diagnostics[256];
    int pid = (int)getpid();
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/physics_sim_retained_doc_dup_%d", pid);
    (void)mkdir(temp_dir, 0755);
    snprintf(source_path, sizeof(source_path), "%s/%s", temp_dir, "scene_ps4d_visual_test.json");
    if (!write_text_file(source_path, "{\n  \"scene_id\": \"scene_ps4d_visual_test\",\n  \"objects\": []\n}\n")) {
        return false;
    }
    if (!scene_editor_retained_document_duplicate_scene_file(source_path,
                                                             temp_dir,
                                                             duplicate_path,
                                                             sizeof(duplicate_path),
                                                             diagnostics,
                                                             sizeof(diagnostics))) {
        remove(source_path);
        rmdir(temp_dir);
        return false;
    }
    if (!read_text_file(duplicate_path, duplicate_text, sizeof(duplicate_text))) {
        remove(duplicate_path);
        remove(source_path);
        rmdir(temp_dir);
        return false;
    }
    remove(duplicate_path);
    remove(source_path);
    rmdir(temp_dir);
    return strstr(duplicate_path, "scene_ps4d_visual_test_copy.json") != NULL &&
           strstr(duplicate_text, "\"scene_id\": \"scene_ps4d_visual_test_copy\"") != NULL;
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
    if (!test_save_path_reuses_existing_runtime_scene_id_file()) {
        fprintf(stderr, "scene_editor_retained_document_contract_test: existing scene-id save path reuse failed\n");
        return 1;
    }
    if (!test_duplicate_scene_file_creates_copy_with_new_scene_id()) {
        fprintf(stderr, "scene_editor_retained_document_contract_test: duplicate scene file failed\n");
        return 1;
    }
    if (!test_runtime_user_path_detection_handles_absolute_paths()) {
        fprintf(stderr, "scene_editor_retained_document_contract_test: runtime path detection failed\n");
        return 1;
    }
    fprintf(stdout, "scene_editor_retained_document_contract_test: success\n");
    return 0;
}
