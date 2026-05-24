#include "physics/soft/soft_body.h"

#include <math.h>
#include <stdio.h>

static int nearly_equal(float a, float b) {
    return fabsf(a - b) < 0.001f;
}

static int test_spring_pulls_nodes_together(void) {
    AppConfig cfg = {0};
    cfg.velocity_damping = 1.0f;
    SoftBody2D *body = soft_body2d_create(2);
    if (!body) return 0;
    int a = soft_body2d_add_node(body, vec2(0.0f, 0.0f), 1.0f);
    int b = soft_body2d_add_node(body, vec2(2.0f, 0.0f), 1.0f);
    if (a != 0 || b != 1 || !soft_body2d_add_spring(body, a, b, 10.0f, 0.5f)) {
        soft_body2d_destroy(body);
        return 0;
    }
    body->springs[0].rest_length = 1.0f;
    body->gravity = vec2(0.0f, 0.0f);
    soft_body2d_step(body, 0.1, &cfg);
    int pass = body->nodes[0].position.x > 0.0f && body->nodes[1].position.x < 2.0f;
    soft_body2d_destroy(body);
    return pass;
}

static int test_pinned_node_stays_put(void) {
    AppConfig cfg = {0};
    cfg.velocity_damping = 1.0f;
    SoftBody2D *body = soft_body2d_create(2);
    if (!body) return 0;
    int pinned = soft_body2d_add_node(body, vec2(0.0f, 0.0f), 0.0f);
    int free_node = soft_body2d_add_node(body, vec2(2.0f, 0.0f), 1.0f);
    if (pinned != 0 || free_node != 1 ||
        !soft_body2d_add_spring(body, pinned, free_node, 8.0f, 0.0f)) {
        soft_body2d_destroy(body);
        return 0;
    }
    body->springs[0].rest_length = 1.0f;
    body->gravity = vec2(0.0f, 0.0f);
    soft_body2d_step(body, 0.1, &cfg);
    int pass = nearly_equal(body->nodes[0].position.x, 0.0f) &&
               nearly_equal(body->nodes[0].position.y, 0.0f) &&
               body->nodes[1].position.x < 2.0f;
    soft_body2d_destroy(body);
    return pass;
}

static int test_gravity_accelerates_free_node(void) {
    AppConfig cfg = {0};
    cfg.velocity_damping = 1.0f;
    SoftBody2D *body = soft_body2d_create(1);
    if (!body) return 0;
    if (soft_body2d_add_node(body, vec2(0.0f, 1.0f), 2.0f) != 0) {
        soft_body2d_destroy(body);
        return 0;
    }
    body->gravity = vec2(0.0f, -9.8f);
    soft_body2d_step(body, 0.1, &cfg);
    int pass = body->nodes[0].velocity.y < 0.0f && body->nodes[0].position.y < 1.0f;
    soft_body2d_destroy(body);
    return pass;
}

static int test_constraint_chain_preserves_rest_lengths(void) {
    AppConfig cfg = {0};
    cfg.velocity_damping = 1.0f;
    SoftBody2D *body = soft_body2d_create(3);
    if (!body) return 0;
    body->constraint_iterations = 12;
    body->constraint_stiffness = 1.0f;

    int top = soft_body2d_add_node(body, vec2(0.0f, 0.0f), 0.0f);
    int mid = soft_body2d_add_node(body, vec2(0.0f, -1.0f), 1.0f);
    int bottom = soft_body2d_add_node(body, vec2(0.0f, -2.0f), 1.0f);
    if (top != 0 || mid != 1 || bottom != 2) {
        soft_body2d_destroy(body);
        return 0;
    }
    if (!soft_body2d_add_spring(body, top, mid, 30.0f, 0.25f) ||
        !soft_body2d_add_spring(body, mid, bottom, 30.0f, 0.25f)) {
        soft_body2d_destroy(body);
        return 0;
    }

    body->springs[0].rest_length = 1.0f;
    body->springs[1].rest_length = 1.0f;
    body->gravity = vec2(0.0f, -9.8f);

    for (int i = 0; i < 8; ++i) {
        soft_body2d_step(body, 0.016, &cfg);
    }

    float top_mid = vec2_len(vec2_sub(body->nodes[mid].position, body->nodes[top].position));
    float mid_bottom =
        vec2_len(vec2_sub(body->nodes[bottom].position, body->nodes[mid].position));
    int pass = nearly_equal(body->nodes[top].position.x, 0.0f) &&
               nearly_equal(body->nodes[top].position.y, 0.0f) &&
               fabsf(top_mid - 1.0f) < 0.05f &&
               fabsf(mid_bottom - 1.0f) < 0.05f &&
               body->nodes[bottom].position.y < body->nodes[mid].position.y;
    soft_body2d_destroy(body);
    return pass;
}

static int test_triangle_area_constraint_preserves_shape(void) {
    AppConfig cfg = {0};
    cfg.velocity_damping = 1.0f;
    SoftBody2D *body = soft_body2d_create(3);
    if (!body) return 0;
    body->constraint_iterations = 14;
    body->constraint_stiffness = 1.0f;

    int a = soft_body2d_add_node(body, vec2(-0.5f, 0.0f), 0.0f);
    int b = soft_body2d_add_node(body, vec2(0.5f, 0.0f), 0.0f);
    int c = soft_body2d_add_node(body, vec2(0.0f, -1.0f), 1.0f);
    if (a != 0 || b != 1 || c != 2) {
        soft_body2d_destroy(body);
        return 0;
    }
    if (!soft_body2d_add_spring(body, a, c, 24.0f, 0.2f) ||
        !soft_body2d_add_spring(body, b, c, 24.0f, 0.2f) ||
        !soft_body2d_add_spring(body, a, b, 24.0f, 0.2f) ||
        !soft_body2d_add_area_constraint(body, a, b, c, 1.0f)) {
        soft_body2d_destroy(body);
        return 0;
    }

    float rest_area = fabsf(0.5f * ((body->nodes[b].position.x - body->nodes[a].position.x) *
                                    (body->nodes[c].position.y - body->nodes[a].position.y) -
                                    (body->nodes[b].position.y - body->nodes[a].position.y) *
                                        (body->nodes[c].position.x - body->nodes[a].position.x)));
    body->gravity = vec2(0.0f, -9.8f);

    for (int i = 0; i < 12; ++i) {
        soft_body2d_step(body, 0.016, &cfg);
    }

    float current_area = fabsf(0.5f * ((body->nodes[1].position.x - body->nodes[0].position.x) *
                                       (body->nodes[2].position.y - body->nodes[0].position.y) -
                                       (body->nodes[1].position.y - body->nodes[0].position.y) *
                                           (body->nodes[2].position.x - body->nodes[0].position.x)));
    int pass = nearly_equal(body->nodes[0].position.x, -0.5f) &&
               nearly_equal(body->nodes[1].position.x, 0.5f) &&
               fabsf(current_area - rest_area) < 0.05f &&
               body->nodes[2].position.y < 0.0f;
    soft_body2d_destroy(body);
    return pass;
}

int main(void) {
    if (!test_spring_pulls_nodes_together()) {
        fprintf(stderr, "soft_body_contract_test: spring pull failed\n");
        return 1;
    }
    if (!test_pinned_node_stays_put()) {
        fprintf(stderr, "soft_body_contract_test: pinned node failed\n");
        return 1;
    }
    if (!test_gravity_accelerates_free_node()) {
        fprintf(stderr, "soft_body_contract_test: gravity failed\n");
        return 1;
    }
    if (!test_constraint_chain_preserves_rest_lengths()) {
        fprintf(stderr, "soft_body_contract_test: constraint chain failed\n");
        return 1;
    }
    if (!test_triangle_area_constraint_preserves_shape()) {
        fprintf(stderr, "soft_body_contract_test: triangle area failed\n");
        return 1;
    }
    printf("soft_body_contract_test: success\n");
    return 0;
}
