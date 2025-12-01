#pragma once

#include <stdbool.h>

#include "geo/shape_asset.h"
#include "physics/objects/scene_object_base.h"

// Build a PhysicsObject from a ShapeAsset and SceneObjectBase transform.
// Assumes base.position is normalized (0..1) in grid space and rotation is radians.
// extra_opts can override margin/stroke/center_fit; position/rotation/scale
// are derived from the base unless extra_opts supplies center_fit=false overrides.
bool physics_object_from_asset(const ShapeAsset *asset,
                               const SceneObjectBase *base,
                               int grid_w,
                               int grid_h,
                               const ShapeAssetRasterOptions *extra_opts,
                               PhysicsObject *out);

void physics_object_free(PhysicsObject *obj);
