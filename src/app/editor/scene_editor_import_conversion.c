#include "app/editor/scene_editor_input_import_helpers.h"
#include "app/physics_sim_persistence.h"
#include "app/scene_presets.h"
#include "import/shape_asset_output.h"
#include "import/shape_import.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool scene_editor_input_convert_import_to_asset(const char *import_path,
                                                const char *configured_root,
                                                char *out_asset_path,
                                                size_t out_sz) {
    /* configured_root selects input/library discovery, not mutable output. */
    (void)configured_root;
    if (!out_asset_path || !out_sz) return false;
    out_asset_path[0] = '\0';
    if (!import_path || !import_path[0]) return false;
    const char *base = strrchr(import_path, '/');
    base = base ? base + 1 : import_path;
    const char *dot = strrchr(base, '.');
    size_t length = dot && dot > base ? (size_t)(dot - base) : strlen(base);
    char name[256];
    if (!length || length >= sizeof(name)) return false;
    memcpy(name, base, length);
    name[length] = '\0';
    const char *directory = getenv("SHAPE_ASSET_DIR");
    bool generated_default = !directory || !directory[0];
    if (generated_default) directory = "data/runtime";
    char asset_path[512];
    int required = snprintf(asset_path, sizeof(asset_path), "%s/%s.asset.json", directory, name);
    if (required < 0 || (size_t)required >= sizeof(asset_path) || (size_t)required >= out_sz ||
        (size_t)required >= sizeof(((ImportedShape *)0)->path)) return false;

    ShapeDocument doc = {0};
    if (!shape_import_load(import_path, &doc)) return false;
    if (!doc.shapeCount) { ShapeDocument_Free(&doc); return false; }
    ShapeAsset asset = {0};
    bool ok = shape_asset_from_shapelib_shape(&doc.shapes[0], 0.5f, &asset);
    if (ok) {
        char *asset_name = malloc(length + 1);
        if (!asset_name) ok = false;
        else {
            memcpy(asset_name, name, length + 1);
            free(asset.name);
            asset.name = asset_name;
        }
    }
    if (ok && generated_default) ok = physics_sim_persistence_runtime_directory();
    if (ok) ok = physics_sim_shape_asset_publish(&asset, import_path, asset_path);
    shape_asset_free(&asset);
    ShapeDocument_Free(&doc);
    if (ok) memcpy(out_asset_path, asset_path, (size_t)required + 1);
    return ok;
}
