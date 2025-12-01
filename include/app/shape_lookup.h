#pragma once

#include "geo/shape_library.h"

// Resolve an asset from a library given a path-like or name-like key.
// Falls back to stripping directories and extensions before matching name.
const ShapeAsset *shape_lookup_from_path(const ShapeAssetLibrary *lib,
                                         const char *path_or_name);
