#include "physics/particles/particles2d.h"

#include <stdlib.h>
#include <string.h>

static Vec2 particles2d_gravity_accel(void) {
    float gravity_x
        [[fisics::dim(acceleration)]]
        [[fisics::unit(meter_per_second_squared)]] = 0.0f;
    float gravity_y
        [[fisics::dim(acceleration)]]
        [[fisics::unit(meter_per_second_squared)]] = 9.8f;
    return vec2(gravity_x, gravity_y);
}

static float particles2d_integrate_lifetime(
    float lifetime [[fisics::dim(time)]] [[fisics::unit(second)]],
    float dt [[fisics::dim(time)]] [[fisics::unit(second)]]) {
    return lifetime - dt;
}

static float particles2d_integrate_velocity_component(
    float velocity [[fisics::dim(velocity)]] [[fisics::unit(meter_per_second)]],
    float accel [[fisics::dim(acceleration)]] [[fisics::unit(meter_per_second_squared)]],
    float dt [[fisics::dim(time)]] [[fisics::unit(second)]]) {
    return velocity + accel * dt;
}

static float particles2d_integrate_position_component(
    float position [[fisics::dim(length)]] [[fisics::unit(meter)]],
    float velocity [[fisics::dim(velocity)]] [[fisics::unit(meter_per_second)]],
    float dt [[fisics::dim(time)]] [[fisics::unit(second)]]) {
    return position + velocity * dt;
}

static float particles2d_fluid_velocity_blend_factor(void) {
    return 0.1f;
}

// Sample fluid velocity at a given position in grid coordinates
static Vec2 particles2d_sample_legacy_fluid_velocity_grid(
    const Fluid2D *fluid,
    float particle_grid_x,
    float particle_grid_y) {
    Vec2 sampled_velocity = vec2(0.0f, 0.0f);
    if (!fluid) return sampled_velocity;
    /* Shared Fluid2D sampling still exposes legacy grid-space velocities. */
    fluid2d_sample_velocity(
        fluid, particle_grid_x, particle_grid_y, &sampled_velocity);
    return sampled_velocity;
}

static Vec2 particles2d_apply_legacy_fluid_velocity_bridge(
    Vec2 particle_velocity,
    const Fluid2D *fluid,
    Vec2 particle_position) {
    if (!fluid) return particle_velocity;

    /*
     * Legacy compatibility bridge:
     * - particle positions are still sampled directly in grid coordinates
     * - sampled fluid values are still legacy scalar grid-space velocities
     * - blend keeps the seam localized until fluid/world units are separated
     */
    Vec2 sampled_grid_velocity = particles2d_sample_legacy_fluid_velocity_grid(
        fluid, particle_position.x, particle_position.y);
    return vec2_lerp(
        particle_velocity,
        sampled_grid_velocity,
        particles2d_fluid_velocity_blend_factor());
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
                      double dt [[fisics::dim(time)]] [[fisics::unit(second)]],
                      const AppConfig *cfg,
                      const Fluid2D   *fluid,
                      const Rigid2DWorld *rigid) {
    (void)cfg;
    (void)rigid;
    if (!p || p->count == 0) return;

    float zero_seconds [[fisics::dim(time)]] [[fisics::unit(second)]] = 0.0f;
    if (dt <= zero_seconds) return;

    float fdt [[fisics::dim(time)]] [[fisics::unit(second)]] = (float)dt;
    Vec2 gravity = particles2d_gravity_accel();

    int write_index = 0;
    for (int i = 0; i < p->count; ++i) {
        Particle2D pt = p->particles[i];

        // Kill dead particles
        {
            float lifetime_seconds [[fisics::dim(time)]] [[fisics::unit(second)]] = pt.lifetime;
            lifetime_seconds = particles2d_integrate_lifetime(lifetime_seconds, fdt);
            pt.lifetime = lifetime_seconds;
        }
        if (pt.lifetime <= zero_seconds) {
            continue;
        }

        // Apply gravity
        {
            float velocity_x
                [[fisics::dim(velocity)]]
                [[fisics::unit(meter_per_second)]] = pt.velocity.x;
            float velocity_y
                [[fisics::dim(velocity)]]
                [[fisics::unit(meter_per_second)]] = pt.velocity.y;
            float gravity_x
                [[fisics::dim(acceleration)]]
                [[fisics::unit(meter_per_second_squared)]] = gravity.x;
            float gravity_y
                [[fisics::dim(acceleration)]]
                [[fisics::unit(meter_per_second_squared)]] = gravity.y;
            pt.velocity.x = particles2d_integrate_velocity_component(
                velocity_x, gravity_x, fdt);
            pt.velocity.y = particles2d_integrate_velocity_component(
                velocity_y, gravity_y, fdt);
        }

        // Apply fluid velocity influence if available
        pt.velocity = particles2d_apply_legacy_fluid_velocity_bridge(
            pt.velocity, fluid, pt.position);

        // Integrate
        {
            float position_x [[fisics::dim(length)]] [[fisics::unit(meter)]] = pt.position.x;
            float position_y [[fisics::dim(length)]] [[fisics::unit(meter)]] = pt.position.y;
            float velocity_x
                [[fisics::dim(velocity)]]
                [[fisics::unit(meter_per_second)]] = pt.velocity.x;
            float velocity_y
                [[fisics::dim(velocity)]]
                [[fisics::unit(meter_per_second)]] = pt.velocity.y;
            pt.position.x = particles2d_integrate_position_component(
                position_x, velocity_x, fdt);
            pt.position.y = particles2d_integrate_position_component(
                position_y, velocity_y, fdt);
        }

        // Write back to compacted array
        p->particles[write_index++] = pt;
    }

    p->count = write_index;
}
