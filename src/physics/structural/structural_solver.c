#include "physics/structural/structural_solver.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MAX_DOF (STRUCT_MAX_NODES * 3)
#define MAX_ENTRIES (STRUCT_MAX_EDGES * 36)

typedef struct SparseTriplet {
    int   row;
    int   col;
    float value;
} SparseTriplet;

typedef struct SparseMatrix {
    int n;
    int nnz;
    int row_offsets[MAX_DOF + 1];
    int col_indices[MAX_ENTRIES];
    float values[MAX_ENTRIES];
} SparseMatrix;

static void sparse_matrix_init(SparseMatrix *mat, int n) {
    if (!mat) return;
    mat->n = n;
    mat->nnz = 0;
    memset(mat->row_offsets, 0, sizeof(mat->row_offsets));
}

static bool sparse_matrix_from_triplets(SparseMatrix *mat,
                                        const SparseTriplet *trips,
                                        int trip_count,
                                        int n) {
    if (!mat || !trips || n <= 0) return false;
    if (trip_count < 0 || trip_count > MAX_ENTRIES) return false;

    sparse_matrix_init(mat, n);

    int counts[MAX_DOF] = {0};
    for (int i = 0; i < trip_count; ++i) {
        int row = trips[i].row;
        if (row < 0 || row >= n) continue;
        counts[row]++;
    }

    mat->row_offsets[0] = 0;
    for (int i = 0; i < n; ++i) {
        mat->row_offsets[i + 1] = mat->row_offsets[i] + counts[i];
    }
    int total = mat->row_offsets[n];
    if (total > MAX_ENTRIES) return false;
    mat->nnz = total;

    int offsets[MAX_DOF];
    for (int i = 0; i < n; ++i) {
        offsets[i] = mat->row_offsets[i];
    }

    for (int i = 0; i < trip_count; ++i) {
        int row = trips[i].row;
        int col = trips[i].col;
        float val = trips[i].value;
        if (row < 0 || row >= n || col < 0 || col >= n) continue;
        int dst = offsets[row]++;
        if (dst >= MAX_ENTRIES) return false;
        mat->col_indices[dst] = col;
        mat->values[dst] = val;
    }
    return true;
}

static void sparse_matrix_mul(const SparseMatrix *mat, const float *x, float *out) {
    if (!mat || !x || !out) return;
    for (int i = 0; i < mat->n; ++i) {
        float sum = 0.0f;
        int start = mat->row_offsets[i];
        int end = mat->row_offsets[i + 1];
        for (int j = start; j < end; ++j) {
            sum += mat->values[j] * x[mat->col_indices[j]];
        }
        out[i] = sum;
    }
}

static float vec_dot(const float *a, const float *b, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

static void vec_axpy(float *y, const float *x, float a, int n) {
    for (int i = 0; i < n; ++i) {
        y[i] += a * x[i];
    }
}

static void vec_copy(float *dst, const float *src, int n) {
    memcpy(dst, src, (size_t)n * sizeof(float));
}

static bool solve_cg(const SparseMatrix *mat,
                     const float *b,
                     float *x,
                     int n,
                     int max_iter,
                     float tol,
                     int *out_iter,
                     float *out_residual) {
    if (!mat || !b || !x || n <= 0) return false;

    float r[MAX_DOF] = {0};
    float p[MAX_DOF] = {0};
    float ap[MAX_DOF] = {0};

    for (int i = 0; i < n; ++i) {
        x[i] = 0.0f;
    }

    sparse_matrix_mul(mat, x, ap);
    for (int i = 0; i < n; ++i) {
        r[i] = b[i] - ap[i];
    }
    vec_copy(p, r, n);

    float rsold = vec_dot(r, r, n);
    if (rsold < tol * tol) {
        if (out_iter) *out_iter = 0;
        if (out_residual) *out_residual = sqrtf(rsold);
        return true;
    }

    for (int iter = 0; iter < max_iter; ++iter) {
        sparse_matrix_mul(mat, p, ap);
        float denom = vec_dot(p, ap, n);
        if (fabsf(denom) < 1e-8f) {
            if (out_iter) *out_iter = iter;
            if (out_residual) *out_residual = sqrtf(rsold);
            return false;
        }
        float alpha = rsold / denom;
        vec_axpy(x, p, alpha, n);
        for (int i = 0; i < n; ++i) {
            r[i] -= alpha * ap[i];
        }
        float rsnew = vec_dot(r, r, n);
        if (sqrtf(rsnew) < tol) {
            if (out_iter) *out_iter = iter + 1;
            if (out_residual) *out_residual = sqrtf(rsnew);
            return true;
        }
        float beta = rsnew / rsold;
        for (int i = 0; i < n; ++i) {
            p[i] = r[i] + beta * p[i];
        }
        rsold = rsnew;
    }

    if (out_iter) *out_iter = max_iter;
    if (out_residual) *out_residual = sqrtf(rsold);
    return false;
}

static void fill_warning(StructuralSolveResult *result, const char *msg) {
    if (!result) return;
    result->success = false;
    result->singular = true;
    result->iterations = 0;
    result->residual = 0.0f;
    if (msg) {
        snprintf(result->warning, sizeof(result->warning), "%s", msg);
    } else {
        result->warning[0] = '\0';
    }
}

static bool node_has_theta_stiffness(const StructuralScene *scene, size_t node_index) {
    if (!scene || node_index >= scene->node_count) return false;
    int node_id = scene->nodes[node_index].id;
    for (size_t e = 0; e < scene->edge_count; ++e) {
        const StructEdge *edge = &scene->edges[e];
        if (edge->a_id == node_id) {
            if (!edge->release_a) return true;
        } else if (edge->b_id == node_id) {
            if (!edge->release_b) return true;
        }
    }
    return false;
}

static void apply_frame_releases(float k_local[6][6],
                                 bool release_a,
                                 bool release_b) {
    if (release_a) {
        for (int i = 0; i < 6; ++i) {
            k_local[2][i] = 0.0f;
            k_local[i][2] = 0.0f;
        }
    }
    if (release_b) {
        for (int i = 0; i < 6; ++i) {
            k_local[5][i] = 0.0f;
            k_local[i][5] = 0.0f;
        }
    }
}

static void add_frame_gravity_loads(const StructuralScene *scene,
                                    const int *dof_map,
                                    float *b) {
    if (!scene || !dof_map || !b) return;
    if (!scene->gravity_enabled || fabsf(scene->gravity_strength) < 1e-6f) return;

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
        const StructNode *bnode = &scene->nodes[idx_b];
        float dx = bnode->x - a->x;
        float dy = bnode->y - a->y;
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
        float w = density * area * scene->gravity_strength;
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

        int dofs[6] = {
            dof_map[idx_a * 3],
            dof_map[idx_a * 3 + 1],
            dof_map[idx_a * 3 + 2],
            dof_map[idx_b * 3],
            dof_map[idx_b * 3 + 1],
            dof_map[idx_b * 3 + 2]
        };

        for (int i = 0; i < 6; ++i) {
            if (dofs[i] >= 0) {
                b[dofs[i]] += f_global[i];
            }
        }
    }
}

bool structural_solve_truss(StructuralScene *scene, StructuralSolveResult *result) {
    if (!scene) return false;

    StructuralSolveResult local = {0};
    if (!result) result = &local;
    result->warning[0] = '\0';

    if (scene->node_count == 0 || scene->edge_count == 0) {
        fill_warning(result, "No nodes/edges to solve.");
        structural_scene_clear_solution(scene);
        return false;
    }

    int dof_map[MAX_DOF];
    int dof_count = 0;
    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        int base = (int)i * 2;
        dof_map[base] = node->fixed_x ? -1 : dof_count++;
        dof_map[base + 1] = node->fixed_y ? -1 : dof_count++;
    }

    if (dof_count <= 0) {
        fill_warning(result, "All DOFs are fixed.");
        structural_scene_clear_solution(scene);
        return false;
    }

    float b[MAX_DOF] = {0};
    for (size_t i = 0; i < scene->load_count; ++i) {
        const StructLoad *load = &scene->loads[i];
        if (load->case_id != scene->active_load_case) continue;
        int node_index = -1;
        for (size_t n = 0; n < scene->node_count; ++n) {
            if (scene->nodes[n].id == load->node_id) {
                node_index = (int)n;
                break;
            }
        }
        if (node_index < 0) continue;
        int dof_x = dof_map[node_index * 2];
        int dof_y = dof_map[node_index * 2 + 1];
        if (dof_x >= 0) b[dof_x] += load->fx;
        if (dof_y >= 0) b[dof_y] += load->fy;
    }

    SparseTriplet trips[MAX_ENTRIES];
    int trip_count = 0;

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
        const StructNode *bnode = &scene->nodes[idx_b];
        float dx = bnode->x - a->x;
        float dy = bnode->y - a->y;
        float L = sqrtf(dx * dx + dy * dy);
        if (L < 1e-4f) continue;
        float c = dx / L;
        float s = dy / L;

        float E = 1.0f;
        float A = 1.0f;
        if (edge->material_index >= 0 && edge->material_index < (int)scene->material_count) {
            const StructMaterial *mat = &scene->materials[edge->material_index];
            E = mat->youngs_modulus;
            A = mat->area;
        }
        float k = (E * A) / L;

        float k_local[4][4] = {
            { c * c,  c * s, -c * c, -c * s },
            { c * s,  s * s, -c * s, -s * s },
            {-c * c, -c * s,  c * c,  c * s },
            {-c * s, -s * s,  c * s,  s * s }
        };

        int dofs[4] = {
            dof_map[idx_a * 2],
            dof_map[idx_a * 2 + 1],
            dof_map[idx_b * 2],
            dof_map[idx_b * 2 + 1]
        };

        for (int r = 0; r < 4; ++r) {
            if (dofs[r] < 0) continue;
            for (int cidx = 0; cidx < 4; ++cidx) {
                if (dofs[cidx] < 0) continue;
                if (trip_count >= MAX_ENTRIES) break;
                trips[trip_count++] = (SparseTriplet){
                    .row = dofs[r],
                    .col = dofs[cidx],
                    .value = k * k_local[r][cidx]
                };
            }
        }
    }

    SparseMatrix K;
    if (!sparse_matrix_from_triplets(&K, trips, trip_count, dof_count)) {
        fill_warning(result, "Stiffness assembly failed.");
        structural_scene_clear_solution(scene);
        return false;
    }

    float x[MAX_DOF] = {0};
    int iterations = 0;
    float residual = 0.0f;
    bool ok = solve_cg(&K, b, x, dof_count, 512, 1e-4f, &iterations, &residual);
    result->success = ok;
    result->singular = !ok;
    result->iterations = iterations;
    result->residual = residual;

    if (!ok) {
        snprintf(result->warning, sizeof(result->warning),
                 "Solve failed (singular or ill-conditioned).");
        structural_scene_clear_solution(scene);
        return false;
    }

    for (size_t i = 0; i < scene->node_count; ++i) {
        int dof_x = dof_map[(int)i * 2];
        int dof_y = dof_map[(int)i * 2 + 1];
        scene->disp_x[i] = (dof_x >= 0) ? x[dof_x] : 0.0f;
        scene->disp_y[i] = (dof_y >= 0) ? x[dof_y] : 0.0f;
    }
    scene->has_solution = true;

    for (size_t e = 0; e < scene->edge_count; ++e) {
        StructEdge *edge = &scene->edges[e];
        int idx_a = -1;
        int idx_b = -1;
        for (size_t n = 0; n < scene->node_count; ++n) {
            if (scene->nodes[n].id == edge->a_id) idx_a = (int)n;
            if (scene->nodes[n].id == edge->b_id) idx_b = (int)n;
        }
        if (idx_a < 0 || idx_b < 0) continue;
        const StructNode *a = &scene->nodes[idx_a];
        const StructNode *bnode = &scene->nodes[idx_b];
        float dx = bnode->x - a->x;
        float dy = bnode->y - a->y;
        float L = sqrtf(dx * dx + dy * dy);
        if (L < 1e-4f) {
            edge->axial_force = 0.0f;
            edge->axial_stress = 0.0f;
            continue;
        }
        float c = dx / L;
        float s = dy / L;

        float E = 1.0f;
        float A = 1.0f;
        if (edge->material_index >= 0 && edge->material_index < (int)scene->material_count) {
            const StructMaterial *mat = &scene->materials[edge->material_index];
            E = mat->youngs_modulus;
            A = mat->area;
        }
        float k = (E * A) / L;

        float dux = scene->disp_x[idx_b] - scene->disp_x[idx_a];
        float duy = scene->disp_y[idx_b] - scene->disp_y[idx_a];
        float axial = dux * c + duy * s;
        edge->axial_force = k * axial;
        edge->axial_stress = (A > 0.0f) ? edge->axial_force / A : 0.0f;
    }

    return true;
}

bool structural_solve_frame(StructuralScene *scene, StructuralSolveResult *result) {
    if (!scene) return false;

    StructuralSolveResult local = {0};
    if (!result) result = &local;
    result->warning[0] = '\0';

    if (scene->node_count == 0 || scene->edge_count == 0) {
        fill_warning(result, "No nodes/edges to solve.");
        structural_scene_clear_solution(scene);
        return false;
    }

    int dof_map[MAX_DOF];
    int dof_count = 0;
    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        bool theta_active = node_has_theta_stiffness(scene, i);
        int base = (int)i * 3;
        dof_map[base] = node->fixed_x ? -1 : dof_count++;
        dof_map[base + 1] = node->fixed_y ? -1 : dof_count++;
        dof_map[base + 2] = (node->fixed_theta || !theta_active) ? -1 : dof_count++;
    }

    if (dof_count <= 0) {
        fill_warning(result, "All DOFs are fixed.");
        structural_scene_clear_solution(scene);
        return false;
    }

    float b[MAX_DOF] = {0};
    for (size_t i = 0; i < scene->load_count; ++i) {
        const StructLoad *load = &scene->loads[i];
        if (load->case_id != scene->active_load_case) continue;
        int node_index = -1;
        for (size_t n = 0; n < scene->node_count; ++n) {
            if (scene->nodes[n].id == load->node_id) {
                node_index = (int)n;
                break;
            }
        }
        if (node_index < 0) continue;
        int dof_x = dof_map[node_index * 3];
        int dof_y = dof_map[node_index * 3 + 1];
        int dof_t = dof_map[node_index * 3 + 2];
        if (dof_x >= 0) b[dof_x] += load->fx;
        if (dof_y >= 0) b[dof_y] += load->fy;
        if (dof_t >= 0) b[dof_t] += load->mz;
    }
    add_frame_gravity_loads(scene, dof_map, b);

    SparseTriplet trips[MAX_ENTRIES];
    int trip_count = 0;

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
        const StructNode *bnode = &scene->nodes[idx_b];
        float dx = bnode->x - a->x;
        float dy = bnode->y - a->y;
        float L = sqrtf(dx * dx + dy * dy);
        if (L < 1e-4f) continue;
        float c = dx / L;
        float s = dy / L;

        float E = 1.0f;
        float A = 1.0f;
        float I = 1.0f;
        if (edge->material_index >= 0 && edge->material_index < (int)scene->material_count) {
            const StructMaterial *mat = &scene->materials[edge->material_index];
            E = mat->youngs_modulus;
            A = mat->area;
            I = mat->moment_inertia;
        }

        float L2 = L * L;
        float L3 = L2 * L;
        float k_axial = (E * A) / L;
        float k_bend = (E * I) / L3;

        float k_local[6][6] = {
            { k_axial, 0.0f, 0.0f, -k_axial, 0.0f, 0.0f },
            { 0.0f, 12.0f * k_bend, 6.0f * k_bend * L, 0.0f, -12.0f * k_bend, 6.0f * k_bend * L },
            { 0.0f, 6.0f * k_bend * L, 4.0f * k_bend * L2, 0.0f, -6.0f * k_bend * L, 2.0f * k_bend * L2 },
            { -k_axial, 0.0f, 0.0f, k_axial, 0.0f, 0.0f },
            { 0.0f, -12.0f * k_bend, -6.0f * k_bend * L, 0.0f, 12.0f * k_bend, -6.0f * k_bend * L },
            { 0.0f, 6.0f * k_bend * L, 2.0f * k_bend * L2, 0.0f, -6.0f * k_bend * L, 4.0f * k_bend * L2 }
        };
        apply_frame_releases(k_local, edge->release_a, edge->release_b);

        float T[6][6] = {
            { c, s, 0.0f, 0.0f, 0.0f, 0.0f },
            { -s, c, 0.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, c, s, 0.0f },
            { 0.0f, 0.0f, 0.0f, -s, c, 0.0f },
            { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }
        };

        float temp[6][6] = {0};
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < 6; ++k) {
                    sum += k_local[i][k] * T[k][j];
                }
                temp[i][j] = sum;
            }
        }

        float k_global[6][6] = {0};
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < 6; ++k) {
                    sum += T[k][i] * temp[k][j];
                }
                k_global[i][j] = sum;
            }
        }

        int dofs[6] = {
            dof_map[idx_a * 3],
            dof_map[idx_a * 3 + 1],
            dof_map[idx_a * 3 + 2],
            dof_map[idx_b * 3],
            dof_map[idx_b * 3 + 1],
            dof_map[idx_b * 3 + 2]
        };

        for (int r = 0; r < 6; ++r) {
            if (dofs[r] < 0) continue;
            for (int cidx = 0; cidx < 6; ++cidx) {
                if (dofs[cidx] < 0) continue;
                if (trip_count >= MAX_ENTRIES) break;
                trips[trip_count++] = (SparseTriplet){
                    .row = dofs[r],
                    .col = dofs[cidx],
                    .value = k_global[r][cidx]
                };
            }
        }
    }

    SparseMatrix K;
    if (!sparse_matrix_from_triplets(&K, trips, trip_count, dof_count)) {
        fill_warning(result, "Stiffness assembly failed.");
        structural_scene_clear_solution(scene);
        return false;
    }

    float x[MAX_DOF] = {0};
    int iterations = 0;
    float residual = 0.0f;
    bool ok = solve_cg(&K, b, x, dof_count, 1024, 1e-4f, &iterations, &residual);
    result->success = ok;
    result->singular = !ok;
    result->iterations = iterations;
    result->residual = residual;

    if (!ok) {
        snprintf(result->warning, sizeof(result->warning),
                 "Solve failed (singular or ill-conditioned).");
        structural_scene_clear_solution(scene);
        return false;
    }

    for (size_t i = 0; i < scene->node_count; ++i) {
        int dof_x = dof_map[(int)i * 3];
        int dof_y = dof_map[(int)i * 3 + 1];
        int dof_t = dof_map[(int)i * 3 + 2];
        scene->disp_x[i] = (dof_x >= 0) ? x[dof_x] : 0.0f;
        scene->disp_y[i] = (dof_y >= 0) ? x[dof_y] : 0.0f;
        scene->disp_theta[i] = (dof_t >= 0) ? x[dof_t] : 0.0f;
    }
    scene->has_solution = true;

    for (size_t e = 0; e < scene->edge_count; ++e) {
        StructEdge *edge = &scene->edges[e];
        int idx_a = -1;
        int idx_b = -1;
        for (size_t n = 0; n < scene->node_count; ++n) {
            if (scene->nodes[n].id == edge->a_id) idx_a = (int)n;
            if (scene->nodes[n].id == edge->b_id) idx_b = (int)n;
        }
        if (idx_a < 0 || idx_b < 0) continue;
        const StructNode *a = &scene->nodes[idx_a];
        const StructNode *bnode = &scene->nodes[idx_b];
        float dx = bnode->x - a->x;
        float dy = bnode->y - a->y;
        float L = sqrtf(dx * dx + dy * dy);
        if (L < 1e-4f) {
            edge->axial_force = 0.0f;
            edge->axial_stress = 0.0f;
            edge->shear_force_a = 0.0f;
            edge->shear_force_b = 0.0f;
            edge->bending_moment_a = 0.0f;
            edge->bending_moment_b = 0.0f;
            continue;
        }
        float c = dx / L;
        float s = dy / L;

        float E = 1.0f;
        float A = 1.0f;
        float I = 1.0f;
        if (edge->material_index >= 0 && edge->material_index < (int)scene->material_count) {
            const StructMaterial *mat = &scene->materials[edge->material_index];
            E = mat->youngs_modulus;
            A = mat->area;
            I = mat->moment_inertia;
        }

        float L2 = L * L;
        float L3 = L2 * L;
        float k_axial = (E * A) / L;
        float k_bend = (E * I) / L3;

        float k_local[6][6] = {
            { k_axial, 0.0f, 0.0f, -k_axial, 0.0f, 0.0f },
            { 0.0f, 12.0f * k_bend, 6.0f * k_bend * L, 0.0f, -12.0f * k_bend, 6.0f * k_bend * L },
            { 0.0f, 6.0f * k_bend * L, 4.0f * k_bend * L2, 0.0f, -6.0f * k_bend * L, 2.0f * k_bend * L2 },
            { -k_axial, 0.0f, 0.0f, k_axial, 0.0f, 0.0f },
            { 0.0f, -12.0f * k_bend, -6.0f * k_bend * L, 0.0f, 12.0f * k_bend, -6.0f * k_bend * L },
            { 0.0f, 6.0f * k_bend * L, 2.0f * k_bend * L2, 0.0f, -6.0f * k_bend * L, 4.0f * k_bend * L2 }
        };
        apply_frame_releases(k_local, edge->release_a, edge->release_b);

        float u1 = c * scene->disp_x[idx_a] + s * scene->disp_y[idx_a];
        float v1 = -s * scene->disp_x[idx_a] + c * scene->disp_y[idx_a];
        float u2 = c * scene->disp_x[idx_b] + s * scene->disp_y[idx_b];
        float v2 = -s * scene->disp_x[idx_b] + c * scene->disp_y[idx_b];
        float d_local[6] = {
            u1,
            v1,
            scene->disp_theta[idx_a],
            u2,
            v2,
            scene->disp_theta[idx_b]
        };

        float f_local[6] = {0};
        for (int r = 0; r < 6; ++r) {
            float sum = 0.0f;
            for (int cidx = 0; cidx < 6; ++cidx) {
                sum += k_local[r][cidx] * d_local[cidx];
            }
            f_local[r] = sum;
        }

        edge->axial_force = f_local[0];
        edge->axial_stress = (A > 0.0f) ? edge->axial_force / A : 0.0f;
        edge->shear_force_a = f_local[1];
        edge->bending_moment_a = f_local[2];
        edge->shear_force_b = f_local[4];
        edge->bending_moment_b = f_local[5];
    }

    return true;
}

void structural_compute_frame_internal_forces_ex(StructuralScene *scene,
                                                 const float *u,
                                                 float *out_forces,
                                                 size_t force_count,
                                                 bool update_edges) {
    if (!scene || !u) return;
    size_t dof_count = scene->node_count * 3;
    if (out_forces && force_count >= dof_count) {
        memset(out_forces, 0, sizeof(float) * dof_count);
    }

    for (size_t e = 0; e < scene->edge_count; ++e) {
        StructEdge *edge = &scene->edges[e];
        int idx_a = -1;
        int idx_b = -1;
        for (size_t n = 0; n < scene->node_count; ++n) {
            if (scene->nodes[n].id == edge->a_id) idx_a = (int)n;
            if (scene->nodes[n].id == edge->b_id) idx_b = (int)n;
        }
        if (idx_a < 0 || idx_b < 0) continue;
        const StructNode *a = &scene->nodes[idx_a];
        const StructNode *bnode = &scene->nodes[idx_b];
        float dx = bnode->x - a->x;
        float dy = bnode->y - a->y;
        float L = sqrtf(dx * dx + dy * dy);
        if (L < 1e-4f) {
            if (update_edges) {
                edge->axial_force = 0.0f;
                edge->axial_stress = 0.0f;
                edge->shear_force_a = 0.0f;
                edge->shear_force_b = 0.0f;
                edge->bending_moment_a = 0.0f;
                edge->bending_moment_b = 0.0f;
            }
            continue;
        }
        float c = dx / L;
        float s = dy / L;

        float E = 1.0f;
        float A = 1.0f;
        float I = 1.0f;
        if (edge->material_index >= 0 && edge->material_index < (int)scene->material_count) {
            const StructMaterial *mat = &scene->materials[edge->material_index];
            E = mat->youngs_modulus;
            A = mat->area;
            I = mat->moment_inertia;
        }

        float L2 = L * L;
        float L3 = L2 * L;
        float k_axial = (E * A) / L;
        float k_bend = (E * I) / L3;

        float k_local[6][6] = {
            { k_axial, 0.0f, 0.0f, -k_axial, 0.0f, 0.0f },
            { 0.0f, 12.0f * k_bend, 6.0f * k_bend * L, 0.0f, -12.0f * k_bend, 6.0f * k_bend * L },
            { 0.0f, 6.0f * k_bend * L, 4.0f * k_bend * L2, 0.0f, -6.0f * k_bend * L, 2.0f * k_bend * L2 },
            { -k_axial, 0.0f, 0.0f, k_axial, 0.0f, 0.0f },
            { 0.0f, -12.0f * k_bend, -6.0f * k_bend * L, 0.0f, 12.0f * k_bend, -6.0f * k_bend * L },
            { 0.0f, 6.0f * k_bend * L, 2.0f * k_bend * L2, 0.0f, -6.0f * k_bend * L, 4.0f * k_bend * L2 }
        };
        apply_frame_releases(k_local, edge->release_a, edge->release_b);

        float uax = u[idx_a * 3 + 0];
        float uay = u[idx_a * 3 + 1];
        float tax = u[idx_a * 3 + 2];
        float ubx = u[idx_b * 3 + 0];
        float uby = u[idx_b * 3 + 1];
        float tbx = u[idx_b * 3 + 2];

        float u1 = c * uax + s * uay;
        float v1 = -s * uax + c * uay;
        float u2 = c * ubx + s * uby;
        float v2 = -s * ubx + c * uby;
        float d_local[6] = {
            u1,
            v1,
            tax,
            u2,
            v2,
            tbx
        };

        float f_local[6] = {0};
        for (int r = 0; r < 6; ++r) {
            float sum = 0.0f;
            for (int cidx = 0; cidx < 6; ++cidx) {
                sum += k_local[r][cidx] * d_local[cidx];
            }
            f_local[r] = sum;
        }

        if (update_edges) {
            edge->axial_force = f_local[0];
            edge->axial_stress = (A > 0.0f) ? edge->axial_force / A : 0.0f;
            edge->shear_force_a = f_local[1];
            edge->bending_moment_a = f_local[2];
            edge->shear_force_b = f_local[4];
            edge->bending_moment_b = f_local[5];
        }

        if (out_forces && force_count >= dof_count) {
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

            out_forces[idx_a * 3 + 0] += f_global[0];
            out_forces[idx_a * 3 + 1] += f_global[1];
            out_forces[idx_a * 3 + 2] += f_global[2];
            out_forces[idx_b * 3 + 0] += f_global[3];
            out_forces[idx_b * 3 + 1] += f_global[4];
            out_forces[idx_b * 3 + 2] += f_global[5];
        }
    }
}

void structural_compute_frame_internal_forces(StructuralScene *scene,
                                              const float *u,
                                              float *out_forces,
                                              size_t force_count) {
    structural_compute_frame_internal_forces_ex(scene, u, out_forces, force_count, true);
}

void structural_apply_frame_stiffness(const StructuralScene *scene,
                                      const float *u,
                                      float *out_forces,
                                      size_t force_count) {
    if (!scene || !u || !out_forces) return;
    size_t dof_count = scene->node_count * 3;
    if (force_count < dof_count) return;
    memset(out_forces, 0, sizeof(float) * dof_count);

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
        const StructNode *bnode = &scene->nodes[idx_b];
        float dx = bnode->x - a->x;
        float dy = bnode->y - a->y;
        float L = sqrtf(dx * dx + dy * dy);
        if (L < 1e-4f) continue;
        float c = dx / L;
        float s = dy / L;

        float E = 1.0f;
        float A = 1.0f;
        float I = 1.0f;
        if (edge->material_index >= 0 && edge->material_index < (int)scene->material_count) {
            const StructMaterial *mat = &scene->materials[edge->material_index];
            E = mat->youngs_modulus;
            A = mat->area;
            I = mat->moment_inertia;
        }

        float L2 = L * L;
        float L3 = L2 * L;
        float k_axial = (E * A) / L;
        float k_bend = (E * I) / L3;

        float k_local[6][6] = {
            { k_axial, 0.0f, 0.0f, -k_axial, 0.0f, 0.0f },
            { 0.0f, 12.0f * k_bend, 6.0f * k_bend * L, 0.0f, -12.0f * k_bend, 6.0f * k_bend * L },
            { 0.0f, 6.0f * k_bend * L, 4.0f * k_bend * L2, 0.0f, -6.0f * k_bend * L, 2.0f * k_bend * L2 },
            { -k_axial, 0.0f, 0.0f, k_axial, 0.0f, 0.0f },
            { 0.0f, -12.0f * k_bend, -6.0f * k_bend * L, 0.0f, 12.0f * k_bend, -6.0f * k_bend * L },
            { 0.0f, 6.0f * k_bend * L, 2.0f * k_bend * L2, 0.0f, -6.0f * k_bend * L, 4.0f * k_bend * L2 }
        };
        apply_frame_releases(k_local, edge->release_a, edge->release_b);

        float T[6][6] = {
            { c, s, 0.0f, 0.0f, 0.0f, 0.0f },
            { -s, c, 0.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, c, s, 0.0f },
            { 0.0f, 0.0f, 0.0f, -s, c, 0.0f },
            { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }
        };

        float temp[6][6] = {0};
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < 6; ++k) {
                    sum += k_local[i][k] * T[k][j];
                }
                temp[i][j] = sum;
            }
        }

        float k_global[6][6] = {0};
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < 6; ++k) {
                    sum += T[k][i] * temp[k][j];
                }
                k_global[i][j] = sum;
            }
        }

        int dofs[6] = {
            idx_a * 3,
            idx_a * 3 + 1,
            idx_a * 3 + 2,
            idx_b * 3,
            idx_b * 3 + 1,
            idx_b * 3 + 2
        };

        float u_local[6] = {0};
        for (int i = 0; i < 6; ++i) {
            u_local[i] = u[dofs[i]];
        }

        float f_local[6] = {0};
        for (int i = 0; i < 6; ++i) {
            float sum = 0.0f;
            for (int k = 0; k < 6; ++k) {
                sum += k_global[i][k] * u_local[k];
            }
            f_local[i] = sum;
        }

        for (int i = 0; i < 6; ++i) {
            out_forces[dofs[i]] += f_local[i];
        }
    }
}
