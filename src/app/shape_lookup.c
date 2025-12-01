#include "app/shape_lookup.h"

#include <string.h>

static const ShapeAsset *find_by_name(const ShapeAssetLibrary *lib,
                                      const char *name) {
    if (!lib || !name) return NULL;
    for (size_t i = 0; i < lib->count; ++i) {
        const ShapeAsset *a = &lib->assets[i];
        if (a->name && strcmp(a->name, name) == 0) {
            return a;
        }
    }
    return NULL;
}

const ShapeAsset *shape_lookup_from_path(const ShapeAssetLibrary *lib,
                                         const char *path_or_name) {
    if (!lib || !path_or_name || path_or_name[0] == '\0') return NULL;
    const ShapeAsset *a = find_by_name(lib, path_or_name);
    if (a) return a;

    // Strip directories.
    const char *basename = strrchr(path_or_name, '/');
    basename = basename ? basename + 1 : path_or_name;

    // Try basename as-is.
    a = find_by_name(lib, basename);
    if (a) return a;

    // Strip known trailing extensions (handle .asset.json or single .json).
    char buf[256] = {0};
    size_t len = strlen(basename);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, basename, len);
    buf[len] = '\0';
    // Remove .json
    char *dot = strrchr(buf, '.');
    if (dot && strcmp(dot, ".json") == 0) {
        *dot = '\0';
    }
    // Remove trailing .asset if present
    char *asset_ext = NULL;
    if ((asset_ext = strstr(buf, ".asset")) != NULL && asset_ext[6] == '\0') {
        *asset_ext = '\0';
    }
    if (buf[0] != '\0') {
        a = find_by_name(lib, buf);
        if (a) return a;
    }
    return NULL;
}
