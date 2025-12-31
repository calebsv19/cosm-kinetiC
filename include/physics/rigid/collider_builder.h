#ifndef COLLIDER_BUILDER_H
#define COLLIDER_BUILDER_H

#include <stdbool.h>
#include "app/app_config.h"
#include "app/scene_presets.h"
#include "geo/shape_library.h"

// Builds collider data for an imported shape:
// - Fills collider_part_count, collider_part_offsets/counts, collider_parts_verts (local space).
// - Fills legacy collider_verts for compatibility.
// Returns true on success; false if generation failed.
bool collider_build_import(const AppConfig *cfg,
                           const ShapeAssetLibrary *lib,
                           ImportedShape *imp);

#endif // COLLIDER_BUILDER_H
