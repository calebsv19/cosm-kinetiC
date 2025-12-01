#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "physics/math/math2d.h"

// System-agnostic instance data for a shape placed in a scene.
typedef struct {
    int      shape_id;  // index into ShapeAsset library
    Vec2     position;
    float    rotation;  // radians
    Vec2     scale;     // uniform or non-uniform
    uint32_t flags;     // visibility/collision toggles
} SceneObjectBase;

// Physics-specific extension of SceneObjectBase.
typedef struct {
    SceneObjectBase base;
    uint8_t  *mask;    // rasterized collider mask (owned)
    int       mask_w;
    int       mask_h;
    float     density;
    float     friction;
    bool      is_static;
} PhysicsObject;
