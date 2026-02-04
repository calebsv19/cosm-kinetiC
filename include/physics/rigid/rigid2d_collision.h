#ifndef RIGID2D_COLLISION_H
#define RIGID2D_COLLISION_H

#include "physics/rigid/rigid2d.h"

// Compute/caches AABB for any body (circle/box/poly) in world space.
void rigid2d_update_body_aabb(RigidBody2D *b);

// Simple AABB overlap test using cached bounds.
int rigid2d_aabb_overlap(const RigidBody2D *a, const RigidBody2D *b);

// Convert a poly body's local vertices to world space (returns count written).
int rigid2d_poly_world(const RigidBody2D *b, Vec2 *out, int cap);

// Build a contact manifold for convex pairs (poly-poly, circle-poly). Returns 1 if hit.
int rigid2d_collision_manifold(const RigidBody2D *a,
                               const RigidBody2D *b,
                               RigidManifold *m);

// Basic positional correction along manifold normal.
void rigid2d_positional_correction(RigidBody2D *a,
                                   RigidBody2D *b,
                                   const RigidManifold *m);

// Basic normal + friction impulse resolution.
void rigid2d_resolve_impulse_basic(RigidBody2D *a,
                                   RigidBody2D *b,
                                   RigidManifold *m,
                                   float dt);

#endif // RIGID2D_COLLISION_H
