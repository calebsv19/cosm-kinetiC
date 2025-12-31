#ifndef COLLIDER_PRIM_GEOM_H
#define COLLIDER_PRIM_GEOM_H

#include "physics/rigid/collider_types.h"

int collider_capsule_vertices(const ColliderPrimitive *p, Vec2 *out, int max_out);
int collider_box_vertices(const ColliderPrimitive *p, Vec2 *out, int max_out);
int collider_hull_vertices(const ColliderPrimitive *p, Vec2 *out, int max_out);
int collider_primitive_to_vertices(const ColliderPrimitive *prim, Vec2 *out, int max_out);

#endif // COLLIDER_PRIM_GEOM_H
