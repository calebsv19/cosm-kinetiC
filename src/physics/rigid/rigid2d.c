#include "physics/rigid/rigid2d.h"

#include <stdlib.h>
#include <string.h>

// Helpers
static RigidBody2D *rigid2d_get_body(Rigid2DWorld *w, int index) {
    if (!w) return NULL;
    if (index < 0 || index >= w->count) return NULL;
    return &w->bodies[index];
}

Rigid2DWorld *rigid2d_create(int capacity) {
    if (capacity <= 0) capacity = 16;

    Rigid2DWorld *w = (Rigid2DWorld *)malloc(sizeof(Rigid2DWorld));
    if (!w) return NULL;

    w->bodies = (RigidBody2D *)calloc((size_t)capacity, sizeof(RigidBody2D));
    if (!w->bodies) {
        free(w);
        return NULL;
    }

    w->count    = 0;
    w->capacity = capacity;
    w->gravity  = vec2(0.0f, 9.8f); // downwards

    return w;
}

void rigid2d_destroy(Rigid2DWorld *w) {
    if (!w) return;
    free(w->bodies);
    free(w);
}

int rigid2d_add_body(Rigid2DWorld *w, const RigidBody2D *body) {
    if (!w || !body) return -1;

    if (w->count >= w->capacity) {
        int new_capacity = w->capacity * 2;
        RigidBody2D *new_data = (RigidBody2D *)realloc(
            w->bodies, (size_t)new_capacity * sizeof(RigidBody2D));
        if (!new_data) return -1;
        w->bodies   = new_data;
        w->capacity = new_capacity;
    }

    w->bodies[w->count] = *body;
    return w->count++;
}

// Simple ground plane at y = ground_y in "world units"
static void resolve_ground_collision(RigidBody2D *b, float ground_y) {
    if (b->is_static || b->locked) return;

    if (b->position.y > ground_y) {
        b->position.y = ground_y;
        if (b->velocity.y > 0.0f) {
            b->velocity.y = -b->velocity.y * b->restitution;
            b->velocity.x *= (1.0f - b->friction);
        }
    }
}

// Very minimal circle-circle collision with impulse resolution
static void resolve_circle_circle(RigidBody2D *a, RigidBody2D *b) {
    if (a->shape != RIGID2D_SHAPE_CIRCLE || b->shape != RIGID2D_SHAPE_CIRCLE)
        return;

    if (a->is_static && b->is_static) return;

    Vec2 delta = vec2_sub(b->position, a->position);
    float dist_sq = vec2_len_sq(delta);
    float radius_sum = a->radius + b->radius;

    if (dist_sq <= 0.0f || dist_sq >= radius_sum * radius_sum) {
        return; // no overlap
    }

    float dist = sqrtf(dist_sq);
    Vec2 normal = dist > 0.0f ? vec2_scale(delta, 1.0f / dist)
                              : vec2(0.0f, 1.0f);

    float penetration = radius_sum - dist;

    float inv_mass_sum = a->inv_mass + b->inv_mass;
    if (inv_mass_sum <= 0.0f) return;

    // positional correction to avoid sinking
    Vec2 correction = vec2_scale(normal, penetration / inv_mass_sum);
    if (!a->is_static) {
        a->position = vec2_sub(a->position, vec2_scale(correction, a->inv_mass));
    }
    if (!b->is_static) {
        b->position = vec2_add(b->position, vec2_scale(correction, b->inv_mass));
    }

    // relative velocity
    Vec2 rv = vec2_sub(b->velocity, a->velocity);
    float vel_along_normal = vec2_dot(rv, normal);
    if (vel_along_normal > 0.0f) {
        return; // moving apart
    }

    float e = 0.5f * (a->restitution + b->restitution);
    float j = -(1.0f + e) * vel_along_normal;
    j /= inv_mass_sum;

    Vec2 impulse = vec2_scale(normal, j);
    if (!a->is_static) {
        a->velocity = vec2_sub(a->velocity, vec2_scale(impulse, a->inv_mass));
    }
    if (!b->is_static) {
        b->velocity = vec2_add(b->velocity, vec2_scale(impulse, b->inv_mass));
    }
}

void rigid2d_step(Rigid2DWorld *w, double dt, const AppConfig *cfg) {
    if (!w) return;

    float fdt = (float)dt;
    float ground_y = (cfg && cfg->window_h > 0) ? (float)(cfg->window_h - 1) : 0.0f;
    float gravity_mag = (cfg && cfg->window_h > 0)
                            ? (float)cfg->window_h * 2.0f // ~2 screen-heights/s^2
                            : 980.0f;
    Vec2 gravity_vec = vec2(0.0f, gravity_mag);

    // Integrate
    for (int i = 0; i < w->count; ++i) {
        RigidBody2D *b = &w->bodies[i];
        if (b->is_static || b->inv_mass <= 0.0f || b->locked) continue;

        // apply gravity
        if (b->gravity_enabled) {
            b->velocity = vec2_add(b->velocity, vec2_scale(gravity_vec, fdt));
        }

        // integrate
        b->position = vec2_add(b->position, vec2_scale(b->velocity, fdt));
        b->angle   += b->angular_velocity * fdt;

        // floor collision
        resolve_ground_collision(b, ground_y);
    }

    // Pairwise circle-circle collisions (N^2, fine for small counts)
    for (int i = 0; i < w->count; ++i) {
        for (int j = i + 1; j < w->count; ++j) {
            RigidBody2D *a = rigid2d_get_body(w, i);
            RigidBody2D *b = rigid2d_get_body(w, j);
            if (!a || !b) continue;
            resolve_circle_circle(a, b);
        }
    }
}
