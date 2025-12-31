#ifndef COLLIDER_LEGACY_H
#define COLLIDER_LEGACY_H

#include "geo/shape_library.h"
#include "render/import_project.h"
#include "physics/rigid/collider_types.h"

int collider_resample_path(const ShapeAssetPath *path,
                           float cx,
                           float cy,
                           float norm,
                           const ImportProjectParams *proj,
                           HullPoint *out,
                           int max_out,
                           float samples_per_100);

int collider_decompose_to_convex(const HullPoint *pts,
                                 int count,
                                 HullPoint parts[][32],
                                 int *counts,
                                 int max_parts,
                                 int vert_cap);

#endif // COLLIDER_LEGACY_H
