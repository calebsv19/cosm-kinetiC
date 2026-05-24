#include "physics/soft/soft_body.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const float SOFT_BODY_DEFAULT_GRAVITY_Y = -9.8f;
static const float SOFT_BODY_MIN_LENGTH_EPSILON = 0.0001f;
static const float SOFT_BODY_DEFAULT_DAMPING = 0.999f;
static const int   SOFT_BODY_DEFAULT_CONSTRAINT_ITERATIONS = 6;
static const float SOFT_BODY_DEFAULT_CONSTRAINT_STIFFNESS = 0.85f;

static float soft_body_inverse_mass(float mass) {
    return (mass > 0.0f) ? (1.0f / mass) : 0.0f;
}

static float soft_body_triangle_signed_area(Vec2 a, Vec2 b, Vec2 c) {
    return 0.5f * ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
}

static float soft_body_velocity_displacement(
    float velocity [[fisics::dim(velocity)]] [[fisics::unit(meter_per_second)]],
    float dt [[fisics::dim(time)]] [[fisics::unit(second)]]) {
    return velocity * dt;
}

static float soft_body_position_advance(
    float position [[fisics::dim(length)]] [[fisics::unit(meter)]],
    float displacement [[fisics::dim(length)]] [[fisics::unit(meter)]]) {
    return position + displacement;
}

static float soft_body_force_from_mass_acceleration(
    float mass [[fisics::dim(mass)]] [[fisics::unit(kilogram)]],
    float acceleration [[fisics::dim(acceleration)]]
                       [[fisics::unit(meter_per_second_squared)]]) {
    return mass * acceleration;
}

static float soft_body_acceleration_from_force(
    float force [[fisics::dim(force)]] [[fisics::unit(newton)]],
    float mass [[fisics::dim(mass)]] [[fisics::unit(kilogram)]]) {
    float zero_mass [[fisics::dim(mass)]] [[fisics::unit(kilogram)]] = 0.0f;
    float zero_accel [[fisics::dim(acceleration)]]
                     [[fisics::unit(meter_per_second_squared)]] = 0.0f;
    return (mass > zero_mass) ? (force / mass) : zero_accel;
}

static void soft_body_apply_spring_constraints(SoftBody2D *body) {
    if (!body || body->spring_count == 0) return;
    int iterations = body->constraint_iterations > 0 ? body->constraint_iterations : 0;
    float stiffness = body->constraint_stiffness;
    if (iterations <= 0 || stiffness <= 0.0f) return;

    for (int iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < body->spring_count; ++i) {
            const SoftBodySpring *spring = &body->springs[i];
            SoftBodyNode *node_a = &body->nodes[spring->node_a];
            SoftBodyNode *node_b = &body->nodes[spring->node_b];
            Vec2 delta = vec2_sub(node_b->position, node_a->position);
            float length = vec2_len(delta);
            if (length <= SOFT_BODY_MIN_LENGTH_EPSILON) continue;

            float error = length - spring->rest_length;
            if (fabsf(error) <= SOFT_BODY_MIN_LENGTH_EPSILON) continue;

            Vec2 direction = vec2_scale(delta, 1.0f / length);
            Vec2 correction = vec2_scale(direction, error * stiffness);
            float inv_mass_a = soft_body_inverse_mass(node_a->mass);
            float inv_mass_b = soft_body_inverse_mass(node_b->mass);
            float total_inv_mass = inv_mass_a + inv_mass_b;
            if (total_inv_mass <= 0.0f) continue;

            if (inv_mass_a > 0.0f) {
                node_a->position = vec2_add(
                    node_a->position,
                    vec2_scale(correction, inv_mass_a / total_inv_mass));
            }
            if (inv_mass_b > 0.0f) {
                node_b->position = vec2_sub(
                    node_b->position,
                    vec2_scale(correction, inv_mass_b / total_inv_mass));
            }
        }
    }
}

static void soft_body_apply_area_constraints(SoftBody2D *body) {
    if (!body || body->area_constraint_count == 0) return;
    int iterations = body->constraint_iterations > 0 ? body->constraint_iterations : 0;
    if (iterations <= 0) return;

    for (int iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < body->area_constraint_count; ++i) {
            const SoftBodyAreaConstraint *constraint = &body->area_constraints[i];
            SoftBodyNode *node_a = &body->nodes[constraint->node_a];
            SoftBodyNode *node_b = &body->nodes[constraint->node_b];
            SoftBodyNode *node_c = &body->nodes[constraint->node_c];

            float current_area =
                soft_body_triangle_signed_area(node_a->position, node_b->position, node_c->position);
            float error = current_area - constraint->rest_area;
            if (fabsf(error) <= SOFT_BODY_MIN_LENGTH_EPSILON) continue;

            Vec2 grad_a = vec2(0.5f * (node_b->position.y - node_c->position.y),
                               0.5f * (node_c->position.x - node_b->position.x));
            Vec2 grad_b = vec2(0.5f * (node_c->position.y - node_a->position.y),
                               0.5f * (node_a->position.x - node_c->position.x));
            Vec2 grad_c = vec2(0.5f * (node_a->position.y - node_b->position.y),
                               0.5f * (node_b->position.x - node_a->position.x));

            float inv_mass_a = soft_body_inverse_mass(node_a->mass);
            float inv_mass_b = soft_body_inverse_mass(node_b->mass);
            float inv_mass_c = soft_body_inverse_mass(node_c->mass);
            float denom = inv_mass_a * vec2_dot(grad_a, grad_a) +
                          inv_mass_b * vec2_dot(grad_b, grad_b) +
                          inv_mass_c * vec2_dot(grad_c, grad_c);
            if (denom <= SOFT_BODY_MIN_LENGTH_EPSILON) continue;

            float lambda = (error * constraint->stiffness * body->constraint_stiffness) / denom;
            if (inv_mass_a > 0.0f) {
                node_a->position =
                    vec2_sub(node_a->position, vec2_scale(grad_a, lambda * inv_mass_a));
            }
            if (inv_mass_b > 0.0f) {
                node_b->position =
                    vec2_sub(node_b->position, vec2_scale(grad_b, lambda * inv_mass_b));
            }
            if (inv_mass_c > 0.0f) {
                node_c->position =
                    vec2_sub(node_c->position, vec2_scale(grad_c, lambda * inv_mass_c));
            }
        }
    }
}

static bool soft_body_reserve_nodes(SoftBody2D *body, int desired) {
    if (!body) return false;
    if (desired <= body->capacity) return true;
    int new_capacity = body->capacity > 0 ? body->capacity : 4;
    while (new_capacity < desired) new_capacity *= 2;
    SoftBodyNode *nodes =
        (SoftBodyNode *)realloc(body->nodes, (size_t)new_capacity * sizeof(*nodes));
    if (!nodes) return false;
    if (new_capacity > body->capacity) {
        memset(nodes + body->capacity,
               0,
               (size_t)(new_capacity - body->capacity) * sizeof(*nodes));
    }
    body->nodes = nodes;
    body->capacity = new_capacity;
    return true;
}

static bool soft_body_reserve_springs(SoftBody2D *body, int desired) {
    if (!body) return false;
    if (desired <= body->spring_capacity) return true;
    int new_capacity = body->spring_capacity > 0 ? body->spring_capacity : 4;
    while (new_capacity < desired) new_capacity *= 2;
    SoftBodySpring *springs = (SoftBodySpring *)realloc(
        body->springs, (size_t)new_capacity * sizeof(*springs));
    if (!springs) return false;
    if (new_capacity > body->spring_capacity) {
        memset(springs + body->spring_capacity,
               0,
               (size_t)(new_capacity - body->spring_capacity) * sizeof(*springs));
    }
    body->springs = springs;
    body->spring_capacity = new_capacity;
    return true;
}

static bool soft_body_reserve_area_constraints(SoftBody2D *body, int desired) {
    if (!body) return false;
    if (desired <= body->area_constraint_capacity) return true;
    int new_capacity = body->area_constraint_capacity > 0 ? body->area_constraint_capacity : 4;
    while (new_capacity < desired) new_capacity *= 2;
    SoftBodyAreaConstraint *constraints = (SoftBodyAreaConstraint *)realloc(
        body->area_constraints, (size_t)new_capacity * sizeof(*constraints));
    if (!constraints) return false;
    if (new_capacity > body->area_constraint_capacity) {
        memset(constraints + body->area_constraint_capacity,
               0,
               (size_t)(new_capacity - body->area_constraint_capacity) * sizeof(*constraints));
    }
    body->area_constraints = constraints;
    body->area_constraint_capacity = new_capacity;
    return true;
}

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
    body->springs = NULL;
    body->spring_count = 0;
    body->spring_capacity = 0;
    body->area_constraints = NULL;
    body->area_constraint_count = 0;
    body->area_constraint_capacity = 0;
    body->gravity = vec2(0.0f, SOFT_BODY_DEFAULT_GRAVITY_Y);
    body->constraint_iterations = SOFT_BODY_DEFAULT_CONSTRAINT_ITERATIONS;
    body->constraint_stiffness = SOFT_BODY_DEFAULT_CONSTRAINT_STIFFNESS;
    return body;
}

void soft_body2d_destroy(SoftBody2D *body) {
    if (!body) return;
    free(body->nodes);
    free(body->springs);
    free(body->area_constraints);
    free(body);
}

int soft_body2d_add_node(SoftBody2D *body,
                         Vec2 position,
                         float mass [[fisics::dim(mass)]]
                                    [[fisics::unit(kilogram)]]) {
    if (!body) return -1;
    if (!soft_body_reserve_nodes(body, body->count + 1)) return -1;
    SoftBodyNode *node = &body->nodes[body->count];
    memset(node, 0, sizeof(*node));
    node->position = position;
    node->velocity = vec2(0.0f, 0.0f);
    node->mass = mass;
    return body->count++;
}

bool soft_body2d_add_spring(SoftBody2D *body,
                            int node_a,
                            int node_b,
                            float stiffness,
                            float damping) {
    if (!body) return false;
    if (node_a < 0 || node_b < 0 || node_a >= body->count || node_b >= body->count ||
        node_a == node_b) {
        return false;
    }
    if (stiffness <= 0.0f || damping < 0.0f) return false;
    if (!soft_body_reserve_springs(body, body->spring_count + 1)) return false;
    SoftBodySpring *spring = &body->springs[body->spring_count++];
    Vec2 delta = vec2_sub(body->nodes[node_b].position, body->nodes[node_a].position);
    spring->node_a = node_a;
    spring->node_b = node_b;
    spring->rest_length = math_maxf(vec2_len(delta), SOFT_BODY_MIN_LENGTH_EPSILON);
    spring->stiffness = stiffness;
    spring->damping = damping;
    return true;
}

bool soft_body2d_add_area_constraint(SoftBody2D *body,
                                     int node_a,
                                     int node_b,
                                     int node_c,
                                     float stiffness) {
    if (!body) return false;
    if (node_a < 0 || node_b < 0 || node_c < 0 || node_a >= body->count ||
        node_b >= body->count || node_c >= body->count) {
        return false;
    }
    if (node_a == node_b || node_a == node_c || node_b == node_c) return false;
    if (stiffness <= 0.0f) return false;
    if (!soft_body_reserve_area_constraints(body, body->area_constraint_count + 1)) return false;
    SoftBodyAreaConstraint *constraint =
        &body->area_constraints[body->area_constraint_count++];
    constraint->node_a = node_a;
    constraint->node_b = node_b;
    constraint->node_c = node_c;
    constraint->rest_area = soft_body_triangle_signed_area(body->nodes[node_a].position,
                                                           body->nodes[node_b].position,
                                                           body->nodes[node_c].position);
    constraint->stiffness = stiffness;
    return true;
}

void soft_body2d_step(SoftBody2D *body,
                      double dt [[fisics::dim(time)]] [[fisics::unit(second)]],
                      const AppConfig *cfg) {
    (void)cfg;
    if (!body || body->count == 0) return;
    float fdt [[fisics::dim(time)]] [[fisics::unit(second)]] = (float)dt;
    float zero_seconds [[fisics::dim(time)]] [[fisics::unit(second)]] = 0.0f;
    if (fdt <= zero_seconds) return;
    Vec2 *forces = (Vec2 *)calloc((size_t)body->count, sizeof(*forces));
    Vec2 *previous_positions = (Vec2 *)calloc((size_t)body->count, sizeof(*previous_positions));
    if (!forces || !previous_positions) {
        free(forces);
        free(previous_positions);
        return;
    }

    for (int i = 0; i < body->count; ++i) {
        previous_positions[i] = body->nodes[i].position;
    }

    float gravity_y [[fisics::dim(acceleration)]]
                    [[fisics::unit(meter_per_second_squared)]] = body->gravity.y;
    float zero_mass [[fisics::dim(mass)]] [[fisics::unit(kilogram)]] = 0.0f;
    for (int i = 0; i < body->count; ++i) {
        SoftBodyNode *node = &body->nodes[i];
        float mass [[fisics::dim(mass)]] [[fisics::unit(kilogram)]] = node->mass;
        if (mass <= zero_mass) continue;
        float gravity_force_y [[fisics::dim(force)]] [[fisics::unit(newton)]] =
            soft_body_force_from_mass_acceleration(mass, gravity_y);
        forces[i].y += gravity_force_y;
    }

    for (int i = 0; i < body->spring_count; ++i) {
        const SoftBodySpring *spring = &body->springs[i];
        SoftBodyNode *node_a = &body->nodes[spring->node_a];
        SoftBodyNode *node_b = &body->nodes[spring->node_b];
        Vec2 delta = vec2_sub(node_b->position, node_a->position);
        float length = vec2_len(delta);
        if (length <= SOFT_BODY_MIN_LENGTH_EPSILON) continue;
        Vec2 direction = vec2_scale(delta, 1.0f / length);
        Vec2 rel_velocity = vec2_sub(node_b->velocity, node_a->velocity);
        float stretch = length - spring->rest_length;
        float rel_speed = vec2_dot(rel_velocity, direction);
        float force_magnitude = stretch * spring->stiffness + rel_speed * spring->damping;
        Vec2 spring_force = vec2_scale(direction, force_magnitude);
        if (node_a->mass > 0.0f) {
            forces[spring->node_a] = vec2_add(forces[spring->node_a], spring_force);
        }
        if (node_b->mass > 0.0f) {
            forces[spring->node_b] = vec2_sub(forces[spring->node_b], spring_force);
        }
    }

    float velocity_damping = (cfg && cfg->velocity_damping > 0.0f)
                                 ? cfg->velocity_damping
                                 : SOFT_BODY_DEFAULT_DAMPING;
    for (int i = 0; i < body->count; ++i) {
        SoftBodyNode *node = &body->nodes[i];
        float mass [[fisics::dim(mass)]] [[fisics::unit(kilogram)]] = node->mass;
        if (mass <= zero_mass) {
            node->velocity = vec2(0.0f, 0.0f);
            continue;
        }
        float force_x [[fisics::dim(force)]] [[fisics::unit(newton)]] = forces[i].x;
        float force_y [[fisics::dim(force)]] [[fisics::unit(newton)]] = forces[i].y;
        float accel_x [[fisics::dim(acceleration)]]
                      [[fisics::unit(meter_per_second_squared)]] =
            soft_body_acceleration_from_force(force_x, mass);
        float accel_y [[fisics::dim(acceleration)]]
                      [[fisics::unit(meter_per_second_squared)]] =
            soft_body_acceleration_from_force(force_y, mass);
        float position_x [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            node->position.x;
        float position_y [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            node->position.y;
        float velocity_x [[fisics::dim(velocity)]]
                         [[fisics::unit(meter_per_second)]] = node->velocity.x;
        float velocity_y [[fisics::dim(velocity)]]
                         [[fisics::unit(meter_per_second)]] = node->velocity.y;
        velocity_x += accel_x * fdt;
        velocity_y += accel_y * fdt;
        velocity_x *= velocity_damping;
        velocity_y *= velocity_damping;
        float displacement_x [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            soft_body_velocity_displacement(velocity_x, fdt);
        float displacement_y [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            soft_body_velocity_displacement(velocity_y, fdt);
        node->position.x = soft_body_position_advance(position_x, displacement_x);
        node->position.y = soft_body_position_advance(position_y, displacement_y);
        node->velocity.x = velocity_x;
        node->velocity.y = velocity_y;
    }

    soft_body_apply_spring_constraints(body);
    soft_body_apply_area_constraints(body);

    for (int i = 0; i < body->count; ++i) {
        SoftBodyNode *node = &body->nodes[i];
        if (node->mass <= 0.0f) {
            node->position = previous_positions[i];
            node->velocity = vec2(0.0f, 0.0f);
            continue;
        }
        Vec2 displacement = vec2_sub(node->position, previous_positions[i]);
        node->velocity = vec2_scale(displacement, 1.0f / fdt);
    }

    free(forces);
    free(previous_positions);
}
