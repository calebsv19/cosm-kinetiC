#include "app/editor/scene_editor_retained_document.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
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

static void scene_editor_filename_stem_from_path(const char *path,
                                                 char *out,
                                                 size_t out_size) {
    const char *filename = NULL;
    size_t len = 0;
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!path || !path[0]) return;
    filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    len = strlen(filename);
    if (len > 5u && strcmp(filename + len - 5u, ".json") == 0) {
        len -= 5u;
    }
    if (len + 1u > out_size) {
        len = out_size - 1u;
    }
    memcpy(out, filename, len);
    out[len] = '\0';
}

static bool scene_editor_load_scene_id_from_file(const char *path,
                                                 char *out_scene_id,
                                                 size_t out_scene_id_size) {
    FILE *f = NULL;
    long file_size = 0;
    char *json_text = NULL;
    const char *key = "\"scene_id\"";
    char *cursor = NULL;
    char *value = NULL;
    size_t value_len = 0;
    bool ok = false;

    if (!out_scene_id || out_scene_id_size == 0) return false;
    out_scene_id[0] = '\0';
    if (!path || !path[0]) return false;

    f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) goto cleanup;
    file_size = ftell(f);
    if (file_size <= 0) goto cleanup;
    if (fseek(f, 0, SEEK_SET) != 0) goto cleanup;

    json_text = (char *)malloc((size_t)file_size + 1u);
    if (!json_text) goto cleanup;
    if (fread(json_text, 1, (size_t)file_size, f) != (size_t)file_size) goto cleanup;
    json_text[file_size] = '\0';

    cursor = strstr(json_text, key);
    if (!cursor) goto cleanup;
    cursor += strlen(key);
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') cursor++;
    if (*cursor != ':') goto cleanup;
    cursor++;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') cursor++;
    if (*cursor != '\"') goto cleanup;
    cursor++;
    value = cursor;
    while (*cursor != '\0' && *cursor != '\"') {
        if (*cursor == '\\' && cursor[1] != '\0') {
            cursor += 2;
        } else {
            cursor++;
        }
    }
    if (*cursor != '\"') goto cleanup;
    value_len = (size_t)(cursor - value);
    if (value_len + 1u > out_scene_id_size) {
        value_len = out_scene_id_size - 1u;
    }
    memcpy(out_scene_id, value, value_len);
    out_scene_id[value_len] = '\0';
    ok = out_scene_id[0] != '\0';

cleanup:
    free(json_text);
    if (f) fclose(f);
    return ok;
}

static bool scene_editor_runtime_scene_find_existing_path_by_scene_id(const char *runtime_dir,
                                                                      const char *scene_id,
                                                                      char *out_path,
                                                                      size_t out_path_size) {
    DIR *dir = NULL;
    struct dirent *dent = NULL;
    char candidate_path[512];
    char candidate_scene_id[128];
    if (!runtime_dir || !runtime_dir[0] || !scene_id || !scene_id[0] || !out_path || out_path_size == 0) {
        return false;
    }
    out_path[0] = '\0';
    dir = opendir(runtime_dir);
    if (!dir) return false;
    while ((dent = readdir(dir)) != NULL) {
        size_t name_len = 0;
        if (dent->d_name[0] == '.') continue;
        name_len = strlen(dent->d_name);
        if (name_len < 6u || strcmp(dent->d_name + name_len - 5u, ".json") != 0) continue;
        if (snprintf(candidate_path, sizeof(candidate_path), "%s/%s", runtime_dir, dent->d_name) >= (int)sizeof(candidate_path)) {
            continue;
        }
        if (!scene_editor_load_scene_id_from_file(candidate_path,
                                                  candidate_scene_id,
                                                  sizeof(candidate_scene_id))) {
            continue;
        }
        if (strcmp(candidate_scene_id, scene_id) == 0) {
            snprintf(out_path, out_path_size, "%s", candidate_path);
            closedir(dir);
            return true;
        }
    }
    closedir(dir);
    return false;
}

static bool scene_editor_runtime_scene_path_exists(const char *path) {
    FILE *f = NULL;
    if (!path || !path[0]) return false;
    f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static bool scene_editor_runtime_scene_replace_scene_id_json(const char *json_text,
                                                             const char *scene_id,
                                                             char **out_json_text) {
    const char *key = "\"scene_id\"";
    const char *cursor = NULL;
    const char *value = NULL;
    const char *value_end = NULL;
    size_t prefix_len = 0;
    size_t scene_id_len = 0;
    size_t suffix_len = 0;
    size_t total_len = 0;
    char *updated = NULL;
    if (!json_text || !scene_id || !scene_id[0] || !out_json_text) return false;
    *out_json_text = NULL;
    cursor = strstr(json_text, key);
    if (!cursor) return false;
    cursor += strlen(key);
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') cursor++;
    if (*cursor != ':') return false;
    cursor++;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') cursor++;
    if (*cursor != '\"') return false;
    value = cursor + 1;
    value_end = value;
    while (*value_end != '\0' && *value_end != '\"') {
        if (*value_end == '\\' && value_end[1] != '\0') {
            value_end += 2;
        } else {
            value_end++;
        }
    }
    if (*value_end != '\"') return false;

    prefix_len = (size_t)(value - json_text);
    scene_id_len = strlen(scene_id);
    suffix_len = strlen(value_end);
    total_len = prefix_len + scene_id_len + suffix_len;
    updated = (char *)malloc(total_len + 1u);
    if (!updated) return false;
    memcpy(updated, json_text, prefix_len);
    memcpy(updated + prefix_len, scene_id, scene_id_len);
    memcpy(updated + prefix_len + scene_id_len, value_end, suffix_len + 1u);
    *out_json_text = updated;
    return true;
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

    if (scene_editor_runtime_scene_find_existing_path_by_scene_id(runtime_dir,
                                                                  provenance_scene_id,
                                                                  out_path,
                                                                  out_path_size)) {
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

bool scene_editor_retained_document_duplicate_scene_file(const char *source_path,
                                                         const char *runtime_dir,
                                                         char *out_path,
                                                         size_t out_path_size,
                                                         char *out_diagnostics,
                                                         size_t out_diagnostics_size) {
    FILE *f = NULL;
    long file_size = 0;
    char *json_text = NULL;
    char *updated_json = NULL;
    char source_stem[128];
    char source_scene_id[128];
    char duplicate_stem[128];
    char duplicate_scene_id[128];
    int suffix = 0;

    if (out_path && out_path_size > 0) out_path[0] = '\0';
    if (out_diagnostics && out_diagnostics_size > 0) out_diagnostics[0] = '\0';
    if (!source_path || !source_path[0] || !runtime_dir || !runtime_dir[0] || !out_path || out_path_size == 0) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics, out_diagnostics_size, "%s", "Duplicate target invalid.");
        }
        return false;
    }

    f = fopen(source_path, "rb");
    if (!f) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics, out_diagnostics_size, "%s", "Failed to open source scene.");
        }
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) goto file_error;
    file_size = ftell(f);
    if (file_size <= 0) goto file_error;
    if (fseek(f, 0, SEEK_SET) != 0) goto file_error;

    json_text = (char *)malloc((size_t)file_size + 1u);
    if (!json_text) goto file_error;
    if (fread(json_text, 1, (size_t)file_size, f) != (size_t)file_size) goto file_error;
    json_text[file_size] = '\0';
    fclose(f);
    f = NULL;

    scene_editor_filename_stem_from_path(source_path, source_stem, sizeof(source_stem));
    if (!scene_editor_load_scene_id_from_file(source_path, source_scene_id, sizeof(source_scene_id))) {
        scene_editor_sanitize_runtime_scene_name(source_stem, source_scene_id, sizeof(source_scene_id));
    }

    for (suffix = 0; suffix < 1000; ++suffix) {
        if (suffix == 0) {
            snprintf(duplicate_stem, sizeof(duplicate_stem), "%s_copy", source_stem);
            snprintf(duplicate_scene_id, sizeof(duplicate_scene_id), "%s_copy", source_scene_id);
        } else {
            snprintf(duplicate_stem, sizeof(duplicate_stem), "%s_copy_%d", source_stem, suffix + 1);
            snprintf(duplicate_scene_id, sizeof(duplicate_scene_id), "%s_copy_%d", source_scene_id, suffix + 1);
        }
        if (snprintf(out_path, out_path_size, "%s/%s.json", runtime_dir, duplicate_stem) >= (int)out_path_size) {
            if (out_diagnostics && out_diagnostics_size > 0) {
                snprintf(out_diagnostics, out_diagnostics_size, "%s", "Duplicate path too long.");
            }
            free(json_text);
            return false;
        }
        if (!scene_editor_runtime_scene_path_exists(out_path)) {
            break;
        }
    }
    if (suffix >= 1000) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics, out_diagnostics_size, "%s", "No duplicate scene slot available.");
        }
        free(json_text);
        out_path[0] = '\0';
        return false;
    }

    if (!scene_editor_runtime_scene_replace_scene_id_json(json_text, duplicate_scene_id, &updated_json)) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics, out_diagnostics_size, "%s", "Failed to rewrite scene id.");
        }
        free(json_text);
        out_path[0] = '\0';
        return false;
    }
    free(json_text);
    json_text = NULL;

    f = fopen(out_path, "wb");
    if (!f) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics, out_diagnostics_size, "%s", "Failed to create duplicate scene.");
        }
        free(updated_json);
        out_path[0] = '\0';
        return false;
    }
    if (fwrite(updated_json, 1, strlen(updated_json), f) != strlen(updated_json)) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics, out_diagnostics_size, "%s", "Failed to write duplicate scene.");
        }
        fclose(f);
        free(updated_json);
        out_path[0] = '\0';
        return false;
    }
    fclose(f);
    free(updated_json);
    if (out_diagnostics && out_diagnostics_size > 0) {
        snprintf(out_diagnostics, out_diagnostics_size, "Duplicated scene to %s.json", duplicate_stem);
    }
    return true;

file_error:
    if (out_diagnostics && out_diagnostics_size > 0) {
        snprintf(out_diagnostics, out_diagnostics_size, "%s", "Failed to read source scene.");
    }
    if (f) fclose(f);
    free(json_text);
    out_path[0] = '\0';
    return false;
}
