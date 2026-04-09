#include "app/editor/scene_editor_input_import_helpers.h"

#include "app/data_paths.h"
#include "app/editor/scene_editor_import.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_model.h"
#include "app/shape_lookup.h"
#include "geo/shape_asset.h"
#include "import/shape_import.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool path_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    size_t len = strlen(prefix);
    return strncmp(s, prefix, len) == 0;
}

static void to_asset_basename(const char *import_path, char *out_name, size_t out_sz) {
    if (!out_name || out_sz == 0) return;
    out_name[0] = '\0';
    if (!import_path) return;
    const char *base = strrchr(import_path, '/');
    base = base ? base + 1 : import_path;
    const char *dot = strrchr(base, '.');
    size_t len = dot && dot > base ? (size_t)(dot - base) : strlen(base);
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out_name, base, len);
    out_name[len] = '\0';
}

static void resolve_import_shape_id(SceneEditorState *state, ImportedShape *imp) {
    if (!state || !imp || !state->shape_library) return;
    const ShapeAsset *asset = shape_lookup_from_path(state->shape_library, imp->path);
    if (!asset) {
        fprintf(stderr, "[editor] No asset match for import path: %s\n", imp->path);
        imp->shape_id = -1;
        return;
    }
    for (size_t si = 0; si < state->shape_library->count; ++si) {
        if (&state->shape_library->assets[si] == asset) {
            imp->shape_id = (int)si;
            fprintf(stderr, "[editor] Resolved import '%s' to shape_id=%d (name=%s)\n",
                    imp->path, imp->shape_id, asset->name ? asset->name : "(unnamed)");
            return;
        }
    }
    imp->shape_id = -1;
}

static bool ensure_shape_loaded(SceneEditorState *state, const char *asset_path) {
    if (!state || !asset_path || !state->shape_library) return false;
    if (shape_lookup_from_path(state->shape_library, asset_path)) {
        return true;
    }
    // Append into the shared library so the newly converted asset is available immediately.
    ShapeAsset asset = (ShapeAsset){0};
    if (!shape_asset_load_file(asset_path, &asset)) {
        fprintf(stderr, "[editor] Failed to load asset %s\n", asset_path);
        return false;
    }
    ShapeAssetLibrary *lib = (ShapeAssetLibrary *)state->shape_library; // editor owns the lifetime
    ShapeAsset *tmp = (ShapeAsset *)realloc(lib->assets, (lib->count + 1) * sizeof(ShapeAsset));
    if (!tmp) {
        shape_asset_free(&asset);
        fprintf(stderr, "[editor] Failed to grow shape library for %s\n", asset_path);
        return false;
    }
    lib->assets = tmp;
    lib->assets[lib->count] = asset;
    lib->count += 1;
    fprintf(stderr, "[editor] Appended asset to library: %s (count=%zu)\n", asset_path, lib->count);
    return true;
}

bool scene_editor_input_convert_import_to_asset(const char *import_path,
                                                const char *configured_root,
                                                char *out_asset_path,
                                                size_t out_sz) {
    char objects_dir[512];
    const char *objects_root = NULL;
    if (!import_path || !out_asset_path || out_sz == 0) return false;
    out_asset_path[0] = '\0';

    char name[256];
    to_asset_basename(import_path, name, sizeof(name));
    if (name[0] == '\0') return false;

    objects_root = physics_sim_resolve_shape_asset_dir_for_root(configured_root,
                                                                objects_dir,
                                                                sizeof(objects_dir));
    char asset_path[512];
    snprintf(asset_path, sizeof(asset_path), "%s/%s.asset.json", objects_root, name);

    FILE *f = fopen(asset_path, "rb");
    if (f) {
        fclose(f);
        snprintf(out_asset_path, out_sz, "%s", asset_path);
        return true;
    }

    ShapeDocument doc;
    if (!shape_import_load(import_path, &doc) || doc.shapeCount == 0) {
        return false;
    }

    ShapeAsset asset;
    bool ok = shape_asset_from_shapelib_shape(&doc.shapes[0], 0.5f, &asset);
    if (ok) {
        if (asset.name) free(asset.name);
        asset.name = (char *)malloc(strlen(name) + 1);
        if (asset.name) {
            memcpy(asset.name, name, strlen(name) + 1);
        }
        ok = shape_asset_save_file(&asset, asset_path);
    }
    shape_asset_free(&asset);
    ShapeDocument_Free(&doc);
    if (ok) {
        snprintf(out_asset_path, out_sz, "%s", asset_path);
    }
    return ok;
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

bool scene_editor_input_add_import_from_picker(SceneEditorState *state, int row) {
    if (!state || row < 0 || row >= state->import_file_count) return false;
    const char *selected_path = state->import_files[row];
    char asset_path[512] = {0};
    const char *store_path = selected_path;
    if (scene_editor_input_path_contains_import_segment(selected_path, state->cfg.input_root)) {
        if (scene_editor_input_convert_import_to_asset(selected_path,
                                                       state->cfg.input_root,
                                                       asset_path,
                                                       sizeof(asset_path))) {
            store_path = asset_path;
            scene_editor_refresh_import_files(state);
        }
    }
    ensure_shape_loaded(state, store_path);
    if (state->working.import_shape_count < MAX_IMPORTED_SHAPES) {
        ImportedShape *imp = &state->working.import_shapes[state->working.import_shape_count++];
        memset(imp, 0, sizeof(*imp));
        snprintf(imp->path, sizeof(imp->path), "%s", store_path);
        imp->shape_id = -1;
        imp->position_x = 0.5f;
        imp->position_y = 0.5f;
        imp->position_z = 0.0f;
        imp->scale = 1.0f;
        imp->rotation_deg = 0.0f;
        imp->density = 1.0f;
        imp->friction = 0.2f;
        imp->is_static = true;
        imp->enabled = true;
        state->selected_row = (int)state->working.import_shape_count - 1;
        state->selection_kind = SELECTION_IMPORT;
        fprintf(stderr, "[editor] Added import row %zu: %s\n",
                state->working.import_shape_count - 1, store_path);
        resolve_import_shape_id(state, imp);
        set_dirty(state);
    }
    state->showing_import_picker = false;
    return true;
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
    set_dirty(state);
}
