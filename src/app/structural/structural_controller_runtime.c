#include "app/structural/structural_controller_internal.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void runtime_build_mass_diagonal(const StructuralScene *scene,
                                        StructuralRuntimeView *view) {
    if (!scene || !view || !view->mass || view->dof_count < scene->node_count * 3) return;
    size_t node_count = scene->node_count;
    for (size_t i = 0; i < node_count * 3; ++i) {
        view->mass[i] = 0.0f;
    }

    for (size_t e = 0; e < scene->edge_count; ++e) {
        const StructEdge *edge = &scene->edges[e];
        int idx_a = -1;
        int idx_b = -1;
        for (size_t n = 0; n < scene->node_count; ++n) {
            if (scene->nodes[n].id == edge->a_id) idx_a = (int)n;
            if (scene->nodes[n].id == edge->b_id) idx_b = (int)n;
        }
        if (idx_a < 0 || idx_b < 0) continue;
        const StructNode *a = &scene->nodes[idx_a];
        const StructNode *b = &scene->nodes[idx_b];
        float dx = b->x - a->x;
        float dy = b->y - a->y;
        float L = sqrtf(dx * dx + dy * dy);
        if (L < 1e-4f) continue;

        float density = 1.0f;
        float area = 1.0f;
        if (edge->material_index >= 0 && edge->material_index < (int)scene->material_count) {
            const StructMaterial *mat = &scene->materials[edge->material_index];
            density = mat->density;
            area = mat->area;
        }
        float m_edge = density * area * L;
        float rot_edge = m_edge * (L * L) / 12.0f;

        float m_half = 0.5f * m_edge;
        float r_half = 0.5f * rot_edge;

        view->mass[idx_a * 3 + 0] += m_half;
        view->mass[idx_a * 3 + 1] += m_half;
        view->mass[idx_a * 3 + 2] += r_half;
        view->mass[idx_b * 3 + 0] += m_half;
        view->mass[idx_b * 3 + 1] += m_half;
        view->mass[idx_b * 3 + 2] += r_half;
    }

    {
        const float min_mass = 1e-3f;
        for (size_t i = 0; i < node_count; ++i) {
            for (int d = 0; d < 3; ++d) {
                size_t idx = i * 3 + (size_t)d;
                if (view->mass[idx] < min_mass) {
                    view->mass[idx] = min_mass;
                }
            }
        }
    }
}

static void runtime_build_external_forces(const StructuralScene *scene,
                                          const StructuralRuntimeView *view,
                                          float *f_ext,
                                          size_t dof_count,
                                          float gravity_factor) {
    if (!scene || !view || !f_ext || dof_count == 0) return;
    memset(f_ext, 0, sizeof(float) * dof_count);

    if (scene->gravity_enabled && fabsf(scene->gravity_strength) > 1e-6f && gravity_factor > 0.0f) {
        for (size_t e = 0; e < scene->edge_count; ++e) {
            const StructEdge *edge = &scene->edges[e];
            int idx_a = -1;
            int idx_b = -1;
            for (size_t n = 0; n < scene->node_count; ++n) {
                if (scene->nodes[n].id == edge->a_id) idx_a = (int)n;
                if (scene->nodes[n].id == edge->b_id) idx_b = (int)n;
            }
            if (idx_a < 0 || idx_b < 0) continue;
            {
                const StructNode *a = &scene->nodes[idx_a];
                const StructNode *b = &scene->nodes[idx_b];
                float dx = b->x - a->x;
                float dy = b->y - a->y;
                float L = sqrtf(dx * dx + dy * dy);
                if (L < 1e-4f) continue;
                float c = dx / L;
                float s = dy / L;

                float density = 1.0f;
                float area = 1.0f;
                if (edge->material_index >= 0 && edge->material_index < (int)scene->material_count) {
                    const StructMaterial *mat = &scene->materials[edge->material_index];
                    density = mat->density;
                    area = mat->area;
                }
                {
                    float w = density * area * scene->gravity_strength * gravity_factor;
                    float q_axial = w * s;
                    float q_trans = w * c;
                    float L2 = L * L;
                    float f_local[6] = {
                        q_axial * L * 0.5f,
                        q_trans * L * 0.5f,
                        q_trans * L2 / 12.0f,
                        q_axial * L * 0.5f,
                        q_trans * L * 0.5f,
                        -q_trans * L2 / 12.0f
                    };
                    float T[6][6] = {
                        { c, s, 0.0f, 0.0f, 0.0f, 0.0f },
                        { -s, c, 0.0f, 0.0f, 0.0f, 0.0f },
                        { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },
                        { 0.0f, 0.0f, 0.0f, c, s, 0.0f },
                        { 0.0f, 0.0f, 0.0f, -s, c, 0.0f },
                        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }
                    };
                    float f_global[6] = {0};
                    for (int i = 0; i < 6; ++i) {
                        float sum = 0.0f;
                        for (int k = 0; k < 6; ++k) {
                            sum += T[k][i] * f_local[k];
                        }
                        f_global[i] = sum;
                    }
                    f_ext[idx_a * 3 + 0] += f_global[0];
                    f_ext[idx_a * 3 + 1] += f_global[1];
                    f_ext[idx_a * 3 + 2] += f_global[2];
                    f_ext[idx_b * 3 + 0] += f_global[3];
                    f_ext[idx_b * 3 + 1] += f_global[4];
                    f_ext[idx_b * 3 + 2] += f_global[5];
                }
            }
        }
    }

    for (size_t i = 0; i < scene->load_count; ++i) {
        const StructLoad *load = &scene->loads[i];
        if (load->case_id != scene->active_load_case) continue;
        {
            int idx = -1;
            for (size_t n = 0; n < scene->node_count; ++n) {
                if (scene->nodes[n].id == load->node_id) {
                    idx = (int)n;
                    break;
                }
            }
            if (idx < 0) continue;
            f_ext[idx * 3 + 0] += load->fx;
            f_ext[idx * 3 + 1] += load->fy;
            f_ext[idx * 3 + 2] += load->mz;
        }
    }
}

static void runtime_zero_constrained(const StructuralScene *scene,
                                     float *vec,
                                     size_t dof_count) {
    if (!scene || !vec || dof_count < scene->node_count * 3) return;
    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        if (node->fixed_x) vec[i * 3 + 0] = 0.0f;
        if (node->fixed_y) vec[i * 3 + 1] = 0.0f;
        if (node->fixed_theta) vec[i * 3 + 2] = 0.0f;
    }
}

static void runtime_build_damping_force(const StructuralScene *scene,
                                        const StructuralRuntimeView *view,
                                        float alpha,
                                        float beta,
                                        const float *vel,
                                        float *out_force,
                                        size_t dof_count) {
    if (!scene || !view || !vel || !out_force || dof_count == 0) return;
    memset(out_force, 0, sizeof(float) * dof_count);
    if (fabsf(alpha) > 1e-6f) {
        for (size_t i = 0; i < dof_count; ++i) {
            out_force[i] += alpha * view->mass[i] * vel[i];
        }
    }
    if (fabsf(beta) > 1e-6f) {
        float kv[STRUCT_MAX_NODES * 3] = {0};
        structural_apply_frame_stiffness(scene, vel, kv, dof_count);
        for (size_t i = 0; i < dof_count; ++i) {
            out_force[i] += beta * kv[i];
        }
    }
}

static void runtime_apply_constraints(const StructuralScene *scene,
                                      StructuralRuntimeView *view) {
    if (!scene || !view || !view->u || !view->v || !view->a) return;
    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        if (node->fixed_x) {
            view->u[i * 3 + 0] = 0.0f;
            view->v[i * 3 + 0] = 0.0f;
            view->a[i * 3 + 0] = 0.0f;
        }
        if (node->fixed_y) {
            view->u[i * 3 + 1] = 0.0f;
            view->v[i * 3 + 1] = 0.0f;
            view->a[i * 3 + 1] = 0.0f;
        }
        if (node->fixed_theta) {
            view->u[i * 3 + 2] = 0.0f;
            view->v[i * 3 + 2] = 0.0f;
            view->a[i * 3 + 2] = 0.0f;
        }
    }
}

static void runtime_limit_step(StructuralRuntimeView *view,
                               const float *prev_u,
                               size_t node_count,
                               float dt) {
    if (!view || !prev_u || dt <= 0.0f) return;
    {
        const float max_trans = 8.0f;
        const float max_rot = 0.25f;
        for (size_t i = 0; i < node_count; ++i) {
            size_t base = i * 3;
            float dx = view->u[base + 0] - prev_u[base + 0];
            float dy = view->u[base + 1] - prev_u[base + 1];
            float dtheta = view->u[base + 2] - prev_u[base + 2];
            float len = sqrtf(dx * dx + dy * dy);
            float scale = 1.0f;
            if (len > max_trans && len > 1e-6f) {
                scale = max_trans / len;
            }
            if (scale < 1.0f) {
                dx *= scale;
                dy *= scale;
                view->u[base + 0] = prev_u[base + 0] + dx;
                view->u[base + 1] = prev_u[base + 1] + dy;
                view->v[base + 0] = dx / dt;
                view->v[base + 1] = dy / dt;
                view->a[base + 0] = 0.0f;
                view->a[base + 1] = 0.0f;
            }
            if (fabsf(dtheta) > max_rot) {
                float clamped = (dtheta > 0.0f) ? max_rot : -max_rot;
                view->u[base + 2] = prev_u[base + 2] + clamped;
                view->v[base + 2] = clamped / dt;
                view->a[base + 2] = 0.0f;
            }
        }
    }
}

static void runtime_build_constraint_mask(const StructuralScene *scene,
                                          bool *mask,
                                          size_t dof_count) {
    if (!scene || !mask || dof_count < scene->node_count * 3) return;
    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        mask[i * 3 + 0] = node->fixed_x;
        mask[i * 3 + 1] = node->fixed_y;
        mask[i * 3 + 2] = node->fixed_theta;
    }
}

static size_t runtime_build_dof_map(const StructuralScene *scene,
                                    int *map,
                                    size_t full_count) {
    size_t free_count = 0;
    if (!scene || !map) return 0;
    for (size_t i = 0; i < full_count; ++i) {
        map[i] = -1;
    }
    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        int base = (int)(i * 3);
        if (!node->fixed_x) map[base] = (int)free_count++;
        if (!node->fixed_y) map[base + 1] = (int)free_count++;
        if (!node->fixed_theta) map[base + 2] = (int)free_count++;
    }
    return free_count;
}

static void runtime_compact_vector(const float *full,
                                   const int *map,
                                   size_t full_count,
                                   float *reduced,
                                   size_t reduced_count) {
    if (!full || !map || !reduced) return;
    for (size_t i = 0; i < reduced_count; ++i) reduced[i] = 0.0f;
    for (size_t i = 0; i < full_count; ++i) {
        int idx = map[i];
        if (idx >= 0 && (size_t)idx < reduced_count) {
            reduced[idx] = full[i];
        }
    }
}

static void runtime_expand_vector(const float *reduced,
                                  const int *map,
                                  size_t full_count,
                                  float *full) {
    if (!reduced || !map || !full) return;
    for (size_t i = 0; i < full_count; ++i) {
        int idx = map[i];
        full[i] = (idx >= 0) ? reduced[idx] : 0.0f;
    }
}

static float runtime_dot(const float *a, const float *b, size_t n) {
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

typedef struct RuntimeOperator {
    const StructuralScene *scene;
    const StructuralRuntimeView *view;
    size_t dof_count;
    float dt;
    float alpha;
    float beta;
    float gamma;
} RuntimeOperator;

static void runtime_apply_meff_full(const RuntimeOperator *op,
                                    const float *x,
                                    float *out) {
    if (!op || !x || !out) return;
    {
        float kv[STRUCT_MAX_NODES * 3] = {0};
        structural_apply_frame_stiffness(op->scene, x, kv, op->dof_count);

        float dt = op->dt;
        float alpha = op->alpha;
        float beta = op->beta;
        float gamma = op->gamma;
        for (size_t i = 0; i < op->dof_count; ++i) {
            float m = op->view->mass[i];
            float mx = m * x[i];
            float cx = alpha * m * x[i] + beta * kv[i];
            float kx = kv[i];
            out[i] = mx + gamma * dt * cx + beta * dt * dt * kx;
        }
    }
}

static void runtime_apply_meff_reduced(const RuntimeOperator *op,
                                       const int *map,
                                       size_t full_count,
                                       size_t reduced_count,
                                       const float *x,
                                       float *out) {
    float full_x[STRUCT_MAX_NODES * 3] = {0};
    float full_out[STRUCT_MAX_NODES * 3] = {0};
    runtime_expand_vector(x, map, full_count, full_x);
    runtime_apply_meff_full(op, full_x, full_out);
    runtime_compact_vector(full_out, map, full_count, out, reduced_count);
}

static bool runtime_solve_cg_reduced(const RuntimeOperator *op,
                                     const int *map,
                                     size_t full_count,
                                     size_t reduced_count,
                                     const float *b,
                                     float *x,
                                     int max_iter,
                                     float tol) {
    float r[STRUCT_MAX_NODES * 3] = {0};
    float p[STRUCT_MAX_NODES * 3] = {0};
    float Ap[STRUCT_MAX_NODES * 3] = {0};

    runtime_apply_meff_reduced(op, map, full_count, reduced_count, x, Ap);
    for (size_t i = 0; i < reduced_count; ++i) {
        r[i] = b[i] - Ap[i];
        p[i] = r[i];
    }

    {
        float rsold = runtime_dot(r, r, reduced_count);
        if (rsold < tol * tol) return true;

        for (int iter = 0; iter < max_iter; ++iter) {
            runtime_apply_meff_reduced(op, map, full_count, reduced_count, p, Ap);
            {
                float denom = runtime_dot(p, Ap, reduced_count);
                if (fabsf(denom) < 1e-6f) break;
                float alpha = rsold / denom;
                for (size_t i = 0; i < reduced_count; ++i) {
                    x[i] += alpha * p[i];
                    r[i] -= alpha * Ap[i];
                }
                {
                    float rsnew = runtime_dot(r, r, reduced_count);
                    if (rsnew < tol * tol) return true;
                    {
                        float beta = rsnew / rsold;
                        for (size_t i = 0; i < reduced_count; ++i) {
                            p[i] = r[i] + beta * p[i];
                        }
                    }
                    rsold = rsnew;
                }
            }
        }
    }
    return false;
}

void structural_controller_runtime_view_clear(StructuralRuntimeView *view) {
    if (!view) return;
    free(view->u);
    free(view->v);
    free(view->a);
    free(view->mass);
    view->u = NULL;
    view->v = NULL;
    view->a = NULL;
    view->mass = NULL;
    view->dof_count = 0;
}

void structural_controller_runtime_view_resize(StructuralRuntimeView *view, size_t node_count) {
    if (!view) return;
    {
        size_t dof = node_count * 3;
        if (dof == view->dof_count) return;
        structural_controller_runtime_view_clear(view);
        if (dof == 0) return;
        view->u = (float *)calloc(dof, sizeof(float));
        view->v = (float *)calloc(dof, sizeof(float));
        view->a = (float *)calloc(dof, sizeof(float));
        view->mass = (float *)calloc(dof, sizeof(float));
        view->dof_count = dof;
    }
}

void structural_controller_runtime_view_sync_from_scene(StructuralRuntimeView *view,
                                                        const StructuralScene *scene) {
    if (!view || !scene) return;
    structural_controller_runtime_view_resize(view, scene->node_count);
    if (!view->u) return;
    for (size_t i = 0; i < scene->node_count; ++i) {
        view->u[i * 3 + 0] = scene->disp_x[i];
        view->u[i * 3 + 1] = scene->disp_y[i];
        view->u[i * 3 + 2] = scene->disp_theta[i];
    }
    if (view->v) memset(view->v, 0, sizeof(float) * view->dof_count);
    if (view->a) memset(view->a, 0, sizeof(float) * view->dof_count);
}

void structural_controller_runtime_step_dynamic(StructuralController *ctrl, float dt) {
    if (!ctrl || dt <= 0.0f) return;
    {
        StructuralScene *scene = &ctrl->scene;
        StructuralRuntimeView *view = &ctrl->runtime;
        if (scene->node_count == 0 || scene->edge_count == 0) return;

        structural_controller_runtime_view_resize(view, scene->node_count);
        runtime_build_mass_diagonal(scene, view);

        size_t dof_count = scene->node_count * 3;
        float f_ext[STRUCT_MAX_NODES * 3] = {0};
        float f_int[STRUCT_MAX_NODES * 3] = {0};
        float f_damp[STRUCT_MAX_NODES * 3] = {0};
        bool constrained[STRUCT_MAX_NODES * 3] = {0};
        int dof_map[STRUCT_MAX_NODES * 3];

        float gravity_factor = scene->gravity_enabled ? 1.0f : 0.0f;
        if (scene->gravity_enabled && ctrl->gravity_ramp_enabled && ctrl->gravity_ramp_duration > 0.0f) {
            gravity_factor = fminf(1.0f, ctrl->gravity_ramp_time / ctrl->gravity_ramp_duration);
        }

        runtime_build_external_forces(scene, view, f_ext, dof_count, gravity_factor);
        runtime_zero_constrained(scene, f_ext, dof_count);

        runtime_build_constraint_mask(scene, constrained, dof_count);
        {
            size_t reduced_count = runtime_build_dof_map(scene, dof_map, dof_count);
            float prev_u[STRUCT_MAX_NODES * 3] = {0};
            for (size_t i = 0; i < dof_count; ++i) {
                prev_u[i] = view->u[i];
            }

            if (ctrl->integrator == STRUCT_INTEGRATOR_NEWMARK) {
                float u_pred[STRUCT_MAX_NODES * 3] = {0};
                float v_pred[STRUCT_MAX_NODES * 3] = {0};
                for (size_t i = 0; i < dof_count; ++i) {
                    u_pred[i] = view->u[i] + dt * view->v[i] + dt * dt * (0.5f - ctrl->newmark_beta) * view->a[i];
                    v_pred[i] = view->v[i] + dt * (1.0f - ctrl->newmark_gamma) * view->a[i];
                }
                runtime_zero_constrained(scene, u_pred, dof_count);
                runtime_zero_constrained(scene, v_pred, dof_count);

                {
                    float f_int_pred[STRUCT_MAX_NODES * 3] = {0};
                    structural_apply_frame_stiffness(scene, u_pred, f_int_pred, dof_count);
                    runtime_build_damping_force(scene, view, ctrl->damping_alpha, ctrl->damping_beta,
                                                v_pred, f_damp, dof_count);
                    runtime_zero_constrained(scene, f_int_pred, dof_count);
                    runtime_zero_constrained(scene, f_damp, dof_count);

                    {
                        float rhs[STRUCT_MAX_NODES * 3] = {0};
                        for (size_t i = 0; i < dof_count; ++i) {
                            rhs[i] = f_ext[i] - f_int_pred[i] - f_damp[i];
                        }
                        runtime_zero_constrained(scene, rhs, dof_count);

                        {
                            float a_next[STRUCT_MAX_NODES * 3] = {0};
                            RuntimeOperator op = {
                                .scene = scene,
                                .view = view,
                                .dof_count = dof_count,
                                .dt = dt,
                                .alpha = ctrl->damping_alpha,
                                .beta = ctrl->damping_beta,
                                .gamma = ctrl->newmark_gamma
                            };
                            float rhs_reduced[STRUCT_MAX_NODES * 3] = {0};
                            float a_reduced[STRUCT_MAX_NODES * 3] = {0};
                            if (reduced_count > 0) {
                                runtime_compact_vector(rhs, dof_map, dof_count, rhs_reduced, reduced_count);
                                runtime_solve_cg_reduced(&op, dof_map, dof_count, reduced_count,
                                                         rhs_reduced, a_reduced, 128, 1e-5f);
                                runtime_expand_vector(a_reduced, dof_map, dof_count, a_next);
                            }
                            runtime_zero_constrained(scene, a_next, dof_count);

                            for (size_t i = 0; i < dof_count; ++i) {
                                view->a[i] = a_next[i];
                                view->u[i] = u_pred[i] + ctrl->newmark_beta * dt * dt * a_next[i];
                                view->v[i] = v_pred[i] + ctrl->newmark_gamma * dt * a_next[i];
                            }
                        }
                    }
                }
            } else {
                structural_apply_frame_stiffness(scene, view->u, f_int, dof_count);
                runtime_build_damping_force(scene, view, ctrl->damping_alpha, ctrl->damping_beta,
                                            view->v, f_damp, dof_count);
                runtime_zero_constrained(scene, f_int, dof_count);
                runtime_zero_constrained(scene, f_damp, dof_count);

                for (size_t i = 0; i < dof_count; ++i) {
                    float mass = view->mass[i];
                    float accel = (f_ext[i] - f_int[i] - f_damp[i]) / mass;
                    view->a[i] = accel;
                    view->v[i] += accel * dt;
                    view->u[i] += view->v[i] * dt;
                }
            }

            runtime_limit_step(view, prev_u, scene->node_count, dt);
            runtime_apply_constraints(scene, view);
        }

        for (size_t i = 0; i < scene->node_count; ++i) {
            scene->disp_x[i] = view->u[i * 3 + 0];
            scene->disp_y[i] = view->u[i * 3 + 1];
            scene->disp_theta[i] = view->u[i * 3 + 2];
        }
        structural_compute_frame_internal_forces_ex(scene, view->u, NULL, 0, true);
        scene->has_solution = true;
        if (ctrl->gravity_ramp_enabled) {
            ctrl->gravity_ramp_time += dt;
        }
    }
}
