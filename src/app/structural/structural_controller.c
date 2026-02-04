#include "app/structural/structural_controller.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_vulkan.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input/input.h"
#include "input/input_context.h"
#include "physics/structural/structural_scene.h"
#include "physics/structural/structural_solver.h"
#include "app/structural/structural_render.h"
#include "font_paths.h"
#include "vk_renderer.h"
#include "render/vk_shared_device.h"

typedef struct StructuralRuntimeView {
    float *u;
    float *v;
    float *a;
    float *mass;
    size_t dof_count;
} StructuralRuntimeView;

typedef enum StructuralIntegrator {
    STRUCT_INTEGRATOR_EXPLICIT = 0,
    STRUCT_INTEGRATOR_NEWMARK = 1
} StructuralIntegrator;

typedef struct StructuralController {
    StructuralScene  scene;
    StructuralSolveResult last_result;
    StructuralRuntimeView runtime;
    bool show_constraints;
    bool show_loads;
    bool show_ids;
    bool show_deformed;
    bool show_stress;
    bool show_bending;
    bool show_shear;
    bool show_combined;
    bool scale_use_percentile;
    bool scale_freeze;
    bool scale_initialized;
    bool scale_thickness;
    float scale_gamma;
    float scale_percentile;
    float scale_stress;
    float scale_moment;
    float scale_shear;
    float scale_combined;
    float thickness_gain;
    float deform_scale;
    bool solve_requested;
    bool dynamic_mode;
    bool dynamic_playing;
    bool dynamic_step;
    StructuralIntegrator integrator;
    float time_scale;
    float damping_alpha;
    float damping_beta;
    float newmark_beta;
    float newmark_gamma;
    bool gravity_ramp_enabled;
    float gravity_ramp_time;
    float gravity_ramp_duration;
    float sim_time;
    int window_w;
    int window_h;
    int pointer_x;
    int pointer_y;
    bool running;
    TTF_Font *font_small;
    TTF_Font *font_hud;
    char preset_path[256];
} StructuralController;

static TTF_Font *load_font(int size) {
    const char *paths[] = {
        FONT_BODY_PATH_1,
        FONT_BODY_PATH_2,
        FONT_TITLE_PATH_1,
        FONT_TITLE_PATH_2
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], size);
        if (font) return font;
    }
    return NULL;
}

static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                        int x, int y, SDL_Color color, const char *text) {
    if (!renderer || !font || !text) return;
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Rect dst = {x, y, surface->w, surface->h};
    VkRendererTexture texture = {0};
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                   surface,
                                                   &texture,
                                                   VK_FILTER_LINEAR) == VK_SUCCESS) {
        vk_renderer_draw_texture((VkRenderer *)renderer, &texture, NULL, &dst);
        vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &texture);
    }
    SDL_FreeSurface(surface);
}

static void runtime_view_clear(StructuralRuntimeView *view) {
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

static void runtime_view_resize(StructuralRuntimeView *view, size_t node_count) {
    if (!view) return;
    size_t dof = node_count * 3;
    if (dof == view->dof_count) return;
    runtime_view_clear(view);
    if (dof == 0) return;
    view->u = (float *)calloc(dof, sizeof(float));
    view->v = (float *)calloc(dof, sizeof(float));
    view->a = (float *)calloc(dof, sizeof(float));
    view->mass = (float *)calloc(dof, sizeof(float));
    view->dof_count = dof;
}

static void runtime_view_sync_from_scene(StructuralRuntimeView *view,
                                         const StructuralScene *scene) {
    if (!view || !scene) return;
    runtime_view_resize(view, scene->node_count);
    if (!view->u) return;
    for (size_t i = 0; i < scene->node_count; ++i) {
        view->u[i * 3 + 0] = scene->disp_x[i];
        view->u[i * 3 + 1] = scene->disp_y[i];
        view->u[i * 3 + 2] = scene->disp_theta[i];
    }
    if (view->v) memset(view->v, 0, sizeof(float) * view->dof_count);
    if (view->a) memset(view->a, 0, sizeof(float) * view->dof_count);
}

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

    for (size_t i = 0; i < scene->load_count; ++i) {
        const StructLoad *load = &scene->loads[i];
        if (load->case_id != scene->active_load_case) continue;
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

    float rsold = runtime_dot(r, r, reduced_count);
    if (rsold < tol * tol) return true;

    for (int iter = 0; iter < max_iter; ++iter) {
        runtime_apply_meff_reduced(op, map, full_count, reduced_count, p, Ap);
        float denom = runtime_dot(p, Ap, reduced_count);
        if (fabsf(denom) < 1e-6f) break;
        float alpha = rsold / denom;
        for (size_t i = 0; i < reduced_count; ++i) {
            x[i] += alpha * p[i];
            r[i] -= alpha * Ap[i];
        }
        float rsnew = runtime_dot(r, r, reduced_count);
        if (rsnew < tol * tol) return true;
        float beta = rsnew / rsold;
        for (size_t i = 0; i < reduced_count; ++i) {
            p[i] = r[i] + beta * p[i];
        }
        rsold = rsnew;
    }
    return false;
}

static void runtime_step_dynamic(StructuralController *ctrl, float dt) {
    if (!ctrl || dt <= 0.0f) return;
    StructuralScene *scene = &ctrl->scene;
    StructuralRuntimeView *view = &ctrl->runtime;
    if (scene->node_count == 0 || scene->edge_count == 0) return;

    runtime_view_resize(view, scene->node_count);
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

        float f_int_pred[STRUCT_MAX_NODES * 3] = {0};
        structural_apply_frame_stiffness(scene, u_pred, f_int_pred, dof_count);
        runtime_build_damping_force(scene, view, ctrl->damping_alpha, ctrl->damping_beta,
                                    v_pred, f_damp, dof_count);
        runtime_zero_constrained(scene, f_int_pred, dof_count);
        runtime_zero_constrained(scene, f_damp, dof_count);

        float rhs[STRUCT_MAX_NODES * 3] = {0};
        for (size_t i = 0; i < dof_count; ++i) {
            rhs[i] = f_ext[i] - f_int_pred[i] - f_damp[i];
        }
        runtime_zero_constrained(scene, rhs, dof_count);

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

static void draw_circle(SDL_Renderer *renderer, float cx, float cy, float radius) {
    const int segments = 16;
    for (int i = 0; i < segments; ++i) {
        float a0 = (float)i / (float)segments * 6.2831853f;
        float a1 = (float)(i + 1) / (float)segments * 6.2831853f;
        int x0 = (int)(cx + cosf(a0) * radius);
        int y0 = (int)(cy + sinf(a0) * radius);
        int x1 = (int)(cx + cosf(a1) * radius);
        int y1 = (int)(cy + sinf(a1) * radius);
        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }
}

static void draw_arrow(SDL_Renderer *renderer,
                       float x0, float y0,
                       float x1, float y1) {
    SDL_RenderDrawLine(renderer, (int)x0, (int)y0, (int)x1, (int)y1);
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-3f) return;
    float ux = dx / len;
    float uy = dy / len;
    float arrow = 6.0f;
    float left_x = x1 - ux * arrow - uy * arrow * 0.5f;
    float left_y = y1 - uy * arrow + ux * arrow * 0.5f;
    float right_x = x1 - ux * arrow + uy * arrow * 0.5f;
    float right_y = y1 - uy * arrow - ux * arrow * 0.5f;
    SDL_RenderDrawLine(renderer, (int)x1, (int)y1, (int)left_x, (int)left_y);
    SDL_RenderDrawLine(renderer, (int)x1, (int)y1, (int)right_x, (int)right_y);
}

static float distance_to_segment(float px, float py,
                                 float ax, float ay,
                                 float bx, float by) {
    float dx = bx - ax;
    float dy = by - ay;
    float len2 = dx * dx + dy * dy;
    if (len2 < 1e-6f) {
        float ex = px - ax;
        float ey = py - ay;
        return sqrtf(ex * ex + ey * ey);
    }
    float t = ((px - ax) * dx + (py - ay) * dy) / len2;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float cx = ax + t * dx;
    float cy = ay + t * dy;
    float ex = px - cx;
    float ey = py - cy;
    return sqrtf(ex * ex + ey * ey);
}

static float max_abs_edge_stress(const StructuralScene *scene) {
    float max_val = 0.0f;
    if (!scene) return max_val;
    for (size_t i = 0; i < scene->edge_count; ++i) {
        float v = fabsf(scene->edges[i].axial_stress);
        if (v > max_val) max_val = v;
    }
    return max_val;
}

static float max_abs_edge_moment(const StructuralScene *scene) {
    float max_val = 0.0f;
    if (!scene) return max_val;
    for (size_t i = 0; i < scene->edge_count; ++i) {
        float m1 = fabsf(scene->edges[i].bending_moment_a);
        float m2 = fabsf(scene->edges[i].bending_moment_b);
        if (m1 > max_val) max_val = m1;
        if (m2 > max_val) max_val = m2;
    }
    return max_val;
}

static float max_abs_edge_shear(const StructuralScene *scene) {
    float max_val = 0.0f;
    if (!scene) return max_val;
    for (size_t i = 0; i < scene->edge_count; ++i) {
        float v1 = fabsf(scene->edges[i].shear_force_a);
        float v2 = fabsf(scene->edges[i].shear_force_b);
        if (v1 > max_val) max_val = v1;
        if (v2 > max_val) max_val = v2;
    }
    return max_val;
}

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static int compare_float_asc(const void *a, const void *b) {
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

static float compute_scale_from_values(float *values,
                                       size_t count,
                                       bool use_percentile,
                                       float percentile) {
    if (!values || count == 0) return 1.0f;
    qsort(values, count, sizeof(float), compare_float_asc);
    float p = clamp01(percentile);
    if (p < 0.5f) p = 0.5f;
    size_t idx = (size_t)lroundf((float)(count - 1) * p);
    if (idx >= count) idx = count - 1;
    float scale = use_percentile ? values[idx] : values[count - 1];
    if (scale < 1e-6f) scale = 1.0f;
    return scale;
}

static void compute_edge_scales(const StructuralScene *scene,
                                bool use_percentile,
                                float percentile,
                                float *out_stress,
                                float *out_moment,
                                float *out_shear,
                                float *out_combined) {
    if (!out_stress || !out_moment || !out_shear || !out_combined) return;
    if (!scene || scene->edge_count == 0) {
        *out_stress = 1.0f;
        *out_moment = 1.0f;
        *out_shear = 1.0f;
        *out_combined = 1.0f;
        return;
    }

    size_t count = scene->edge_count;
    float *stress_vals = (float *)malloc(sizeof(float) * count);
    float *moment_vals = (float *)malloc(sizeof(float) * count);
    float *shear_vals = (float *)malloc(sizeof(float) * count);
    float *combined_vals = (float *)malloc(sizeof(float) * count);
    if (!stress_vals || !moment_vals || !shear_vals || !combined_vals) {
        free(stress_vals);
        free(moment_vals);
        free(shear_vals);
        free(combined_vals);
        *out_stress = fmaxf(1.0f, max_abs_edge_stress(scene));
        *out_moment = fmaxf(1.0f, max_abs_edge_moment(scene));
        *out_shear = fmaxf(1.0f, max_abs_edge_shear(scene));
        *out_combined = fmaxf(1.0f, *out_stress);
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        const StructEdge *edge = &scene->edges[i];
        float axial = fabsf(edge->axial_stress);
        float moment = fmaxf(fabsf(edge->bending_moment_a), fabsf(edge->bending_moment_b));
        float shear_avg = 0.5f * (edge->shear_force_a + edge->shear_force_b);
        float shear = fmaxf(fabsf(edge->shear_force_a), fabsf(edge->shear_force_b));
        stress_vals[i] = axial;
        moment_vals[i] = moment;
        shear_vals[i] = shear;
        combined_vals[i] = sqrtf(axial * axial + shear_avg * shear_avg);
    }

    *out_stress = compute_scale_from_values(stress_vals, count, use_percentile, percentile);
    *out_moment = compute_scale_from_values(moment_vals, count, use_percentile, percentile);
    *out_shear = compute_scale_from_values(shear_vals, count, use_percentile, percentile);
    *out_combined = compute_scale_from_values(combined_vals, count, use_percentile, percentile);

    free(stress_vals);
    free(moment_vals);
    free(shear_vals);
    free(combined_vals);
}

static SDL_Color stress_color_with_yield(const StructuralScene *scene,
                                         const StructEdge *edge,
                                         float stress_max,
                                         float gamma) {
    if (!scene || !edge) return (SDL_Color){110, 110, 120, 255};
    SDL_Color base = structural_render_color_diverging(-edge->axial_stress, stress_max, gamma);
    float sigma_y = 0.0f;
    if (edge->material_index >= 0 && edge->material_index < (int)scene->material_count) {
        sigma_y = scene->materials[edge->material_index].sigma_y;
    }
    if (sigma_y > 0.0f) {
        float over = fabsf(edge->axial_stress) - sigma_y;
        if (over > 0.0f) {
            float t = over / fmaxf(1e-6f, sigma_y * 0.5f);
            if (t > 1.0f) t = 1.0f;
            SDL_Color warn = {170, 90, 210, 255};
            SDL_Color out = base;
            out.r = (Uint8)((float)base.r + ((float)warn.r - (float)base.r) * t);
            out.g = (Uint8)((float)base.g + ((float)warn.g - (float)base.g) * t);
            out.b = (Uint8)((float)base.b + ((float)warn.b - (float)base.b) * t);
            return out;
        }
    }
    return base;
}

static void draw_constraints(SDL_Renderer *renderer, const StructNode *node) {
    if (!node) return;
    int x = (int)node->x;
    int y = (int)node->y;
    if (node->fixed_x) {
        SDL_RenderDrawLine(renderer, x - 6, y - 6, x - 6, y + 6);
    }
    if (node->fixed_y) {
        SDL_RenderDrawLine(renderer, x - 6, y + 6, x + 6, y + 6);
    }
    if (node->fixed_theta) {
        draw_circle(renderer, (float)x, (float)y, 6.0f);
    }
}

static void draw_moment_icon(SDL_Renderer *renderer, float cx, float cy, float radius, float moment) {
    if (fabsf(moment) < 1e-4f) return;
    draw_circle(renderer, cx, cy, radius);
    float tip_x = (moment >= 0.0f) ? cx + radius : cx - radius;
    float tip_y = cy;
    float sign = (moment >= 0.0f) ? -1.0f : 1.0f;
    SDL_RenderDrawLine(renderer,
                       (int)tip_x, (int)tip_y,
                       (int)(tip_x - sign * 4.0f), (int)(tip_y - 4.0f));
    SDL_RenderDrawLine(renderer,
                       (int)tip_x, (int)tip_y,
                       (int)(tip_x - sign * 4.0f), (int)(tip_y + 4.0f));
}

static void render_scene(SDL_Renderer *renderer, StructuralController *ctrl) {
    if (!renderer || !ctrl) return;
    StructuralScene *scene = &ctrl->scene;
    int w = ctrl->window_w;
    int h = ctrl->window_h;
    if (w <= 0 || h <= 0) return;

    SDL_SetRenderDrawColor(renderer, 16, 18, 20, 255);
    SDL_Rect clear_rect = {0, 0, w, h};
    SDL_RenderFillRect(renderer, &clear_rect);

    float ground_y = (float)h - scene->ground_offset;
    SDL_SetRenderDrawColor(renderer, 90, 80, 70, 255);
    SDL_RenderDrawLine(renderer, 0, (int)ground_y, w, (int)ground_y);

    if (!ctrl->scale_freeze || !ctrl->scale_initialized) {
        compute_edge_scales(scene,
                            ctrl->scale_use_percentile,
                            ctrl->scale_percentile,
                            &ctrl->scale_stress,
                            &ctrl->scale_moment,
                            &ctrl->scale_shear,
                            &ctrl->scale_combined);
        ctrl->scale_initialized = true;
    }
    float stress_scale = ctrl->scale_stress;
    float moment_scale = ctrl->scale_moment;
    float shear_scale = ctrl->scale_shear;
    float combined_scale = ctrl->scale_combined;

    for (size_t i = 0; i < scene->edge_count; ++i) {
        const StructEdge *edge = &scene->edges[i];
        const StructNode *a = structural_scene_get_node(scene, edge->a_id);
        const StructNode *b = structural_scene_get_node(scene, edge->b_id);
        if (!a || !b) continue;
        SDL_Color c0 = {110, 110, 120, 255};
        SDL_Color c1 = c0;
        float value_mag = 0.0f;
        if (scene->has_solution) {
            if (ctrl->show_bending) {
                c0 = structural_render_color_diverging(-edge->bending_moment_a, moment_scale,
                                                       ctrl->scale_gamma);
                c1 = structural_render_color_diverging(-edge->bending_moment_b, moment_scale,
                                                       ctrl->scale_gamma);
                value_mag = fmaxf(fabsf(edge->bending_moment_a), fabsf(edge->bending_moment_b));
            } else if (ctrl->show_shear) {
                float v = 0.5f * (edge->shear_force_a + edge->shear_force_b);
                SDL_Color base = (SDL_Color){90, 95, 110, 255};
                float mag = fabsf(v);
                float t = mag / fmaxf(1e-6f, shear_scale);
                if (t > 1.0f) t = 1.0f;
                t = powf(t, ctrl->scale_gamma);
                SDL_Color heat = structural_render_color_heat(mag, shear_scale, ctrl->scale_gamma);
                c0.r = (Uint8)((float)base.r + ((float)heat.r - (float)base.r) * t);
                c0.g = (Uint8)((float)base.g + ((float)heat.g - (float)base.g) * t);
                c0.b = (Uint8)((float)base.b + ((float)heat.b - (float)base.b) * t);
                c1 = c0;
                value_mag = fabsf(v);
            } else if (ctrl->show_stress) {
                if (ctrl->show_combined) {
                    float v = 0.5f * (edge->shear_force_a + edge->shear_force_b);
                    float combined = sqrtf(edge->axial_stress * edge->axial_stress + v * v);
                    c0 = structural_render_color_heat(combined, combined_scale, ctrl->scale_gamma);
                    c1 = c0;
                    value_mag = combined;
                } else {
                    c0 = stress_color_with_yield(scene, edge, stress_scale, ctrl->scale_gamma);
                    c1 = c0;
                    value_mag = fabsf(edge->axial_stress);
                }
            }
        }
        if (!scene->has_solution) {
            int palette = edge->material_index % 6;
            SDL_Color colors[6] = {
                {190, 170, 120, 255},
                {140, 190, 210, 255},
                {200, 150, 170, 255},
                {170, 200, 140, 255},
                {180, 160, 210, 255},
                {210, 180, 120, 255}
            };
            c0 = colors[palette];
            c1 = c0;
        }
        float width = 6.0f;
        if (ctrl->scale_thickness && scene->has_solution) {
            float ref = ctrl->show_bending ? moment_scale
                      : ctrl->show_shear ? shear_scale
                      : ctrl->show_combined ? combined_scale
                      : stress_scale;
            if (ref < 1e-6f) ref = 1.0f;
            float t = value_mag / ref;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            t = powf(t, ctrl->scale_gamma);
            width = width * (1.0f + ctrl->thickness_gain * t);
        }
        structural_render_draw_beam(renderer, a->x, a->y, b->x, b->y, width, c0, c1);
        if (edge->release_a || edge->release_b) {
            SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255);
            if (edge->release_a) {
                draw_circle(renderer, a->x, a->y, 5.0f);
            }
            if (edge->release_b) {
                draw_circle(renderer, b->x, b->y, 5.0f);
            }
        }
    }

    if (ctrl->show_deformed && scene->has_solution && !ctrl->dynamic_mode) {
        SDL_SetRenderDrawColor(renderer, 120, 200, 120, 180);
        for (size_t i = 0; i < scene->edge_count; ++i) {
            const StructEdge *edge = &scene->edges[i];
            const StructNode *a = structural_scene_get_node(scene, edge->a_id);
            const StructNode *b = structural_scene_get_node(scene, edge->b_id);
            if (!a || !b) continue;
            int idx_a = -1;
            int idx_b = -1;
            for (size_t n = 0; n < scene->node_count; ++n) {
                if (scene->nodes[n].id == edge->a_id) idx_a = (int)n;
                if (scene->nodes[n].id == edge->b_id) idx_b = (int)n;
            }
            if (idx_a < 0 || idx_b < 0) continue;
            float dx = b->x - a->x;
            float dy = b->y - a->y;
            float L = sqrtf(dx * dx + dy * dy);
            if (L < 1e-4f) continue;
            float c = dx / L;
            float s = dy / L;

            float scale = ctrl->deform_scale;
            float uax = scene->disp_x[idx_a];
            float uay = scene->disp_y[idx_a];
            float ubx = scene->disp_x[idx_b];
            float uby = scene->disp_y[idx_b];
            float tax = scene->disp_theta[idx_a];
            float tbx = scene->disp_theta[idx_b];
            if (ctrl->runtime.u && ctrl->runtime.dof_count >= scene->node_count * 3) {
                uax = ctrl->runtime.u[idx_a * 3 + 0];
                uay = ctrl->runtime.u[idx_a * 3 + 1];
                tax = ctrl->runtime.u[idx_a * 3 + 2];
                ubx = ctrl->runtime.u[idx_b * 3 + 0];
                uby = ctrl->runtime.u[idx_b * 3 + 1];
                tbx = ctrl->runtime.u[idx_b * 3 + 2];
            }
            float u1 = (c * uax + s * uay) * scale;
            float v1 = (-s * uax + c * uay) * scale;
            float u2 = (c * ubx + s * uby) * scale;
            float v2 = (-s * ubx + c * uby) * scale;
            float t1 = tax * scale;
            float t2 = tbx * scale;

            float prev_x = a->x + (0.0f + u1) * c - v1 * s;
            float prev_y = a->y + (0.0f + u1) * s + v1 * c;

            const int segments = 12;
            for (int seg = 1; seg <= segments; ++seg) {
                float xi = (float)seg / (float)segments;
                float x_local = L * xi;
                float n1 = 1.0f - 3.0f * xi * xi + 2.0f * xi * xi * xi;
                float n2 = L * (xi - 2.0f * xi * xi + xi * xi * xi);
                float n3 = 3.0f * xi * xi - 2.0f * xi * xi * xi;
                float n4 = L * (-xi * xi + xi * xi * xi);

                float u = (1.0f - xi) * u1 + xi * u2;
                float v = n1 * v1 + n2 * t1 + n3 * v2 + n4 * t2;

                float gx = a->x + (x_local + u) * c - v * s;
                float gy = a->y + (x_local + u) * s + v * c;

                SDL_RenderDrawLine(renderer, (int)prev_x, (int)prev_y, (int)gx, (int)gy);
                prev_x = gx;
                prev_y = gy;
            }
        }
    }

    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        SDL_SetRenderDrawColor(renderer, 230, 230, 240, 255);
        structural_render_draw_endcap(renderer, node->x, node->y, 4.0f);

        if (ctrl->show_constraints) {
            SDL_SetRenderDrawColor(renderer, 200, 140, 120, 255);
            draw_constraints(renderer, node);
        }

        if (ctrl->show_ids && ctrl->font_small) {
            char label[16];
            snprintf(label, sizeof(label), "%d", node->id);
            SDL_Color color = {200, 200, 200, 255};
            render_text(renderer, ctrl->font_small, (int)node->x + 6, (int)node->y + 6, color, label);
        }
    }

    if (ctrl->show_loads) {
        for (size_t i = 0; i < scene->load_count; ++i) {
            const StructLoad *load = &scene->loads[i];
            if (load->case_id != scene->active_load_case) continue;
            const StructNode *node = structural_scene_get_node(scene, load->node_id);
            if (!node) continue;
            float scale = 10.0f;
            if (fabsf(load->fx) > 1e-4f || fabsf(load->fy) > 1e-4f) {
                SDL_SetRenderDrawColor(renderer, 140, 200, 255, 255);
                draw_arrow(renderer, node->x, node->y, node->x + load->fx * scale, node->y + load->fy * scale);
            }
            if (fabsf(load->mz) > 1e-4f) {
                SDL_SetRenderDrawColor(renderer, 255, 180, 120, 255);
                float radius = 6.0f + fminf(6.0f, fabsf(load->mz) * 4.0f);
                draw_moment_icon(renderer, node->x, node->y, radius, load->mz);
            }
        }
    }

    SDL_Color hud_color = {220, 220, 220, 255};
    int hud_x = 16;
    int hud_y = 16;
    if (ctrl->font_hud) {
        char line[128];
        snprintf(line, sizeof(line), "Mode: Structural");
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Nodes: %zu  Edges: %zu", scene->node_count, scene->edge_count);
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Solve: Space | Reset sim: R");
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Dynamic: E %s | Play: P %s | Step: S",
                 ctrl->dynamic_mode ? "On" : "Off",
                 ctrl->dynamic_playing ? "On" : "Off");
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Integrator: %s (Z)",
                 ctrl->integrator == STRUCT_INTEGRATOR_NEWMARK ? "Newmark" : "Explicit");
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Time scale: %.2f (6/7)", ctrl->time_scale);
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Damping a: %.2f (A/F)", ctrl->damping_alpha);
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Damping b: %.2f (H/J)", ctrl->damping_beta);
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Gravity ramp: %s %.2fs (U/0)",
                 ctrl->gravity_ramp_enabled ? "On" : "Off",
                 ctrl->gravity_ramp_duration);
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Sim time: %.2fs", ctrl->sim_time);
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Overlay: T axial | B bend | V shear | Q combined");
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Scale: %s P%.0f gamma %.2f %s %s",
                 ctrl->scale_use_percentile ? "Pct" : "Max",
                 ctrl->scale_percentile * 100.0f,
                 ctrl->scale_gamma,
                 ctrl->scale_freeze ? "freeze" : "live",
                 ctrl->scale_thickness ? "thick" : "flat");
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        snprintf(line, sizeof(line), "Viz: Q combined | Y pct | G gamma | K freeze | X thick");
        render_text(renderer, ctrl->font_hud, hud_x, hud_y, hud_color, line);
        hud_y += 18;
        if (ctrl->last_result.warning[0]) {
            SDL_Color warn = {255, 180, 100, 255};
            render_text(renderer, ctrl->font_hud, hud_x, hud_y, warn, ctrl->last_result.warning);
            hud_y += 18;
        }
        if (ctrl->last_result.warning[0]) {
            SDL_Color status = {160, 200, 160, 255};
            render_text(renderer, ctrl->font_hud, hud_x, hud_y, status, ctrl->last_result.warning);
        }
    }

    if (ctrl->pointer_x >= 0 && ctrl->pointer_y >= 0) {
        float best = 1e9f;
        const StructEdge *best_edge = NULL;
        const StructNode *best_a = NULL;
        const StructNode *best_b = NULL;
        for (size_t i = 0; i < scene->edge_count; ++i) {
            const StructEdge *edge = &scene->edges[i];
            const StructNode *a = structural_scene_get_node(scene, edge->a_id);
            const StructNode *b = structural_scene_get_node(scene, edge->b_id);
            if (!a || !b) continue;
            float dist = distance_to_segment((float)ctrl->pointer_x,
                                             (float)ctrl->pointer_y,
                                             a->x, a->y,
                                             b->x, b->y);
            if (dist < best) {
                best = dist;
                best_edge = edge;
                best_a = a;
                best_b = b;
            }
        }
        if (best_edge && best <= 12.0f && ctrl->font_small) {
            char line[160];
            int tx = ctrl->pointer_x + 12;
            int ty = ctrl->pointer_y + 12;
            SDL_Color tip = {240, 240, 240, 255};
            SDL_Color dim = {180, 190, 200, 255};
            snprintf(line, sizeof(line), "Edge %d (A %d, B %d)",
                     best_edge->id, best_a->id, best_b->id);
            render_text(renderer, ctrl->font_small, tx, ty, tip, line);
            ty += 16;
            snprintf(line, sizeof(line), "Axial: %.3f  Shear: %.3f",
                     best_edge->axial_stress,
                     0.5f * (best_edge->shear_force_a + best_edge->shear_force_b));
            render_text(renderer, ctrl->font_small, tx, ty, dim, line);
            ty += 16;
            snprintf(line, sizeof(line), "Moment A: %.3f  B: %.3f",
                     best_edge->bending_moment_a,
                     best_edge->bending_moment_b);
            render_text(renderer, ctrl->font_small, tx, ty, dim, line);
        }
    }

}

static void on_pointer_down(void *user, const InputPointerState *state) {
    (void)user;
    (void)state;
}

static void on_pointer_up(void *user, const InputPointerState *state) {
    (void)user;
    (void)state;
}

static void on_pointer_move(void *user, const InputPointerState *state) {
    (void)user;
    (void)state;
}

static void runtime_reset_sim(StructuralController *ctrl) {
    if (!ctrl) return;
    structural_scene_clear_solution(&ctrl->scene);
    runtime_view_sync_from_scene(&ctrl->runtime, &ctrl->scene);
    ctrl->dynamic_playing = false;
    ctrl->dynamic_step = false;
    ctrl->gravity_ramp_time = 0.0f;
    ctrl->sim_time = 0.0f;
}

static void runtime_set_overlay(StructuralController *ctrl, SDL_Keycode key) {
    if (!ctrl) return;
    if (key == SDLK_t) {
        ctrl->show_stress = true;
        ctrl->show_bending = false;
        ctrl->show_shear = false;
        return;
    }
    if (key == SDLK_b) {
        ctrl->show_stress = false;
        ctrl->show_bending = true;
        ctrl->show_shear = false;
        return;
    }
    if (key == SDLK_v) {
        ctrl->show_stress = false;
        ctrl->show_bending = false;
        ctrl->show_shear = true;
        return;
    }
}

static void on_key_down(void *user, SDL_Keycode key, SDL_Keymod mod) {
    StructuralController *ctrl = (StructuralController *)user;
    if (!ctrl) return;
    if (key == SDLK_SPACE || key == SDLK_RETURN) {
        ctrl->solve_requested = true;
        return;
    }
    if (key == SDLK_e) {
        ctrl->dynamic_mode = !ctrl->dynamic_mode;
        ctrl->dynamic_playing = false;
        ctrl->dynamic_step = false;
        runtime_view_sync_from_scene(&ctrl->runtime, &ctrl->scene);
        ctrl->gravity_ramp_time = 0.0f;
        ctrl->sim_time = 0.0f;
        return;
    }
    if (key == SDLK_p) {
        if (ctrl->dynamic_mode) {
            ctrl->dynamic_playing = !ctrl->dynamic_playing;
            if (ctrl->dynamic_playing) {
                ctrl->gravity_ramp_time = 0.0f;
            }
        }
        return;
    }
    if (key == SDLK_z) {
        ctrl->integrator = (ctrl->integrator == STRUCT_INTEGRATOR_EXPLICIT)
                               ? STRUCT_INTEGRATOR_NEWMARK
                               : STRUCT_INTEGRATOR_EXPLICIT;
        return;
    }
    if (key == SDLK_s) {
        if (ctrl->dynamic_mode && !ctrl->dynamic_playing) {
            ctrl->dynamic_step = true;
        }
        return;
    }
    if (key == SDLK_6) {
        ctrl->time_scale = fmaxf(0.1f, ctrl->time_scale - 0.1f);
        return;
    }
    if (key == SDLK_7) {
        ctrl->time_scale = fminf(4.0f, ctrl->time_scale + 0.1f);
        return;
    }
    if (key == SDLK_a) {
        ctrl->damping_alpha = fmaxf(0.0f, ctrl->damping_alpha - 0.02f);
        return;
    }
    if (key == SDLK_f) {
        ctrl->damping_alpha = fminf(2.0f, ctrl->damping_alpha + 0.02f);
        return;
    }
    if (key == SDLK_h) {
        ctrl->damping_beta = fmaxf(0.0f, ctrl->damping_beta - 0.02f);
        return;
    }
    if (key == SDLK_j) {
        ctrl->damping_beta = fminf(2.0f, ctrl->damping_beta + 0.02f);
        return;
    }
    if (key == SDLK_u) {
        ctrl->gravity_ramp_enabled = !ctrl->gravity_ramp_enabled;
        ctrl->gravity_ramp_time = 0.0f;
        return;
    }
    if (key == SDLK_0 && ctrl->dynamic_mode) {
        if (ctrl->gravity_ramp_duration < 0.5f) {
            ctrl->gravity_ramp_duration = 0.5f;
        } else if (ctrl->gravity_ramp_duration < 1.0f) {
            ctrl->gravity_ramp_duration = 1.0f;
        } else if (ctrl->gravity_ramp_duration < 2.0f) {
            ctrl->gravity_ramp_duration = 2.0f;
        } else if (ctrl->gravity_ramp_duration < 4.0f) {
            ctrl->gravity_ramp_duration = 4.0f;
        } else {
            ctrl->gravity_ramp_duration = 0.5f;
        }
        return;
    }
    if (key == SDLK_r) {
        runtime_reset_sim(ctrl);
        return;
    }
    if (key == SDLK_i) {
        ctrl->show_ids = !ctrl->show_ids;
        return;
    }
    if (key == SDLK_c) {
        ctrl->show_constraints = !ctrl->show_constraints;
        return;
    }
    if (key == SDLK_l) {
        ctrl->show_loads = !ctrl->show_loads;
        return;
    }
    if (key == SDLK_o) {
        ctrl->show_deformed = !ctrl->show_deformed;
        return;
    }
    if (key == SDLK_MINUS) {
        ctrl->deform_scale = fmaxf(0.0f, ctrl->deform_scale - 1.0f);
        return;
    }
    if (key == SDLK_EQUALS) {
        ctrl->deform_scale += 1.0f;
        return;
    }
    if (key == SDLK_q) {
        ctrl->show_combined = !ctrl->show_combined;
        return;
    }
    if (key == SDLK_y) {
        ctrl->scale_use_percentile = !ctrl->scale_use_percentile;
        ctrl->scale_initialized = false;
        return;
    }
    if (key == SDLK_k) {
        ctrl->scale_freeze = !ctrl->scale_freeze;
        return;
    }
    if (key == SDLK_g) {
        if (ctrl->scale_gamma > 0.9f) ctrl->scale_gamma = 0.7f;
        else if (ctrl->scale_gamma > 0.6f) ctrl->scale_gamma = 0.5f;
        else if (ctrl->scale_gamma > 0.4f) ctrl->scale_gamma = 0.35f;
        else ctrl->scale_gamma = 1.0f;
        ctrl->scale_initialized = false;
        return;
    }
    if (key == SDLK_x) {
        ctrl->scale_thickness = !ctrl->scale_thickness;
        return;
    }
    if (key == SDLK_t || key == SDLK_b || key == SDLK_v) {
        runtime_set_overlay(ctrl, key);
        return;
    }
    (void)mod;
}

int structural_controller_run(const AppConfig *cfg,
                              const ShapeAssetLibrary *shape_library,
                              const char *preset_path) {
    (void)shape_library;
    if (!cfg) return 1;

    bool sdl_initialized = false;
    bool ttf_initialized = false;
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            fprintf(stderr, "[struct] SDL_Init failed: %s\n", SDL_GetError());
            return 1;
        }
        sdl_initialized = true;
    }
    if (TTF_WasInit() == 0) {
        if (TTF_Init() != 0) {
            fprintf(stderr, "[struct] SDL_ttf init failed: %s\n", TTF_GetError());
            if (sdl_initialized) {
                SDL_Quit();
            }
            return 1;
        }
        ttf_initialized = true;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Physics Sim - Structural Mode",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cfg->window_w, cfg->window_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!window) {
        fprintf(stderr, "[struct] Failed to create window: %s\n", SDL_GetError());
        if (ttf_initialized) {
            TTF_Quit();
        }
        if (sdl_initialized) {
            SDL_Quit();
        }
        return 1;
    }

    VkRenderer renderer_storage;
    SDL_Renderer *renderer = (SDL_Renderer *)&renderer_storage;
    VkRendererConfig vk_cfg;
    vk_renderer_config_set_defaults(&vk_cfg);
    vk_cfg.enable_validation = SDL_FALSE;
    vk_cfg.clear_color[0] = 0.0f;
    vk_cfg.clear_color[1] = 0.0f;
    vk_cfg.clear_color[2] = 0.0f;
    vk_cfg.clear_color[3] = 1.0f;
#if defined(__APPLE__)
    vk_cfg.frames_in_flight = 1;
#endif
#if defined(__APPLE__)
    const bool use_shared_device = true;
#else
    const bool use_shared_device = true;
#endif

    if (use_shared_device) {
        if (!vk_shared_device_init(window, &vk_cfg)) {
            fprintf(stderr, "[struct] Failed to init shared Vulkan device.\n");
            SDL_DestroyWindow(window);
            if (ttf_initialized) {
                TTF_Quit();
            }
            if (sdl_initialized) {
                SDL_Quit();
            }
            return 1;
        }

        VkRendererDevice* shared_device = vk_shared_device_get();
        if (!shared_device) {
            fprintf(stderr, "[struct] Failed to access shared Vulkan device.\n");
            SDL_DestroyWindow(window);
            if (ttf_initialized) {
                TTF_Quit();
            }
            if (sdl_initialized) {
                SDL_Quit();
            }
            return 1;
        }

        if (vk_renderer_init_with_device((VkRenderer *)renderer, shared_device, window, &vk_cfg) != VK_SUCCESS) {
            fprintf(stderr, "[struct] Failed to init Vulkan renderer.\n");
            SDL_DestroyWindow(window);
            if (ttf_initialized) {
                TTF_Quit();
            }
            if (sdl_initialized) {
                SDL_Quit();
            }
            return 1;
        }
        vk_shared_device_acquire();
    } else {
        if (vk_renderer_init((VkRenderer *)renderer, window, &vk_cfg) != VK_SUCCESS) {
            fprintf(stderr, "[struct] Failed to init Vulkan renderer.\n");
            SDL_DestroyWindow(window);
            if (ttf_initialized) {
                TTF_Quit();
            }
            if (sdl_initialized) {
                SDL_Quit();
            }
            return 1;
        }
    }

    StructuralController ctrl = {0};
    structural_scene_init(&ctrl.scene);
    runtime_view_resize(&ctrl.runtime, ctrl.scene.node_count);
    ctrl.show_constraints = true;
    ctrl.show_loads = true;
    ctrl.show_ids = false;
    ctrl.show_deformed = true;
    ctrl.show_stress = true;
    ctrl.show_bending = false;
    ctrl.show_shear = false;
    ctrl.show_combined = false;
    ctrl.scale_use_percentile = true;
    ctrl.scale_freeze = false;
    ctrl.scale_initialized = false;
    ctrl.scale_thickness = true;
    ctrl.scale_gamma = 0.6f;
    ctrl.scale_percentile = 0.95f;
    ctrl.scale_stress = 0.0f;
    ctrl.scale_moment = 0.0f;
    ctrl.scale_shear = 0.0f;
    ctrl.scale_combined = 0.0f;
    ctrl.thickness_gain = 0.6f;
    ctrl.deform_scale = 10.0f;
    ctrl.time_scale = 1.0f;
    ctrl.damping_alpha = 0.1f;
    ctrl.damping_beta = 0.1f;
    ctrl.newmark_beta = 0.25f;
    ctrl.newmark_gamma = 0.5f;
    ctrl.gravity_ramp_duration = 1.0f;
    ctrl.font_small = load_font(12);
    ctrl.font_hud = load_font(14);
    ctrl.running = true;
    SDL_GetWindowSize(window, &ctrl.window_w, &ctrl.window_h);
    vk_renderer_set_logical_size((VkRenderer *)renderer,
                                 (float)ctrl.window_w,
                                 (float)ctrl.window_h);
    if (preset_path && preset_path[0] != '\0') {
        snprintf(ctrl.preset_path, sizeof(ctrl.preset_path), "%s", preset_path);
        if (!structural_scene_load(&ctrl.scene, ctrl.preset_path)) {
            snprintf(ctrl.last_result.warning, sizeof(ctrl.last_result.warning),
                     "Preset load failed.");
        }
        runtime_view_resize(&ctrl.runtime, ctrl.scene.node_count);
    }

    InputContextManager ctx_mgr;
    input_context_manager_init(&ctx_mgr);
    InputContext ctx = {
        .on_pointer_down = on_pointer_down,
        .on_pointer_up = on_pointer_up,
        .on_pointer_move = on_pointer_move,
        .on_key_down = on_key_down,
        .user_data = &ctrl
    };
    input_context_manager_push(&ctx_mgr, &ctx);

    Uint32 last_ticks = SDL_GetTicks();
    while (ctrl.running) {
        InputCommands cmds;
        bool running = input_poll_events(&cmds, NULL, &ctx_mgr);
        if (!running || cmds.quit) {
            ctrl.running = false;
        }

        Uint32 now = SDL_GetTicks();
        float dt = (float)(now - last_ticks) / 1000.0f;
        last_ticks = now;
        if (dt > 0.05f) dt = 0.05f;
        dt *= ctrl.time_scale;

        if (ctrl.solve_requested) {
            ctrl.solve_requested = false;
            StructuralSolveResult result = {0};
            bool ok = structural_solve_frame(&ctrl.scene, &result);
            ctrl.last_result = result;
            if (ok) {
                snprintf(ctrl.last_result.warning, sizeof(ctrl.last_result.warning),
                         "Solve ok (%d iters, r=%.3f).", result.iterations, result.residual);
                runtime_view_sync_from_scene(&ctrl.runtime, &ctrl.scene);
            }
        }

        if (ctrl.dynamic_mode && (ctrl.dynamic_playing || ctrl.dynamic_step)) {
            runtime_step_dynamic(&ctrl, dt);
            ctrl.sim_time += dt;
            ctrl.dynamic_step = false;
        }

        SDL_GetMouseState(&ctrl.pointer_x, &ctrl.pointer_y);
        SDL_GetWindowSize(window, &ctrl.window_w, &ctrl.window_h);
        if (ctrl.window_w > 0 && ctrl.window_h > 0) {
            int drawable_w = ctrl.window_w;
            int drawable_h = ctrl.window_h;
            SDL_Vulkan_GetDrawableSize(window, &drawable_w, &drawable_h);
            if (drawable_w <= 0 || drawable_h <= 0) {
                SDL_Delay(16);
                continue;
            }
            VkExtent2D swap_extent = ((VkRenderer *)renderer)->context.swapchain.extent;
            if ((uint32_t)drawable_w != swap_extent.width ||
                (uint32_t)drawable_h != swap_extent.height) {
                vk_renderer_recreate_swapchain((VkRenderer *)renderer, window);
                vk_renderer_set_logical_size((VkRenderer *)renderer,
                                             (float)ctrl.window_w,
                                             (float)ctrl.window_h);
                SDL_Delay(8);
                continue;
            }
            VkCommandBuffer cmd = VK_NULL_HANDLE;
            VkFramebuffer fb = VK_NULL_HANDLE;
            VkExtent2D extent = {0};
            VkResult frame = vk_renderer_begin_frame((VkRenderer *)renderer, &cmd, &fb, &extent);
            if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
                vk_renderer_recreate_swapchain((VkRenderer *)renderer, window);
                vk_renderer_set_logical_size((VkRenderer *)renderer,
                                             (float)ctrl.window_w,
                                             (float)ctrl.window_h);
            } else if (frame == VK_ERROR_DEVICE_LOST) {
                static int logged_device_lost = 0;
                if (!logged_device_lost) {
                    fprintf(stderr, "[struct] Vulkan device lost; exiting structural mode.\n");
                    logged_device_lost = 1;
                }
                if (use_shared_device) {
                    vk_shared_device_mark_lost();
                }
                ctrl.running = false;
                break;
            } else if (frame == VK_SUCCESS) {
                vk_renderer_set_logical_size((VkRenderer *)renderer,
                                             (float)ctrl.window_w,
                                             (float)ctrl.window_h);
                render_scene(renderer, &ctrl);
                VkResult end = vk_renderer_end_frame((VkRenderer *)renderer, cmd);
                if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
                    vk_renderer_recreate_swapchain((VkRenderer *)renderer, window);
                    vk_renderer_set_logical_size((VkRenderer *)renderer,
                                                 (float)ctrl.window_w,
                                                 (float)ctrl.window_h);
                } else if (end == VK_ERROR_DEVICE_LOST) {
                    static int logged_device_lost_end = 0;
                    if (!logged_device_lost_end) {
                        fprintf(stderr, "[struct] Vulkan device lost at end; exiting structural mode.\n");
                        logged_device_lost_end = 1;
                    }
                    if (use_shared_device) {
                        vk_shared_device_mark_lost();
                    }
                    ctrl.running = false;
                    break;
                } else if (end != VK_SUCCESS) {
                    fprintf(stderr, "[struct] vk_renderer_end_frame failed: %d\n", end);
                }
            } else {
                fprintf(stderr, "[struct] vk_renderer_begin_frame failed: %d\n", frame);
            }
        }
        SDL_Delay(8);
    }

    runtime_view_clear(&ctrl.runtime);
    if (ctrl.font_small) TTF_CloseFont(ctrl.font_small);
    if (ctrl.font_hud) TTF_CloseFont(ctrl.font_hud);
    vk_renderer_wait_idle((VkRenderer *)renderer);
    if (use_shared_device) {
        vk_renderer_shutdown_surface((VkRenderer *)renderer);
        vk_shared_device_release();
    } else {
        vk_renderer_shutdown((VkRenderer *)renderer);
    }
    SDL_DestroyWindow(window);
    return 0;
}
