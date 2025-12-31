#include "physics/rigid/rigid2d.h"

#include <stdlib.h>
#include <string.h>

// Helpers
static const int R2D_MAX_POLY_VERTS = 32;
#define R2D_GRAVITY_Y 9.8f

static void rigid2d_free_poly(RigidBody2D *b);
static void rigid2d_copy_poly(RigidPoly *dst, const RigidPoly *src);

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
    w->gravity  = vec2(0.0f, R2D_GRAVITY_Y); // downwards

    return w;
}

void rigid2d_destroy(Rigid2DWorld *w) {
    if (!w) return;
    if (w->bodies) {
        for (int i = 0; i < w->count; ++i) {
            rigid2d_free_poly(&w->bodies[i]);
        }
        free(w->bodies);
    }
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
    // deep-copy polygon if present
    rigid2d_copy_poly(&w->bodies[w->count].poly, &body->poly);
    return w->count++;
}

static void rigid2d_free_poly(RigidBody2D *b) {
    if (!b) return;
    free(b->poly.verts);
    b->poly.verts = NULL;
    b->poly.count = 0;
}

static void rigid2d_copy_poly(RigidPoly *dst, const RigidPoly *src) {
    if (!dst || !src || src->count <= 0 || !src->verts) {
        if (dst) {
            dst->verts = NULL;
            dst->count = 0;
        }
        return;
    }
    int count = src->count;
    if (count > R2D_MAX_POLY_VERTS) count = R2D_MAX_POLY_VERTS;
    dst->verts = (Vec2 *)malloc((size_t)count * sizeof(Vec2));
    if (!dst->verts) {
        dst->count = 0;
        return;
    }
    memcpy(dst->verts, src->verts, (size_t)count * sizeof(Vec2));
    dst->count = count;
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

// --- Broad-phase helpers ---
typedef struct {
    int   a;
    int   b;
} Pair;

static int aabb_overlap(const RigidBody2D *a, const RigidBody2D *b) {
    if (!a || !b) return 0;
    float aminx = (a->shape == RIGID2D_SHAPE_CIRCLE)
                      ? (a->position.x - a->radius)
                      : a->poly.aabb_min_x;
    float amaxx = (a->shape == RIGID2D_SHAPE_CIRCLE)
                      ? (a->position.x + a->radius)
                      : a->poly.aabb_max_x;
    float aminy = (a->shape == RIGID2D_SHAPE_CIRCLE)
                      ? (a->position.y - a->radius)
                      : a->poly.aabb_min_y;
    float amaxy = (a->shape == RIGID2D_SHAPE_CIRCLE)
                      ? (a->position.y + a->radius)
                      : a->poly.aabb_max_y;

    float bminx = (b->shape == RIGID2D_SHAPE_CIRCLE)
                      ? (b->position.x - b->radius)
                      : b->poly.aabb_min_x;
    float bmaxx = (b->shape == RIGID2D_SHAPE_CIRCLE)
                      ? (b->position.x + b->radius)
                      : b->poly.aabb_max_x;
    float bminy = (b->shape == RIGID2D_SHAPE_CIRCLE)
                      ? (b->position.y - b->radius)
                      : b->poly.aabb_min_y;
    float bmaxy = (b->shape == RIGID2D_SHAPE_CIRCLE)
                      ? (b->position.y + b->radius)
                      : b->poly.aabb_max_y;

    return (amaxx >= bminx && bmaxx >= aminx &&
            amaxy >= bminy && bmaxy >= aminy);
}

// --- Narrow-phase helpers (SAT) ---
typedef struct {
    Vec2 normal;
    float penetration;
    int  hit;
} Contact;

static Vec2 rotate_vec(Vec2 v, float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

static void compute_poly_aabb(RigidBody2D *b) {
    if (!b || b->shape != RIGID2D_SHAPE_POLY || !b->poly.verts || b->poly.count <= 0) return;
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (int i = 0; i < b->poly.count; ++i) {
        Vec2 v = rotate_vec(b->poly.verts[i], b->angle);
        v = vec2_add(v, b->position);
        if (v.x < minx) minx = v.x;
        if (v.x > maxx) maxx = v.x;
        if (v.y < miny) miny = v.y;
        if (v.y > maxy) maxy = v.y;
    }
    b->poly.aabb_min_x = minx;
    b->poly.aabb_min_y = miny;
    b->poly.aabb_max_x = maxx;
    b->poly.aabb_max_y = maxy;
}

static Contact sat_poly_poly(const RigidBody2D *a, const RigidBody2D *b) {
    Contact c = {.hit = 0, .penetration = 0.0f, .normal = vec2(0.0f, 0.0f)};
    if (!a || !b || a->poly.count <= 0 || b->poly.count <= 0) return c;

    const RigidBody2D *polys[2] = {a, b};
    float min_pen = 1e9f;
    Vec2 best_normal = vec2(0.0f, 0.0f);

    for (int idx = 0; idx < 2; ++idx) {
        const RigidBody2D *p = polys[idx];
        for (int i = 0; i < p->poly.count; ++i) {
            Vec2 v0 = rotate_vec(p->poly.verts[i], p->angle);
            Vec2 v1 = rotate_vec(p->poly.verts[(i + 1) % p->poly.count], p->angle);
            v0 = vec2_add(v0, p->position);
            v1 = vec2_add(v1, p->position);
            Vec2 edge = vec2_sub(v1, v0);
            Vec2 axis = vec2(-edge.y, edge.x);
            float len = vec2_len(axis);
            if (len < 1e-6f) continue;
            axis = vec2_scale(axis, 1.0f / len);

            float minA = 1e9f, maxA = -1e9f;
            for (int va = 0; va < a->poly.count; ++va) {
                Vec2 wp = vec2_add(rotate_vec(a->poly.verts[va], a->angle), a->position);
                float proj = vec2_dot(wp, axis);
                if (proj < minA) minA = proj;
                if (proj > maxA) maxA = proj;
            }
            float minB = 1e9f, maxB = -1e9f;
            for (int vb = 0; vb < b->poly.count; ++vb) {
                Vec2 wp = vec2_add(rotate_vec(b->poly.verts[vb], b->angle), b->position);
                float proj = vec2_dot(wp, axis);
                if (proj < minB) minB = proj;
                if (proj > maxB) maxB = proj;
            }
            float overlap = fminf(maxA, maxB) - fmaxf(minA, minB);
            if (overlap <= 0.0f) {
                return c; // separating axis found
            }
            if (overlap < min_pen) {
                min_pen = overlap;
                best_normal = axis;
                // ensure normal points from a to b
                Vec2 delta = vec2_sub(b->position, a->position);
                if (vec2_dot(delta, best_normal) < 0.0f) {
                    best_normal = vec2_scale(best_normal, -1.0f);
                }
            }
        }
    }

    c.hit = 1;
    c.penetration = min_pen;
    c.normal = best_normal;
    return c;
}

static Contact sat_circle_poly(const RigidBody2D *circle, const RigidBody2D *poly) {
    Contact c = {.hit = 0, .penetration = 0.0f, .normal = vec2(0.0f, 0.0f)};
    if (!circle || !poly || poly->poly.count <= 0) return c;
    Vec2 center = circle->position;
    float radius = circle->radius;

    float min_pen = 1e9f;
    Vec2 best_normal = vec2(0.0f, 0.0f);
    int hit = 0;

    // Test polygon edges
    for (int i = 0; i < poly->poly.count; ++i) {
        Vec2 v0 = vec2_add(rotate_vec(poly->poly.verts[i], poly->angle), poly->position);
        Vec2 v1 = vec2_add(rotate_vec(poly->poly.verts[(i + 1) % poly->poly.count], poly->angle), poly->position);
        Vec2 edge = vec2_sub(v1, v0);
        Vec2 axis = vec2(-edge.y, edge.x);
        float len = vec2_len(axis);
        if (len < 1e-6f) continue;
        axis = vec2_scale(axis, 1.0f / len);

        float minP = 1e9f, maxP = -1e9f;
        for (int vp = 0; vp < poly->poly.count; ++vp) {
            Vec2 wp = vec2_add(rotate_vec(poly->poly.verts[vp], poly->angle), poly->position);
            float proj = vec2_dot(wp, axis);
            if (proj < minP) minP = proj;
            if (proj > maxP) maxP = proj;
        }
        float projC = vec2_dot(center, axis);
        float minC = projC - radius;
        float maxC = projC + radius;
        float overlap = fminf(maxP, maxC) - fmaxf(minP, minC);
        if (overlap <= 0.0f) {
            return c; // separating axis
        }
        if (overlap < min_pen) {
            min_pen = overlap;
            best_normal = axis;
            Vec2 delta = vec2_sub(center, poly->position);
            if (vec2_dot(delta, best_normal) < 0.0f) {
                best_normal = vec2_scale(best_normal, -1.0f);
            }
        }
        hit = 1;
    }

    if (!hit) return c;
    c.hit = 1;
    c.penetration = min_pen;
    c.normal = best_normal;
    return c;
}

static void positional_correction(RigidBody2D *a, RigidBody2D *b, Vec2 normal, float penetration) {
    const float percent = 0.8f; // correction factor
    const float slop = 0.01f;
    float inv_mass_sum = a->inv_mass + b->inv_mass;
    if (inv_mass_sum <= 0.0f) return;
    float corr_mag = fmaxf(penetration - slop, 0.0f) * percent / inv_mass_sum;
    Vec2 correction = vec2_scale(normal, corr_mag);
    if (!a->is_static) a->position = vec2_sub(a->position, vec2_scale(correction, a->inv_mass));
    if (!b->is_static) b->position = vec2_add(b->position, vec2_scale(correction, b->inv_mass));
}

static void resolve_impulse(RigidBody2D *a, RigidBody2D *b, Vec2 normal) {
    if (!a || !b) return;
    Vec2 rv = vec2_sub(b->velocity, a->velocity);
    float vel_along_normal = vec2_dot(rv, normal);
    if (vel_along_normal > 0.0f) return;
    float inv_mass_sum = a->inv_mass + b->inv_mass;
    if (inv_mass_sum <= 0.0f) return;
    float e = 0.5f * (a->restitution + b->restitution);
    float j = -(1.0f + e) * vel_along_normal;
    j /= inv_mass_sum;
    Vec2 impulse = vec2_scale(normal, j);
    if (!a->is_static) a->velocity = vec2_sub(a->velocity, vec2_scale(impulse, a->inv_mass));
    if (!b->is_static) b->velocity = vec2_add(b->velocity, vec2_scale(impulse, b->inv_mass));
}

void rigid2d_step(Rigid2DWorld *w, double dt, const AppConfig *cfg) {
    if (!w) return;

    float fdt = (float)dt;
    float ground_y = (cfg && cfg->window_h > 0) ? (float)(cfg->window_h - 1) : 0.0f;
    Vec2 gravity_vec = w->gravity;

    // Integrate and update AABBs
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

    // Update AABBs for polygons
    for (int i = 0; i < w->count; ++i) {
        if (w->bodies[i].shape == RIGID2D_SHAPE_POLY) {
            compute_poly_aabb(&w->bodies[i]);
        } else if (w->bodies[i].shape == RIGID2D_SHAPE_AABB) {
            // treat as box polygon for aabb
            float hx = w->bodies[i].half_extents.x;
            float hy = w->bodies[i].half_extents.y;
            w->bodies[i].poly.aabb_min_x = w->bodies[i].position.x - hx;
            w->bodies[i].poly.aabb_max_x = w->bodies[i].position.x + hx;
            w->bodies[i].poly.aabb_min_y = w->bodies[i].position.y - hy;
            w->bodies[i].poly.aabb_max_y = w->bodies[i].position.y + hy;
        }
    }

    // Broad-phase and narrow-phase
    for (int i = 0; i < w->count; ++i) {
        for (int j = i + 1; j < w->count; ++j) {
            RigidBody2D *a = rigid2d_get_body(w, i);
            RigidBody2D *b = rigid2d_get_body(w, j);
            if (!a || !b) continue;
            if (a->is_static && b->is_static) continue;

            if (!aabb_overlap(a, b)) continue;

            Contact contact = {.hit = 0};
            if (a->shape == RIGID2D_SHAPE_CIRCLE && b->shape == RIGID2D_SHAPE_CIRCLE) {
                // reuse old circle-circle
                resolve_circle_circle(a, b);
                continue;
            } else if (a->shape == RIGID2D_SHAPE_CIRCLE && b->shape == RIGID2D_SHAPE_POLY) {
                contact = sat_circle_poly(a, b);
            } else if (a->shape == RIGID2D_SHAPE_POLY && b->shape == RIGID2D_SHAPE_CIRCLE) {
                contact = sat_circle_poly(b, a);
                contact.normal = vec2_scale(contact.normal, -1.0f); // flip
            } else if (a->shape == RIGID2D_SHAPE_POLY && b->shape == RIGID2D_SHAPE_POLY) {
                contact = sat_poly_poly(a, b);
            } else {
                continue;
            }

            if (contact.hit && contact.penetration > 0.0f) {
                positional_correction(a, b, contact.normal, contact.penetration);
                resolve_impulse(a, b, contact.normal);
            }
        }
    }
}
