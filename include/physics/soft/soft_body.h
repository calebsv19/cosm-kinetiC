#ifndef SOFT_BODY_H
#define SOFT_BODY_H

#include "physics/math/math2d.h"
#include "app/app_config.h"

typedef struct SoftBodyNode {
    Vec2  position;
    Vec2  velocity;
    float mass;
} SoftBodyNode;

typedef struct SoftBodySpring {
    int   node_a;
    int   node_b;
    float rest_length;
    float stiffness;
    float damping;
} SoftBodySpring;

typedef struct SoftBodyAreaConstraint {
    int   node_a;
    int   node_b;
    int   node_c;
    float rest_area;
    float stiffness;
} SoftBodyAreaConstraint;

typedef struct SoftBody2D {
    SoftBodyNode           *nodes;
    int                     count;
    int                     capacity;
    SoftBodySpring         *springs;
    int                     spring_count;
    int                     spring_capacity;
    SoftBodyAreaConstraint *area_constraints;
    int                     area_constraint_count;
    int                     area_constraint_capacity;
    Vec2                    gravity;
    int                     constraint_iterations;
    float                   constraint_stiffness;
} SoftBody2D;

SoftBody2D *soft_body2d_create(int capacity);
void        soft_body2d_destroy(SoftBody2D *body);
int         soft_body2d_add_node(SoftBody2D *body,
                                 Vec2 position,
                                 float mass [[fisics::dim(mass)]]
                                            [[fisics::unit(kilogram)]]);
bool        soft_body2d_add_spring(SoftBody2D *body,
                                   int node_a,
                                   int node_b,
                                   float stiffness,
                                   float damping);
bool        soft_body2d_add_area_constraint(SoftBody2D *body,
                                            int node_a,
                                            int node_b,
                                            int node_c,
                                            float stiffness);
void        soft_body2d_step(SoftBody2D *body,
                             double dt [[fisics::dim(time)]]
                                       [[fisics::unit(second)]],
                             const AppConfig *cfg);

#endif // SOFT_BODY_H
