#include "app/editor/scene_editor_scene_library.h"
#include "app/data_paths.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void scene_library_catalog_clear(PhysicsSimSceneLibraryCatalog *catalog) {
    if (!catalog) return;
    memset(catalog, 0, sizeof(*catalog));
    catalog->selected_index = -1;
}

static bool scene_library_path_is_under_dir(const char *dir, const char *path) {
    size_t dir_len = 0;
    if (!dir || !dir[0] || !path || !path[0]) return false;
    dir_len = strlen(dir);
    if (strncmp(dir, path, dir_len) != 0) return false;
    return path[dir_len] == '\0' || path[dir_len] == '/';
}

static long long scene_library_path_modified_unix(const char *path) {
    struct stat st = {0};
    if (!path || !path[0]) return 0;
    if (stat(path, &st) != 0) return 0;
    return (long long)st.st_mtime;
}

static bool scene_library_has_json_suffix(const char *name) {
    size_t len = 0;
    if (!name) return false;
    len = strlen(name);
    if (len < 6u) return false;
    return strcmp(name + len - 5u, ".json") == 0;
}

static void scene_library_display_name_from_filename(const char *filename,
                                                     char *out,
                                                     size_t out_size) {
    size_t len = 0;
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!filename) return;
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

static bool scene_library_entry_load_scene_id(const char *path,
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

static int scene_library_entry_compare_by_name(const void *lhs, const void *rhs) {
    const PhysicsSimSceneLibraryEntry *a = (const PhysicsSimSceneLibraryEntry *)lhs;
    const PhysicsSimSceneLibraryEntry *b = (const PhysicsSimSceneLibraryEntry *)rhs;
    int by_name = strcmp(a->display_name, b->display_name);
    if (by_name != 0) return by_name;
    return strcmp(a->source_path, b->source_path);
}

static bool scene_library_entry_should_replace(const PhysicsSimSceneLibraryEntry *existing,
                                               const PhysicsSimSceneLibraryEntry *candidate,
                                               const char *current_runtime_scene_path) {
    if (!existing || !candidate) return false;
    if (current_runtime_scene_path && current_runtime_scene_path[0]) {
        bool existing_current = strcmp(existing->source_path, current_runtime_scene_path) == 0;
        bool candidate_current = strcmp(candidate->source_path, current_runtime_scene_path) == 0;
        if (existing_current != candidate_current) return candidate_current;
    }
    if (existing->user_scene != candidate->user_scene) return candidate->user_scene;
    if (existing->modified_unix != candidate->modified_unix) {
        return candidate->modified_unix > existing->modified_unix;
    }
    return strcmp(candidate->display_name, existing->display_name) < 0;
}

static void scene_library_insert_entry_sorted(PhysicsSimSceneLibraryCatalog *catalog,
                                              const PhysicsSimSceneLibraryEntry *entry,
                                              const char *current_runtime_scene_path) {
    int existing_index = -1;
    if (!catalog || !entry) return;
    for (int i = 0; i < catalog->count; ++i) {
        if (strcmp(catalog->entries[i].source_path, entry->source_path) == 0) {
            existing_index = i;
            break;
        }
        if (entry->scene_id[0] &&
            catalog->entries[i].scene_id[0] &&
            strcmp(catalog->entries[i].scene_id, entry->scene_id) == 0) {
            existing_index = i;
            break;
        }
    }
    if (existing_index >= 0) {
        if (strcmp(catalog->entries[existing_index].source_path, entry->source_path) == 0 ||
            scene_library_entry_should_replace(&catalog->entries[existing_index],
                                               entry,
                                               current_runtime_scene_path)) {
            catalog->entries[existing_index] = *entry;
        }
        return;
    }
    if (catalog->count >= PHYSICS_SIM_SCENE_LIBRARY_MAX_ENTRIES) return;
    catalog->entries[catalog->count] = *entry;
    catalog->count++;
}

static bool scene_library_entry_is_active(const PhysicsSimEditorSession *session,
                                          const char *current_runtime_scene_path,
                                          const PhysicsSimSceneLibraryEntry *entry) {
    if (!entry) return false;
    if (current_runtime_scene_path && current_runtime_scene_path[0]) {
        return strcmp(current_runtime_scene_path, entry->source_path) == 0;
    }
    if (session && session->has_retained_scene &&
        session->retained_scene.root.scene_id[0] &&
        entry->scene_id[0]) {
        return strcmp(session->retained_scene.root.scene_id, entry->scene_id) == 0;
    }
    return false;
}

static void scene_library_seed_legacy_catalog(PhysicsSimSceneLibraryCatalog *catalog,
                                              const FluidScenePreset *working_preset) {
    PhysicsSimSceneLibraryEntry entry = {0};
    if (!catalog || !working_preset) return;
    entry.kind = PHYSICS_SIM_SCENE_LIBRARY_ENTRY_LEGACY_PRESET;
    entry.active = true;
    entry.object_count = (int)working_preset->object_count;
    snprintf(entry.display_name,
             sizeof(entry.display_name),
             "%s",
             (working_preset->name && working_preset->name[0]) ? working_preset->name : "Current 2D Preset");
    snprintf(entry.scene_id, sizeof(entry.scene_id), "%s", entry.display_name);
    catalog->entries[0] = entry;
    catalog->count = 1;
    catalog->selected_index = 0;
}

static void scene_library_seed_retained_catalog(PhysicsSimSceneLibraryCatalog *catalog,
                                                const PhysicsSimEditorSession *session,
                                                const char *runtime_scene_dir,
                                                const char *current_runtime_scene_path) {
    const char *runtime_scene_user_dir = physics_sim_default_runtime_scene_user_dir();
    DIR *dir = NULL;
    struct dirent *dent = NULL;
    if (!catalog) return;

    if (runtime_scene_dir && runtime_scene_dir[0]) {
        dir = opendir(runtime_scene_dir);
        if (dir) {
            while ((dent = readdir(dir)) != NULL) {
                PhysicsSimSceneLibraryEntry entry = {0};
                if (dent->d_name[0] == '.') continue;
                if (!scene_library_has_json_suffix(dent->d_name)) continue;
                entry.kind = PHYSICS_SIM_SCENE_LIBRARY_ENTRY_RUNTIME_SCENE;
                entry.object_count = -1;
                scene_library_display_name_from_filename(dent->d_name,
                                                         entry.display_name,
                                                         sizeof(entry.display_name));
                snprintf(entry.source_path, sizeof(entry.source_path), "%s/%s", runtime_scene_dir, dent->d_name);
                entry.user_scene = scene_library_path_is_under_dir(runtime_scene_user_dir, entry.source_path);
                entry.modified_unix = scene_library_path_modified_unix(entry.source_path);
                if (!scene_library_entry_load_scene_id(entry.source_path,
                                                       entry.scene_id,
                                                       sizeof(entry.scene_id))) {
                    snprintf(entry.scene_id, sizeof(entry.scene_id), "%s", entry.display_name);
                }
                entry.active = scene_library_entry_is_active(session,
                                                             current_runtime_scene_path,
                                                             &entry);
                scene_library_insert_entry_sorted(catalog, &entry, current_runtime_scene_path);
            }
            closedir(dir);
        }
    }

    if (runtime_scene_user_dir && runtime_scene_user_dir[0] &&
        (!runtime_scene_dir || strcmp(runtime_scene_user_dir, runtime_scene_dir) != 0)) {
        dir = opendir(runtime_scene_user_dir);
        if (dir) {
            while ((dent = readdir(dir)) != NULL) {
                PhysicsSimSceneLibraryEntry entry = {0};
                if (dent->d_name[0] == '.') continue;
                if (!scene_library_has_json_suffix(dent->d_name)) continue;
                entry.kind = PHYSICS_SIM_SCENE_LIBRARY_ENTRY_RUNTIME_SCENE;
                entry.object_count = -1;
                scene_library_display_name_from_filename(dent->d_name,
                                                         entry.display_name,
                                                         sizeof(entry.display_name));
                snprintf(entry.source_path, sizeof(entry.source_path), "%s/%s", runtime_scene_user_dir, dent->d_name);
                entry.user_scene = true;
                entry.modified_unix = scene_library_path_modified_unix(entry.source_path);
                if (!scene_library_entry_load_scene_id(entry.source_path,
                                                       entry.scene_id,
                                                       sizeof(entry.scene_id))) {
                    snprintf(entry.scene_id, sizeof(entry.scene_id), "%s", entry.display_name);
                }
                entry.active = scene_library_entry_is_active(session,
                                                             current_runtime_scene_path,
                                                             &entry);
                scene_library_insert_entry_sorted(catalog, &entry, current_runtime_scene_path);
            }
            closedir(dir);
        }
    }

    if (catalog->count <= 0) {
        catalog->selected_index = -1;
        return;
    }
    qsort(catalog->entries,
          (size_t)catalog->count,
          sizeof(catalog->entries[0]),
          scene_library_entry_compare_by_name);
    for (int i = 0; i < catalog->count; ++i) {
        if (catalog->entries[i].active) {
            catalog->selected_index = i;
            return;
        }
    }
    catalog->selected_index = 0;
}

void physics_sim_editor_scene_library_refresh(PhysicsSimEditorSceneLibrary *library,
                                              const FluidScenePreset *working_preset,
                                              const PhysicsSimEditorSession *session,
                                              const char *runtime_scene_dir,
                                              const char *current_runtime_scene_path) {
    if (!library) return;
    memset(library, 0, sizeof(*library));
    scene_library_catalog_clear(&library->legacy_presets);
    scene_library_catalog_clear(&library->retained_scenes);
    scene_library_seed_legacy_catalog(&library->legacy_presets, working_preset);
    scene_library_seed_retained_catalog(&library->retained_scenes,
                                        session,
                                        runtime_scene_dir,
                                        current_runtime_scene_path);
    library->mode = (session && session->has_retained_scene)
                        ? PHYSICS_SIM_SCENE_LIBRARY_MODE_3D
                        : PHYSICS_SIM_SCENE_LIBRARY_MODE_2D;
}

const PhysicsSimSceneLibraryEntry *physics_sim_editor_scene_library_selected_legacy(const PhysicsSimEditorSceneLibrary *library) {
    if (!library) return NULL;
    if (library->legacy_presets.selected_index < 0 ||
        library->legacy_presets.selected_index >= library->legacy_presets.count) {
        return NULL;
    }
    return &library->legacy_presets.entries[library->legacy_presets.selected_index];
}

const PhysicsSimSceneLibraryEntry *physics_sim_editor_scene_library_selected_retained(const PhysicsSimEditorSceneLibrary *library) {
    if (!library) return NULL;
    if (library->retained_scenes.selected_index < 0 ||
        library->retained_scenes.selected_index >= library->retained_scenes.count) {
        return NULL;
    }
    return &library->retained_scenes.entries[library->retained_scenes.selected_index];
}

bool physics_sim_editor_scene_library_has_retained_entries(const PhysicsSimEditorSceneLibrary *library) {
    return library && library->retained_scenes.count > 0;
}

int physics_sim_editor_scene_library_find_retained_index_by_path(const PhysicsSimEditorSceneLibrary *library,
                                                                 const char *source_path) {
    if (!library || !source_path || !source_path[0]) return -1;
    for (int i = 0; i < library->retained_scenes.count; ++i) {
        if (strcmp(library->retained_scenes.entries[i].source_path, source_path) == 0) {
            return i;
        }
    }
    return -1;
}

const char *physics_sim_editor_scene_library_mode_label(PhysicsSimSceneLibraryMode mode) {
    return mode == PHYSICS_SIM_SCENE_LIBRARY_MODE_3D ? "3D Scenes" : "2D Presets";
}
