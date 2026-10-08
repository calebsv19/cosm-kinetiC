#include "app/editor/scene_editor_input_import_helpers.h"

#include "app/data_paths.h"
#include "app/editor/scene_editor_import.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_model.h"
#include "app/shape_lookup.h"
#include "geo/shape_asset.h"
#include "import/shape_import.h"
#include "import/shape_asset_input.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool path_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    size_t len = strlen(prefix);
    return strncmp(s, prefix, len) == 0;
}

/* Library slots are model indices. Refresh in place to preserve existing IDs. */
#define PICKER_MAX_ASSETS 1024u
static int load_shape_for_picker(SceneEditorState *state, const char *asset_path) {
    if (!state || !asset_path || !state->shape_library) return -1;
    ShapeAssetLibrary *lib = (ShapeAssetLibrary *)state->shape_library;
    if (lib->count > PICKER_MAX_ASSETS || (lib->count && !lib->assets)) return -1;
    const ShapeAsset *previous = shape_lookup_from_path(lib, asset_path);
    size_t index = previous ? (size_t)(previous - lib->assets) : lib->count;
    if (!previous && lib->count >= PICKER_MAX_ASSETS) return -1;

    ShapeAsset asset = {0};
    if (!physics_sim_shape_asset_load(asset_path, &asset)) {
        shape_asset_free(&asset);
        fprintf(stderr, "[editor] Failed to load asset %s\n", asset_path);
        return -1;
    }
    /* Persisted path lookup must resolve the candidate after a later reload too. */
    ShapeAssetLibrary candidate = { .assets = &asset, .count = 1 };
    if (shape_lookup_from_path(&candidate, asset_path) != &asset) {
        shape_asset_free(&asset);
        fprintf(stderr, "[editor] Asset identity does not match path %s\n", asset_path);
        return -1;
    }
    if (previous) {
        shape_asset_free(&lib->assets[index]);
        lib->assets[index] = asset;
    } else {
        ShapeAsset *grown = realloc(lib->assets, (lib->count + 1) * sizeof(*grown));
        if (!grown) {
            shape_asset_free(&asset);
            fprintf(stderr, "[editor] Failed to grow shape library for %s\n", asset_path);
            return -1;
        }
        lib->assets = grown;
        lib->assets[index] = asset;
        lib->count++;
    }
    return (int)index;
}

bool scene_editor_input_path_contains_import_segment(const char *path, const char *configured_root) {
    char import_dir[512];
    const char *resolved_import_dir = physics_sim_resolve_import_dir_for_root(configured_root,
                                                                               import_dir,
                                                                               sizeof(import_dir));
    size_t resolved_len = strlen(resolved_import_dir);
    if (!path || !path[0]) return false;
    if (strncmp(path, resolved_import_dir, resolved_len) == 0) {
        if (path[resolved_len] == '/' || path[resolved_len] == '\0') return true;
    }
    return path_starts_with(path, "import/");
}

static bool add_import_at(SceneEditorState *state, int row, float x, float y) {
    if (!isfinite(x) || !isfinite(y)) return false;
    if (!state || !state->shape_library || row < 0 ||
        state->import_file_count < 0 || state->import_file_count > MAX_IMPORT_FILES ||
        row >= state->import_file_count ||
        state->working.import_shape_count >= MAX_IMPORTED_SHAPES) return false;
    if (state->shape_library->count > PICKER_MAX_ASSETS ||
        (state->shape_library->count && !state->shape_library->assets)) return false;
    char selected_path[sizeof(state->import_files[0])];
    size_t selected_length = strnlen(state->import_files[row], sizeof(selected_path));
    if (!selected_length || selected_length >= sizeof(selected_path)) return false;
    memcpy(selected_path, state->import_files[row], selected_length + 1);
    char asset_path[sizeof(((ImportedShape *)0)->path)] = {0};
    const char *store_path = selected_path;
    bool converted = scene_editor_input_path_contains_import_segment(selected_path, state->cfg.input_root);
    if (converted) {
        if (!scene_editor_input_convert_import_to_asset(selected_path, state->cfg.input_root,
                                                       asset_path, sizeof(asset_path))) return false;
        store_path = asset_path;
    }
    if (strlen(store_path) >= sizeof(((ImportedShape *)0)->path)) return false;
    int shape_id = load_shape_for_picker(state, store_path);
    if (shape_id < 0) return false;

    ImportedShape *imp = &state->working.import_shapes[state->working.import_shape_count];
    memset(imp, 0, sizeof(*imp));
    memcpy(imp->path, store_path, strlen(store_path) + 1);
    imp->shape_id = shape_id;
    imp->position_x = x;
    imp->position_y = y;
    imp->scale = 1.0f;
    imp->density = 1.0f;
    imp->friction = 0.2f;
    imp->is_static = true;
    imp->enabled = true;
    state->working.import_shape_count++;
    scene_editor_select_import(state, (int)state->working.import_shape_count - 1);
    set_dirty(state);
    state->showing_import_picker = false;
    if (converted) scene_editor_refresh_import_files(state);
    return true;
}

bool scene_editor_input_add_import_from_picker(SceneEditorState *state, int row) {
    return add_import_at(state, row, 0.5f, 0.5f);
}

bool scene_editor_input_drop_import_from_picker(SceneEditorState *state, int row, float x, float y) {
    if (!state || !isfinite(x) || !isfinite(y) || row < 0 ||
        state->import_file_count < 0 || state->import_file_count > MAX_IMPORT_FILES ||
        row >= state->import_file_count || state->working.import_shape_count > MAX_IMPORTED_SHAPES) return false;
    size_t length = strnlen(state->import_files[row], sizeof(state->import_files[0]));
    if (!length || length >= sizeof(state->import_files[0])) return false;
    for (size_t i = 0; i < state->working.import_shape_count; ++i) {
        if (strncmp(state->working.import_shapes[i].path, state->import_files[row],
                    sizeof(state->working.import_shapes[i].path)) == 0) {
            scene_editor_select_import(state, (int)i);
            return true;
        }
    }
    return add_import_at(state, row, x, y);
}

void scene_editor_input_remove_import_at(SceneEditorState *state, int index) {
    if (!state || index < 0 || index >= (int)state->working.import_shape_count) return;

    int em_idx = emitter_index_for_import(state, index);
    if (em_idx >= 0) {
        remove_emitter_at(state, em_idx);
    }
    for (size_t ei = 0; ei < state->working.emitter_count; ++ei) {
        if (state->emitter_import_map[ei] > index) {
            state->emitter_import_map[ei]--;
        } else if (state->emitter_import_map[ei] == index) {
            state->emitter_import_map[ei] = -1;
            state->working.emitters[ei].attached_import = -1;
        }
    }
    for (int i = index; i + 1 < (int)state->working.import_shape_count; ++i) {
        state->working.import_shapes[i] = state->working.import_shapes[i + 1];
    }
    state->working.import_shape_count--;
    if (state->selected_row >= (int)state->working.import_shape_count) {
        state->selected_row = (int)state->working.import_shape_count - 1;
    }
    scene_editor_sync_selection_session(state);
    set_dirty(state);
}
