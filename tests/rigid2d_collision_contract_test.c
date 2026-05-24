#include "physics/rigid/rigid2d_collision.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static RigidBody2D make_dynamic_body(Vec2 position, Vec2 velocity) {
    RigidBody2D body;
    memset(&body, 0, sizeof(body));
    body.position = position;
    body.velocity = velocity;
    body.mass = 1.0f;
    body.inv_mass = 1.0f;
    body.inertia = 1.0f;
    body.inv_inertia = 1.0f;
    body.restitution = 0.6f;
    body.friction = 0.4f;
    return body;
}

static int test_restitution_creates_separating_normal_velocity(void) {
    RigidBody2D a = make_dynamic_body(vec2(-0.5f, 0.0f), vec2(1.0f, 0.0f));
    RigidBody2D b = make_dynamic_body(vec2(0.5f, 0.0f), vec2(-1.0f, 0.0f));

    RigidManifold m;
    memset(&m, 0, sizeof(m));
    m.normal = vec2(1.0f, 0.0f);
    m.depth = 1.0f;
    m.contact_count = 1;
    m.contacts[0].position = vec2(0.0f, 0.0f);
    m.contacts[0].penetration = 1.0f;
    m.restitution = 0.6f;
    m.friction = 0.0f;

    rigid2d_resolve_impulse_basic(&a, &b, &m, 1.0 / 60.0);

    Vec2 rv = vec2_sub(b.velocity, a.velocity);
    float separating_speed = vec2_dot(rv, m.normal);
    return separating_speed > 0.5f;
}

static int test_friction_reduces_tangent_speed(void) {
    RigidBody2D a = make_dynamic_body(vec2(-0.5f, 0.0f), vec2(1.0f, 0.0f));
    RigidBody2D b = make_dynamic_body(vec2(0.5f, 0.0f), vec2(-1.0f, 2.0f));

    RigidManifold m;
    memset(&m, 0, sizeof(m));
    m.normal = vec2(1.0f, 0.0f);
    m.tangent = vec2(0.0f, 1.0f);
    m.depth = 1.0f;
    m.contact_count = 1;
    m.contacts[0].position = vec2(0.0f, 0.0f);
    m.contacts[0].penetration = 1.0f;
    m.restitution = 0.0f;
    m.friction = 0.4f;

    float pre_tangent_speed = fabsf(vec2_dot(vec2_sub(b.velocity, a.velocity), m.tangent));
    rigid2d_resolve_impulse_basic(&a, &b, &m, 1.0 / 60.0);
    float post_tangent_speed = fabsf(vec2_dot(vec2_sub(b.velocity, a.velocity), m.tangent));
    return post_tangent_speed < pre_tangent_speed;
}

int main(void) {
    if (!test_restitution_creates_separating_normal_velocity()) {
        fprintf(stderr, "rigid2d_collision_contract_test: restitution failed\n");
        return 1;
    }
    if (!test_friction_reduces_tangent_speed()) {
        fprintf(stderr, "rigid2d_collision_contract_test: friction failed\n");
        return 1;
    }
    printf("rigid2d_collision_contract_test: success\n");
    return 0;
}
