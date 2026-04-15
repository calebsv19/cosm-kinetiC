#include "app/editor/scene_editor_scene_library.h"
#include "app/data_paths.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

static void scene_library_catalog_clear(PhysicsSimSceneLibraryCatalog *catalog) {
    if (!catalog) return;
    memset(catalog, 0, sizeof(*catalog));
    catalog->selected_index = -1;
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

static void scene_library_insert_entry_sorted(PhysicsSimSceneLibraryCatalog *catalog,
                                              const PhysicsSimSceneLibraryEntry *entry) {
    int existing_index = -1;
    int insert_index = 0;
    if (!catalog || !entry) return;
    for (int i = 0; i < catalog->count; ++i) {
        if (strcmp(catalog->entries[i].source_path, entry->source_path) == 0) {
            existing_index = i;
            break;
        }
    }
    if (existing_index >= 0) {
        catalog->entries[existing_index] = *entry;
        if (catalog->selected_index == existing_index || entry->active) {
            catalog->selected_index = existing_index;
        }
        return;
    }
    if (catalog->count >= PHYSICS_SIM_SCENE_LIBRARY_MAX_ENTRIES) return;
    insert_index = catalog->count;
    while (insert_index > 0 &&
           strcmp(entry->display_name, catalog->entries[insert_index - 1].display_name) < 0) {
        catalog->entries[insert_index] = catalog->entries[insert_index - 1];
        insert_index--;
    }
    catalog->entries[insert_index] = *entry;
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
                snprintf(entry.scene_id, sizeof(entry.scene_id), "%s", entry.display_name);
                snprintf(entry.source_path, sizeof(entry.source_path), "%s/%s", runtime_scene_dir, dent->d_name);
                entry.active = scene_library_entry_is_active(session,
                                                             current_runtime_scene_path,
                                                             &entry);
                scene_library_insert_entry_sorted(catalog, &entry);
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
                snprintf(entry.scene_id, sizeof(entry.scene_id), "%s", entry.display_name);
                snprintf(entry.source_path, sizeof(entry.source_path), "%s/%s", runtime_scene_user_dir, dent->d_name);
                entry.active = scene_library_entry_is_active(session,
                                                             current_runtime_scene_path,
                                                             &entry);
                scene_library_insert_entry_sorted(catalog, &entry);
            }
            closedir(dir);
        }
    }

    if (catalog->count <= 0) {
        catalog->selected_index = -1;
        return;
    }
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
