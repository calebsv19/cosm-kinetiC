#ifndef COLLIDER_TYPES_H
#define COLLIDER_TYPES_H

#include <stdbool.h>
#include "physics/math/math2d.h"

typedef struct HullPoint {
    float x, y;
} HullPoint;

typedef struct TaggedPoint {
    Vec2  pos;
    float angle_deg;
    bool  is_corner;
    bool  is_concave;
} TaggedPoint;

typedef struct ColliderRegion {
    int  boundary_indices[512]; // indices into loop vertices
    int  boundary_count;
    bool is_solid;
} ColliderRegion;

typedef enum ColliderPrimType {
    COLLIDER_PRIM_BOX = 0,
    COLLIDER_PRIM_CAPSULE,
    COLLIDER_PRIM_HULL
} ColliderPrimType;

typedef struct ColliderPrimitive {
    ColliderPrimType type;
    Vec2             center;
    Vec2             axis;          // box/capsule primary axis (unit)
    Vec2             half_extents;  // for box
    float            radius;        // capsule radius or hull inflation
    int              hull_count;    // for hull primitive
    Vec2             hull[16];      // capped hull verts (local space)
} ColliderPrimitive;

typedef enum ColliderSegmentClass {
    SEG_STRAIGHT = 0,
    SEG_GENTLE,
    SEG_TIGHT,
    SEG_SHORT
} ColliderSegmentClass;

typedef struct ColliderSegment {
    int  start_idx;   // vertex index in simplified loop
    int  end_idx;     // inclusive vertex index (wrap not applied)
    float length;     // total length of edges in segment
    float turn;       // total absolute turn (deg)
    float mean_turn;  // turn / length
    float aspect;     // max(w,h)/min(w,h)
    ColliderSegmentClass cls;
    bool solid_facing; // true if segment faces outward (eligible for collider)
} ColliderSegment;

#endif // COLLIDER_TYPES_H
