#ifndef SOFT_BODY_H
#define SOFT_BODY_H

#include "physics/math/math2d.h"
#include "app/app_config.h"

typedef struct SoftBodyNode {
    Vec2  position;
    Vec2  velocity;
    float mass;
} SoftBodyNode;

typedef struct SoftBody2D {
    SoftBodyNode *nodes;
    int           count;
    int           capacity;
} SoftBody2D;

SoftBody2D *soft_body2d_create(int capacity);
void        soft_body2d_destroy(SoftBody2D *body);
void        soft_body2d_step(SoftBody2D *body, double dt, const AppConfig *cfg);

#endif // SOFT_BODY_H
