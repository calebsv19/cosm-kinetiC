#include "physics/soft/soft_body.h"

#include <stdlib.h>
#include <string.h>

SoftBody2D *soft_body2d_create(int capacity) {
    if (capacity <= 0) capacity = 4;
    SoftBody2D *body = (SoftBody2D *)malloc(sizeof(SoftBody2D));
    if (!body) return NULL;

    body->nodes = (SoftBodyNode *)calloc((size_t)capacity, sizeof(SoftBodyNode));
    if (!body->nodes) {
        free(body);
        return NULL;
    }

    body->count = 0;
    body->capacity = capacity;
    return body;
}

void soft_body2d_destroy(SoftBody2D *body) {
    if (!body) return;
    free(body->nodes);
    free(body);
}

void soft_body2d_step(SoftBody2D *body, double dt, const AppConfig *cfg) {
    (void)cfg;
    if (!body || body->count == 0) return;
    float fdt = (float)dt;
    for (int i = 0; i < body->count; ++i) {
        SoftBodyNode *node = &body->nodes[i];
        node->position = vec2_add(node->position, vec2_scale(node->velocity, fdt));
    }
    // Placeholder: constraints and elasticity will go here.
}
