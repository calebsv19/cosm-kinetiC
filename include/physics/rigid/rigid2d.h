#ifndef RIGID2D_H
#define RIGID2D_H

#include "physics/math/math2d.h"
#include "app/app_config.h"

// Simple 2D rigid body representation.
// This is standalone and not yet wired into SceneState.

typedef enum {
    RIGID2D_SHAPE_CIRCLE,
    RIGID2D_SHAPE_AABB,
    RIGID2D_SHAPE_POLY
} Rigid2DShape;

typedef struct {
    Vec2   *verts;      // local-space vertices (counter-clockwise)
    int     count;      // number of vertices
    float   aabb_min_x; // world-space AABB (cached)
    float   aabb_min_y;
    float   aabb_max_x;
    float   aabb_max_y;
} RigidPoly;

typedef struct RigidBody2D {
    Vec2  position;
    Vec2  velocity;
    float angle;
    float angular_velocity;
    Vec2  force_accum;
    float torque_accum;
    int   gravity_enabled; // 0 = ignore gravity
    int   locked;          // 1 = kinematic/locked (no integration)

    float mass;
    float inv_mass;

    float inertia;
    float inv_inertia;

    Rigid2DShape shape;
    float radius;      // for circles
    Vec2  half_extents; // for AABB
    RigidPoly poly;    // for polygons

    int   is_static;   // static bodies are immovable
    float restitution; // bounciness [0,1]
    float friction;    // simple friction [0,1]
} RigidBody2D;

typedef struct {
    Vec2  position;
    float penetration;
    float normal_impulse;
    float tangent_impulse;
} RigidContactPoint;

typedef struct {
    int   body_a;
    int   body_b;
    Vec2  normal;
    float depth;
    Vec2  tangent;
    RigidContactPoint contacts[2];
    int   contact_count;
    float restitution;
    float friction;
} RigidManifold;

typedef struct Rigid2DWorld {
    RigidBody2D *bodies;
    int count;
    int capacity;
    Vec2 gravity;
} Rigid2DWorld;

Rigid2DWorld *rigid2d_create(int capacity);
void          rigid2d_destroy(Rigid2DWorld *w);

int rigid2d_add_body(Rigid2DWorld *w, const RigidBody2D *body);
void rigid2d_set_mass(RigidBody2D *b, float mass, float inertia);

// Integrate motion, handle simple circle-circle and circle-floor collisions.
void rigid2d_step(Rigid2DWorld *w, double dt, const AppConfig *cfg);

#endif // RIGID2D_H
