#ifndef RIGID2D_H
#define RIGID2D_H

#include "physics/math/math2d.h"
#include "app/app_config.h"

// Simple 2D rigid body representation.
// This is standalone and not yet wired into SceneState.

typedef enum {
    RIGID2D_SHAPE_CIRCLE,
    RIGID2D_SHAPE_AABB
} Rigid2DShape;

typedef struct RigidBody2D {
    Vec2  position;
    Vec2  velocity;
    float angle;
    float angular_velocity;
    int   gravity_enabled; // 0 = ignore gravity
    int   locked;          // 1 = kinematic/locked (no integration)

    float mass;
    float inv_mass;

    float inertia;
    float inv_inertia;

    Rigid2DShape shape;
    float radius;      // for circles
    Vec2  half_extents; // for AABB

    int   is_static;   // static bodies are immovable
    float restitution; // bounciness [0,1]
    float friction;    // simple friction [0,1]
} RigidBody2D;

typedef struct Rigid2DWorld {
    RigidBody2D *bodies;
    int count;
    int capacity;
    Vec2 gravity;
} Rigid2DWorld;

Rigid2DWorld *rigid2d_create(int capacity);
void          rigid2d_destroy(Rigid2DWorld *w);

int rigid2d_add_body(Rigid2DWorld *w, const RigidBody2D *body);

// Integrate motion, handle simple circle-circle and circle-floor collisions.
void rigid2d_step(Rigid2DWorld *w, double dt, const AppConfig *cfg);

#endif // RIGID2D_H
