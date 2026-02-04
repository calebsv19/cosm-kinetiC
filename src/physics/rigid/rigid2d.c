#include "physics/rigid/rigid2d.h"
#include "physics/rigid/rigid2d_collision.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Helpers
static const int R2D_MAX_POLY_VERTS = 32;
#define R2D_MAX_MANIFOLDS 4096
#define R2D_IMPULSE_ITERS 12
#define R2D_GRAVITY_Y 9.8f

static void rigid2d_free_poly(RigidBody2D *b);
static void rigid2d_copy_poly(RigidPoly *dst, const RigidPoly *src);

static RigidBody2D *rigid2d_get_body(Rigid2DWorld *w, int index) {
    if (!w) return NULL;
    if (index < 0 || index >= w->count) return NULL;
    return &w->bodies[index];
}

void rigid2d_set_mass(RigidBody2D *b, float mass, float inertia) {
    if (!b) return;
    if (mass < 0.0f) mass = 0.0f;
    if (inertia < 0.0f) inertia = 0.0f;
    b->mass = mass;
    b->inv_mass = (mass > 0.0f) ? 1.0f / mass : 0.0f;
    b->inertia = inertia;
    b->inv_inertia = (inertia > 0.0f) ? 1.0f / inertia : 0.0f;
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
    w->bodies[w->count].force_accum = vec2(0.0f, 0.0f);
    w->bodies[w->count].torque_accum = 0.0f;
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

// Build broad-phase candidate pairs using a coarse grid. Falls back to O(n^2) if disabled or too many bodies.
static int broadphase_pairs_grid(Rigid2DWorld *w,
                                 const AppConfig *cfg,
                                 Pair *pairs,
                                 int max_pairs) {
    if (!w || !cfg || !cfg->physics_broadphase_enabled || w->count <= 1 || max_pairs <= 0) return 0;
    const int n = w->count;
    if (n > 128) return 0; // fallback to brute force for large counts

    float cell_size = (cfg->physics_broadphase_cell_size > 0.0f) ? cfg->physics_broadphase_cell_size : 128.0f;
    float world_w = (cfg->window_w > 0) ? (float)cfg->window_w : 512.0f;
    float world_h = (cfg->window_h > 0) ? (float)cfg->window_h : 512.0f;
    int cols = (int)ceilf(world_w / cell_size);
    int rows = (int)ceilf(world_h / cell_size);
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (cols > 256) cols = 256;
    if (rows > 256) rows = 256;
    int cell_cap = cols * rows;
    if (cell_cap <= 0 || cell_cap > 65536) return 0;

    int *cell_counts = (int *)calloc((size_t)cell_cap, sizeof(int));
    int *cell_indices = (int *)calloc((size_t)cell_cap * (size_t)n, sizeof(int));
    if (!cell_counts || !cell_indices) {
        free(cell_counts);
        free(cell_indices);
        return 0;
    }

    // Map bodies to cells
    for (int bi = 0; bi < n; ++bi) {
        float minx = w->bodies[bi].poly.aabb_min_x;
        float maxx = w->bodies[bi].poly.aabb_max_x;
        float miny = w->bodies[bi].poly.aabb_min_y;
        float maxy = w->bodies[bi].poly.aabb_max_y;
        int cx0 = (int)floorf(minx / cell_size);
        int cx1 = (int)floorf(maxx / cell_size);
        int cy0 = (int)floorf(miny / cell_size);
        int cy1 = (int)floorf(maxy / cell_size);
        if (cx0 < 0) cx0 = 0;
        if (cy0 < 0) cy0 = 0;
        if (cx1 >= cols) cx1 = cols - 1;
        if (cy1 >= rows) cy1 = rows - 1;
        for (int cy = cy0; cy <= cy1; ++cy) {
            for (int cx = cx0; cx <= cx1; ++cx) {
                int cell = cy * cols + cx;
                int idx = cell_counts[cell];
                if (idx < n) {
                    cell_indices[cell * n + idx] = bi;
                    cell_counts[cell] = idx + 1;
                }
            }
        }
    }

    // Visit cells and emit unique pairs
    int pair_count = 0;
    char seen[128 * 128] = {0}; // n <= 128 guard above
    for (int cell = 0; cell < cell_cap && pair_count < max_pairs; ++cell) {
        int cnt = cell_counts[cell];
        if (cnt < 2) continue;
        int *indices = &cell_indices[cell * n];
        for (int i = 0; i < cnt; ++i) {
            for (int j = i + 1; j < cnt; ++j) {
                int a = indices[i];
                int b = indices[j];
                if (a > b) { int t = a; a = b; b = t; }
                int key = a * n + b;
                if (seen[key]) continue;
                seen[key] = 1;
                pairs[pair_count].a = a;
                pairs[pair_count].b = b;
                pair_count++;
                if (pair_count >= max_pairs) break;
            }
            if (pair_count >= max_pairs) break;
        }
    }

    free(cell_counts);
    free(cell_indices);
    return pair_count;
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

        // accumulate acceleration from forces + gravity
        Vec2 accel = vec2(0.0f, 0.0f);
        if (b->inv_mass > 0.0f) {
            accel = vec2_add(accel, vec2_scale(b->force_accum, b->inv_mass));
        }
        if (b->gravity_enabled) {
            accel = vec2_add(accel, gravity_vec);
        }
        b->velocity = vec2_add(b->velocity, vec2_scale(accel, fdt));
        if (b->inv_inertia > 0.0f) {
            b->angular_velocity += b->torque_accum * b->inv_inertia * fdt;
        }

        // integrate pose
        b->position = vec2_add(b->position, vec2_scale(b->velocity, fdt));
        b->angle   += b->angular_velocity * fdt;

        // clear force/torque accumulators
        b->force_accum = vec2(0.0f, 0.0f);
        b->torque_accum = 0.0f;

        // floor collision
        resolve_ground_collision(b, ground_y);
    }

    // Update AABBs for all bodies
    for (int i = 0; i < w->count; ++i) {
        rigid2d_update_body_aabb(&w->bodies[i]);
    }

    // Broad-phase and narrow-phase
    Pair candidates[4096];
    int pair_count = 0;
    int sat_hits = 0;
    const int log_sat = 0; // set to 1 to re-enable SAT debug prints
    if (cfg && cfg->physics_broadphase_enabled) {
        pair_count = broadphase_pairs_grid(w, cfg, candidates, 4096);
    }
    if (pair_count == 0) {
        // fallback to brute-force if disabled or no pairs
        for (int i = 0; i < w->count && pair_count < 4096; ++i) {
            for (int j = i + 1; j < w->count && pair_count < 4096; ++j) {
                candidates[pair_count++] = (Pair){i, j};
            }
        }
    }

    RigidManifold manifolds[R2D_MAX_MANIFOLDS];
    int manifold_count = 0;

    for (int p = 0; p < pair_count; ++p) {
        int i = candidates[p].a;
        int j = candidates[p].b;
        RigidBody2D *a = rigid2d_get_body(w, i);
        RigidBody2D *b = rigid2d_get_body(w, j);
        if (!a || !b) continue;
        if (a->is_static && b->is_static) continue;

        if (!rigid2d_aabb_overlap(a, b)) {
            if (log_sat && cfg && cfg->collider_debug_logs) {
                fprintf(stderr,
                        "[sat] skip AABB pair=(%d,%d) "
                        "A:[%.1f,%.1f]-[%.1f,%.1f] B:[%.1f,%.1f]-[%.1f,%.1f]\n",
                        i,
                        j,
                        a->poly.aabb_min_x,
                        a->poly.aabb_min_y,
                        a->poly.aabb_max_x,
                        a->poly.aabb_max_y,
                        b->poly.aabb_min_x,
                        b->poly.aabb_min_y,
                        b->poly.aabb_max_x,
                        b->poly.aabb_max_y);
            }
            continue;
        }

        if (a->shape == RIGID2D_SHAPE_CIRCLE && b->shape == RIGID2D_SHAPE_CIRCLE) {
            resolve_circle_circle(a, b);
            continue;
        }

        if (manifold_count >= R2D_MAX_MANIFOLDS) continue;

        int hit = rigid2d_collision_manifold(a, b, &manifolds[manifold_count]);
        if (hit && manifolds[manifold_count].contact_count > 0) {
            sat_hits++;
            manifolds[manifold_count].body_a = i;
            manifolds[manifold_count].body_b = j;
            if (log_sat && cfg && cfg->collider_debug_logs) {
                fprintf(stderr,
                        "[sat] pair=(%d,%d) contacts=%d depth=%.3f n=(%.2f,%.2f)\n",
                        i,
                        j,
                        manifolds[manifold_count].contact_count,
                        manifolds[manifold_count].depth,
                        manifolds[manifold_count].normal.x,
                        manifolds[manifold_count].normal.y);
                for (int ci = 0; ci < manifolds[manifold_count].contact_count; ++ci) {
                    Vec2 p = manifolds[manifold_count].contacts[ci].position;
                    fprintf(stderr, "  pt%d: %.2f %.2f depth=%.3f\n",
                            ci, p.x, p.y, manifolds[manifold_count].contacts[ci].penetration);
                }
            }
            manifold_count++;
        } else if (log_sat && cfg && cfg->collider_debug_logs) {
            // Log the world-space vertices for failed SAT to debug scaling/position issues.
            Vec2 va[8], vb[8];
            int ca = 0, cb = 0;
            if (a->shape == RIGID2D_SHAPE_POLY) ca = rigid2d_poly_world(a, va, 8);
            if (b->shape == RIGID2D_SHAPE_POLY) cb = rigid2d_poly_world(b, vb, 8);
            fprintf(stderr, "[sat] miss pair=(%d,%d) vertsA=%d vertsB=%d\n", i, j, ca, cb);
            for (int vi = 0; vi < ca; ++vi) {
                fprintf(stderr, "  A v%d: %.2f %.2f\n", vi, va[vi].x, va[vi].y);
            }
            for (int vi = 0; vi < cb; ++vi) {
                fprintf(stderr, "  B v%d: %.2f %.2f\n", vi, vb[vi].x, vb[vi].y);
            }
            if (ca == 0 && a->shape == RIGID2D_SHAPE_AABB) {
                fprintf(stderr, "  A is AABB half=(%.2f,%.2f) pos=(%.2f,%.2f)\n",
                        a->half_extents.x, a->half_extents.y, a->position.x, a->position.y);
            }
            if (cb == 0 && b->shape == RIGID2D_SHAPE_AABB) {
                fprintf(stderr, "  B is AABB half=(%.2f,%.2f) pos=(%.2f,%.2f)\n",
                        b->half_extents.x, b->half_extents.y, b->position.x, b->position.y);
            }
            // Diagnose first separating axis using world verts we already computed.
            if (ca > 0 && cb > 0) {
                Vec2 axes[16];
                int axis_count = 0;
                for (int vi = 0; vi < ca && axis_count < 16; ++vi) {
                    Vec2 e = vec2_sub(va[(vi + 1) % ca], va[vi]);
                    Vec2 ax = vec2(-e.y, e.x);
                    float len = vec2_len(ax);
                    if (len > 1e-6f) {
                        ax = vec2_scale(ax, 1.0f / len);
                        axes[axis_count++] = ax;
                    }
                }
                for (int vi = 0; vi < cb && axis_count < 16; ++vi) {
                    Vec2 e = vec2_sub(vb[(vi + 1) % cb], vb[vi]);
                    Vec2 ax = vec2(-e.y, e.x);
                    float len = vec2_len(ax);
                    if (len > 1e-6f) {
                        ax = vec2_scale(ax, 1.0f / len);
                        axes[axis_count++] = ax;
                    }
                }
                for (int ai = 0; ai < axis_count; ++ai) {
                    Vec2 ax = axes[ai];
                    float minA = 1e9f, maxA = -1e9f;
                    float minB = 1e9f, maxB = -1e9f;
                    for (int va_i = 0; va_i < ca; ++va_i) {
                        float p = vec2_dot(va[va_i], ax);
                        if (p < minA) minA = p;
                        if (p > maxA) maxA = p;
                    }
                    for (int vb_i = 0; vb_i < cb; ++vb_i) {
                        float p = vec2_dot(vb[vb_i], ax);
                        if (p < minB) minB = p;
                        if (p > maxB) maxB = p;
                    }
                    float overlap = fminf(maxA, maxB) - fmaxf(minA, minB);
                    fprintf(stderr, "  axis %d n=(%.3f,%.3f) overlap=%.3f A[%.3f,%.3f] B[%.3f,%.3f]\n",
                            ai, ax.x, ax.y, overlap, minA, maxA, minB, maxB);
                }
            }
        }
    }

    // Iterative impulse solve
    for (int iter = 0; iter < R2D_IMPULSE_ITERS; ++iter) {
        for (int mi = 0; mi < manifold_count; ++mi) {
            RigidManifold *m = &manifolds[mi];
            RigidBody2D *a = rigid2d_get_body(w, m->body_a);
            RigidBody2D *b = rigid2d_get_body(w, m->body_b);
            if (!a || !b) continue;
            rigid2d_resolve_impulse_basic(a, b, m, fdt);
        }
    }

    // Positional correction (gentle) after velocity solve
    for (int mi = 0; mi < manifold_count; ++mi) {
        RigidManifold *m = &manifolds[mi];
        RigidBody2D *a = rigid2d_get_body(w, m->body_a);
        RigidBody2D *b = rigid2d_get_body(w, m->body_b);
        if (!a || !b) continue;
        rigid2d_positional_correction(a, b, m);
    }

    if (log_sat && cfg && cfg->collider_debug_logs && pair_count > 0) {
        fprintf(stderr, "[sat] pairs=%d hits=%d\n", pair_count, sat_hits);
    }
}
