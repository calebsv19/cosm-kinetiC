#include "app/editor/scene_editor_import.h"
#include "app/data_paths.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

void scene_editor_refresh_import_files(SceneEditorState *state) {
    char objects_dir[512];
    char import_dir[512];
    const char *configured_root = NULL;
    const char *objects_scan_dir = NULL;
    const char *import_scan_dir = NULL;
    if (!state) return;
    state->import_file_count = 0;
    configured_root = physics_sim_resolve_input_root(state->cfg.input_root);
    objects_scan_dir = physics_sim_resolve_shape_asset_dir_for_root(configured_root,
                                                                    objects_dir,
                                                                    sizeof(objects_dir));
    import_scan_dir = physics_sim_resolve_import_dir_for_root(configured_root,
                                                              import_dir,
                                                              sizeof(import_dir));

    // Canonical assets under input-root objects lane (fallback: config/objects).
    DIR *dir = opendir(objects_scan_dir);
    if (dir) {
        struct dirent *ent = NULL;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            const char *name = ent->d_name;
            const char *ext = strrchr(name, '.');
            if (!ext || (strcasecmp(ext, ".json") != 0 &&
                         strcasecmp(ext, ".asset.json") != 0)) {
                continue;
            }
            if (state->import_file_count >= MAX_IMPORT_FILES) break;
            size_t len = strlen(name);
            if (len >= sizeof(state->import_files[0])) continue;
            snprintf(state->import_files[state->import_file_count],
                     sizeof(state->import_files[0]),
                     "%s/%s", objects_scan_dir, name);
            state->import_file_count++;
        }
        closedir(dir);
    }

    // Raw ShapeLib JSONs under input-root import lane (fallback: import/).
    if (state->import_file_count < MAX_IMPORT_FILES) {
        DIR *legacy = opendir(import_scan_dir);
        if (legacy) {
            struct dirent *ent = NULL;
            while ((ent = readdir(legacy)) != NULL) {
                if (ent->d_name[0] == '.') continue;
                const char *name = ent->d_name;
                const char *ext = strrchr(name, '.');
                if (!ext || (strcasecmp(ext, ".json") != 0)) continue;
                if (state->import_file_count >= MAX_IMPORT_FILES) break;
                size_t len = strlen(name);
                if (len >= sizeof(state->import_files[0])) continue;
                snprintf(state->import_files[state->import_file_count],
                         sizeof(state->import_files[0]),
                         "%s/%s", import_scan_dir, name);
                state->import_file_count++;
            }
            closedir(legacy);
        }
    }

    editor_list_view_set_rows(&state->import_view, state->import_file_count);
}
