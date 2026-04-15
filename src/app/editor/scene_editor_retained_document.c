#include "app/editor/scene_editor_retained_document.h"

#include <stdio.h>
#include <string.h>

static void scene_editor_sanitize_runtime_scene_name(const char *input,
                                                     char *out,
                                                     size_t out_size) {
    size_t write_index = 0;
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!input || !input[0]) {
        snprintf(out, out_size, "%s", "scene");
        return;
    }
    for (size_t i = 0; input[i] != '\0' && write_index + 1 < out_size; ++i) {
        unsigned char ch = (unsigned char)input[i];
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9')) {
            out[write_index++] = (char)ch;
        } else if (ch == '_' || ch == '-') {
            out[write_index++] = (char)ch;
        } else if (ch == ' ' || ch == '.') {
            out[write_index++] = '_';
        }
    }
    out[write_index] = '\0';
    if (out[0] == '\0') {
        snprintf(out, out_size, "%s", "scene");
    }
}

void scene_editor_retained_document_name_from_path(const char *runtime_scene_path,
                                                   const char *provenance_scene_id,
                                                   char *out_name,
                                                   size_t out_name_size) {
    const char *filename = NULL;
    size_t len = 0;
    if (!out_name || out_name_size == 0) return;
    out_name[0] = '\0';

    if (runtime_scene_path && runtime_scene_path[0]) {
        filename = strrchr(runtime_scene_path, '/');
        filename = filename ? filename + 1 : runtime_scene_path;
        len = strlen(filename);
        if (len > 5u && strcmp(filename + len - 5u, ".json") == 0) {
            len -= 5u;
        }
        if (len > 0) {
            if (len + 1u > out_name_size) {
                len = out_name_size - 1u;
            }
            memcpy(out_name, filename, len);
            out_name[len] = '\0';
            return;
        }
    }

    scene_editor_sanitize_runtime_scene_name(provenance_scene_id, out_name, out_name_size);
}

bool scene_editor_retained_document_resolve_save_path(const char *runtime_dir,
                                                      const char *current_document_path,
                                                      const char *document_name,
                                                      const char *provenance_scene_id,
                                                      char *out_path,
                                                      size_t out_path_size) {
    char scene_name[128];
    const char *save_key = NULL;
    if (!runtime_dir || !runtime_dir[0] || !out_path || out_path_size == 0) return false;
    out_path[0] = '\0';

    if (scene_editor_retained_document_is_runtime_user_path(runtime_dir, current_document_path)) {
        snprintf(out_path, out_path_size, "%s", current_document_path);
        return true;
    }

    save_key = (document_name && document_name[0]) ? document_name : provenance_scene_id;
    scene_editor_sanitize_runtime_scene_name(save_key, scene_name, sizeof(scene_name));
    if (snprintf(out_path, out_path_size, "%s/%s.json", runtime_dir, scene_name) >= (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

bool scene_editor_retained_document_is_runtime_user_path(const char *runtime_dir,
                                                         const char *path) {
    size_t runtime_dir_len = 0;
    if (!runtime_dir || !runtime_dir[0] || !path || !path[0]) return false;
    runtime_dir_len = strlen(runtime_dir);
    if (strncmp(path, runtime_dir, runtime_dir_len) != 0) return false;
    return path[runtime_dir_len] == '\0' || path[runtime_dir_len] == '/';
}
