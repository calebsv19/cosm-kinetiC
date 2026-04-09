#include "app/structural/structural_controller_internal.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static size_t find_node_index(const StructuralScene *scene, int node_id) {
    if (!scene) return scene->node_count;
    for (size_t i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].id == node_id) return i;
    }
    return scene->node_count;
}

static bool near_zero(float v) {
    return fabsf(v) <= 1e-5f;
}

static bool test_explicit_integrator_runtime_contract(void) {
    StructuralController ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    structural_scene_init(&ctrl.scene);

    int n0 = structural_scene_add_node(&ctrl.scene, 0.0f, 0.0f);
    int n1 = structural_scene_add_node(&ctrl.scene, 2.0f, 0.0f);
    if (n0 < 0 || n1 < 0) return false;
    if (structural_scene_add_edge(&ctrl.scene, n0, n1) < 0) return false;
    if (structural_scene_add_load(&ctrl.scene, n1, 0.0f, -30.0f, 0.0f, ctrl.scene.active_load_case) < 0) {
        return false;
    }

    StructNode *anchor = structural_scene_get_node(&ctrl.scene, n0);
    if (!anchor) return false;
    anchor->fixed_x = true;
    anchor->fixed_y = true;
    anchor->fixed_theta = true;

    ctrl.integrator = STRUCT_INTEGRATOR_EXPLICIT;
    ctrl.damping_alpha = 0.05f;
    ctrl.damping_beta = 0.03f;
    ctrl.gravity_ramp_enabled = false;

    structural_controller_runtime_view_sync_from_scene(&ctrl.runtime, &ctrl.scene);
    structural_controller_runtime_step_dynamic(&ctrl, 1.0f / 60.0f);

    if (!ctrl.scene.has_solution) return false;
    if (ctrl.runtime.dof_count != ctrl.scene.node_count * 3) return false;

    {
        size_t idx = find_node_index(&ctrl.scene, n0);
        if (idx >= ctrl.scene.node_count) return false;
        if (!near_zero(ctrl.scene.disp_x[idx])) return false;
        if (!near_zero(ctrl.scene.disp_y[idx])) return false;
        if (!near_zero(ctrl.scene.disp_theta[idx])) return false;
    }

    structural_controller_runtime_view_clear(&ctrl.runtime);
    return true;
}

static bool test_newmark_integrator_runtime_contract(void) {
    StructuralController ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    structural_scene_init(&ctrl.scene);

    int n0 = structural_scene_add_node(&ctrl.scene, 0.0f, 0.0f);
    int n1 = structural_scene_add_node(&ctrl.scene, 1.5f, 0.0f);
    int n2 = structural_scene_add_node(&ctrl.scene, 3.0f, 0.0f);
    if (n0 < 0 || n1 < 0 || n2 < 0) return false;
    if (structural_scene_add_edge(&ctrl.scene, n0, n1) < 0) return false;
    if (structural_scene_add_edge(&ctrl.scene, n1, n2) < 0) return false;
    if (structural_scene_add_load(&ctrl.scene, n2, 0.0f, -45.0f, 0.0f, ctrl.scene.active_load_case) < 0) {
        return false;
    }

    StructNode *anchor = structural_scene_get_node(&ctrl.scene, n0);
    if (!anchor) return false;
    anchor->fixed_x = true;
    anchor->fixed_y = true;
    anchor->fixed_theta = true;

    ctrl.integrator = STRUCT_INTEGRATOR_NEWMARK;
    ctrl.newmark_beta = 0.25f;
    ctrl.newmark_gamma = 0.5f;
    ctrl.damping_alpha = 0.05f;
    ctrl.damping_beta = 0.02f;
    ctrl.gravity_ramp_enabled = true;
    ctrl.gravity_ramp_duration = 1.0f;
    ctrl.gravity_ramp_time = 0.0f;

    structural_controller_runtime_view_sync_from_scene(&ctrl.runtime, &ctrl.scene);
    structural_controller_runtime_step_dynamic(&ctrl, 1.0f / 120.0f);

    if (!ctrl.scene.has_solution) return false;
    if (ctrl.gravity_ramp_time <= 0.0f) return false;

    for (size_t i = 0; i < ctrl.runtime.dof_count; ++i) {
        if (!isfinite(ctrl.runtime.u[i])) return false;
        if (!isfinite(ctrl.runtime.v[i])) return false;
        if (!isfinite(ctrl.runtime.a[i])) return false;
    }

    {
        size_t idx = find_node_index(&ctrl.scene, n0);
        if (idx >= ctrl.scene.node_count) return false;
        if (!near_zero(ctrl.scene.disp_x[idx])) return false;
        if (!near_zero(ctrl.scene.disp_y[idx])) return false;
        if (!near_zero(ctrl.scene.disp_theta[idx])) return false;
    }

    structural_controller_runtime_view_clear(&ctrl.runtime);
    return true;
}

int main(void) {
    if (!test_explicit_integrator_runtime_contract()) {
        fprintf(stderr, "structural_runtime_split_contract_test: explicit-integrator contract failed\n");
        return 1;
    }
    if (!test_newmark_integrator_runtime_contract()) {
        fprintf(stderr, "structural_runtime_split_contract_test: newmark-integrator contract failed\n");
        return 1;
    }
    fprintf(stdout, "structural_runtime_split_contract_test: success\n");
    return 0;
}
