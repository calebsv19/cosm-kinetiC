#include "app/editor/scene_editor_import.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

void scene_editor_refresh_import_files(SceneEditorState *state) {
    if (!state) return;
    state->import_file_count = 0;

    // Canonical assets in config/objects.
    DIR *dir = opendir("config/objects");
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
                     "config/objects/%s", name);
            state->import_file_count++;
        }
        closedir(dir);
    }

    // Raw ShapeLib JSONs from import/ (line drawing output).
    if (state->import_file_count < MAX_IMPORT_FILES) {
        DIR *legacy = opendir("import");
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
                         "import/%s", name);
                state->import_file_count++;
            }
            closedir(legacy);
        }
    }

    editor_list_view_set_rows(&state->import_view, state->import_file_count);
}
