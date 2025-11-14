#include "physics/particles/particles2d.h"

#include <stdlib.h>
#include <string.h>

// Sample fluid velocity at a given position in grid coordinates
static Vec2 sample_fluid_velocity(const Fluid2D *fluid, float x, float y) {
    if (!fluid) return vec2(0.0f, 0.0f);

    int w = fluid->w;
    int h = fluid->h;

    float px = math_clampf(x, 0.5f, (float)w - 1.5f);
    float py = math_clampf(y, 0.5f, (float)h - 1.5f);

    int x0 = (int)px;
    int y0 = (int)py;
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float sx = px - (float)x0;
    float sy = py - (float)y0;

    size_t idx00 = (size_t)y0 * (size_t)w + (size_t)x0;
    size_t idx10 = (size_t)y0 * (size_t)w + (size_t)x1;
    size_t idx01 = (size_t)y1 * (size_t)w + (size_t)x0;
    size_t idx11 = (size_t)y1 * (size_t)w + (size_t)x1;

    float vx00 = fluid->velX[idx00];
    float vy00 = fluid->velY[idx00];
    float vx10 = fluid->velX[idx10];
    float vy10 = fluid->velY[idx10];
    float vx01 = fluid->velX[idx01];
    float vy01 = fluid->velY[idx01];
    float vx11 = fluid->velX[idx11];
    float vy11 = fluid->velY[idx11];

    float vx0 = math_lerp(vx00, vx10, sx);
    float vy0 = math_lerp(vy00, vy10, sx);
    float vx1 = math_lerp(vx01, vx11, sx);
    float vy1 = math_lerp(vy01, vy11, sx);

    float vx = math_lerp(vx0, vx1, sy);
    float vy = math_lerp(vy0, vy1, sy);

    return vec2(vx, vy);
}

Particles2D *particles2d_create(int capacity) {
    if (capacity <= 0) capacity = 128;

    Particles2D *p = (Particles2D *)malloc(sizeof(Particles2D));
    if (!p) return NULL;

    p->particles = (Particle2D *)calloc((size_t)capacity, sizeof(Particle2D));
    if (!p->particles) {
        free(p);
        return NULL;
    }

    p->count    = 0;
    p->capacity = capacity;
    return p;
}

void particles2d_destroy(Particles2D *p) {
    if (!p) return;
    free(p->particles);
    free(p);
}

void particles2d_spawn(Particles2D *p,
                       Vec2 position,
                       Vec2 velocity,
                       float lifetime) {
    if (!p) return;
    if (p->count >= p->capacity) {
        int new_capacity = p->capacity * 2;
        Particle2D *new_data = (Particle2D *)realloc(
            p->particles, (size_t)new_capacity * sizeof(Particle2D));
        if (!new_data) return;
        p->particles = new_data;
        p->capacity  = new_capacity;
    }

    Particle2D *pt = &p->particles[p->count++];
    pt->position     = position;
    pt->velocity     = velocity;
    pt->lifetime     = lifetime;
    pt->max_lifetime = lifetime;
}

void particles2d_step(Particles2D *p,
                      double dt,
                      const AppConfig *cfg,
                      const Fluid2D   *fluid,
                      const Rigid2DWorld *rigid) {
    (void)cfg;
    (void)rigid;
    if (!p || p->count == 0) return;

    float fdt = (float)dt;
    Vec2 gravity = vec2(0.0f, 9.8f);

    int write_index = 0;
    for (int i = 0; i < p->count; ++i) {
        Particle2D pt = p->particles[i];

        // Kill dead particles
        pt.lifetime -= fdt;
        if (pt.lifetime <= 0.0f) {
            continue;
        }

        // Apply gravity
        pt.velocity = vec2_add(pt.velocity, vec2_scale(gravity, fdt));

        // Apply fluid velocity influence if available
        if (fluid) {
            // Here we assume particle.position is in "grid space"
            Vec2 fv = sample_fluid_velocity(fluid, pt.position.x, pt.position.y);
            // Blend particle velocity slightly toward fluid
            pt.velocity = vec2_lerp(pt.velocity, fv, 0.1f);
        }

        // Integrate
        pt.position = vec2_add(pt.position, vec2_scale(pt.velocity, fdt));

        // Write back to compacted array
        p->particles[write_index++] = pt;
    }

    p->count = write_index;
}
