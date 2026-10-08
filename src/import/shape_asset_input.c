#include "import/shape_asset_input.h"
#include "app/physics_sim_job_json.h"
#include <float.h>
#include <math.h>
#include <string.h>
#include <strings.h>

static bool exact_keys(json_object *object, const char *const *keys, size_t count) {
    if (!object || !json_object_is_type(object, json_type_object)) return false;
    json_object_object_foreach(object, key, value) {
        (void)value;
        for (size_t i = 0; i < count; ++i)
            if (strcasecmp(key, keys[i]) == 0 && strcmp(key, keys[i]) != 0) return false;
    }
    return true;
}
static json_object *field(json_object *object, const char *key) {
    json_object *value = NULL;
    json_object_object_get_ex(object, key, &value);
    return value;
}
static bool asset_document_admitted(json_object *root) {
    static const char *const root_keys[] = {"schema", "name", "paths"};
    static const char *const path_keys[] = {"closed", "points"};
    static const char *const point_keys[] = {"x", "y"};
    if (!exact_keys(root, root_keys, 3)) return false;
    json_object *schema = field(root, "schema"), *name = field(root, "name");
    if (schema && (!json_object_is_type(schema, json_type_int) || json_object_get_int64(schema) != 1)) return false;
    if (name && (!json_object_is_type(name, json_type_string) || json_object_get_string_len(name) > PHYSICS_SIM_ASSET_MAX_NAME_BYTES)) return false;
    /* json-c represents explicit null by NULL too; presence must be checked. */
    json_object *present = NULL;
    if ((json_object_object_get_ex(root, "schema", &present) && !schema) ||
        (json_object_object_get_ex(root, "name", &present) && !name)) return false;
    json_object *paths = field(root, "paths");
    if (!paths || !json_object_is_type(paths, json_type_array) || json_object_array_length(paths) > PHYSICS_SIM_ASSET_MAX_PATHS) return false;
    size_t total = 0;
    for (size_t i = 0; i < json_object_array_length(paths); ++i) {
        json_object *path = json_object_array_get_idx(paths, i);
        if (!exact_keys(path, path_keys, 2)) return false;
        json_object *closed = field(path, "closed");
        if ((closed && !json_object_is_type(closed, json_type_boolean)) ||
            (json_object_object_get_ex(path, "closed", &present) && !closed)) return false;
        json_object *points = field(path, "points");
        if (!points || !json_object_is_type(points, json_type_array)) return false;
        size_t count = json_object_array_length(points);
        if (count > PHYSICS_SIM_ASSET_MAX_POINTS - total) return false;
        total += count;
        for (size_t j = 0; j < count; ++j) {
            json_object *point = json_object_array_get_idx(points, j);
            if (!exact_keys(point, point_keys, 2)) return false;
            for (size_t k = 0; k < 2; ++k) {
                json_object *number = field(point, point_keys[k]);
                if (!number || (!json_object_is_type(number, json_type_int) && !json_object_is_type(number, json_type_double))) return false;
                double value = json_object_get_double(number);
                if (!isfinite(value) || value < -FLT_MAX || value > FLT_MAX) return false;
            }
        }
    }
    return true;
}
bool physics_sim_shape_asset_admitted(const ShapeAsset *asset) {
    if (!asset || asset->schema > 1 || asset->path_count > PHYSICS_SIM_ASSET_MAX_PATHS ||
        (asset->path_count && !asset->paths)) return false;
    if (asset->name && strnlen(asset->name, PHYSICS_SIM_ASSET_MAX_NAME_BYTES + 1u) > PHYSICS_SIM_ASSET_MAX_NAME_BYTES) return false;
    size_t total = 0;
    for (size_t i = 0; i < asset->path_count; ++i) {
        const ShapeAssetPath *path = &asset->paths[i];
        if (path->point_count > PHYSICS_SIM_ASSET_MAX_POINTS - total ||
            (path->point_count && !path->points)) return false;
        total += path->point_count;
        for (size_t j = 0; j < path->point_count; ++j)
            if (!isfinite(path->points[j].x) || !isfinite(path->points[j].y)) return false;
    }
    return true;
}
bool physics_sim_shape_asset_text_admitted(const char *text, size_t size) {
    if (!text || size > PHYSICS_SIM_ASSET_MAX_BYTES) return false;
    json_object *root = physics_sim_job_json_parse(text, size);
    if (!root) return false;
    bool ok = asset_document_admitted(root);
    json_object_put(root);
    return ok;
}
bool physics_sim_shape_asset_load(const char *path, ShapeAsset *out_asset) {
    if (!path || !out_asset) return false;
    memset(out_asset, 0, sizeof(*out_asset));
    json_object *root = physics_sim_job_json_read(path);
    if (!root) return false;
    bool ok = false;
    if (asset_document_admitted(root)) {
        const char *text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
        if (text) ok = shape_asset_from_json_text(text, out_asset);
    }
    json_object_put(root);
    return ok;
}
