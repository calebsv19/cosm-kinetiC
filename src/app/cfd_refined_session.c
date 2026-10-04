#include "app/cfd_refined_session.h"
#include <math.h>
#include <string.h>
#include <time.h>
static struct json_object *get(struct json_object *o, const char *k) {
    struct json_object *v = NULL;
    if (o)
        json_object_object_get_ex(o, k, &v);
    return v;
}
static bool number(struct json_object *o, double lo, double hi, double *out) {
    if (!o ||
        (json_object_get_type(o) != json_type_int && json_object_get_type(o) != json_type_double))
        return false;
    double v = json_object_get_double(o);
    if (!isfinite(v) || v < lo || v > hi)
        return false;
    *out = v;
    return true;
}
static bool vector(struct json_object *o, int count, double lo, double hi, double *out) {
    if (!o || json_object_get_type(o) != json_type_array ||
        json_object_array_length(o) != (size_t)count)
        return false;
    for (int i = 0; i < count; i++)
        if (!number(json_object_array_get_idx(o, i), lo, hi, &out[i]))
            return false;
    return true;
}
void cfd_refined_session_destroy(CfdRefinedSession *s) {
    if (!s)
        return;
    cfd_refined_channel_destroy(&s->channel);
    cfd_refined_mesh_destroy(&s->mesh);
    cfd_memory_free(s->heads);
    cfd_memory_free(s->next);
    cfd_memory_free(s->gradient);
    s->heads = s->next = NULL;
    s->gradient = NULL;
    s->ready = s->observed = false;
}
static const char *failure(const CfdRefinedSession *s, const char *fallback) {
    if (s->memory.last_failure == CFD_MEMORY_LIMIT)
        return "numerical_memory_budget_exceeded";
    if (s->memory.last_failure == CFD_MEMORY_SIZE_OVERFLOW)
        return "numerical_allocation_size_overflow";
    if (s->memory.last_failure == CFD_MEMORY_SYSTEM_FAILURE)
        return "numerical_allocation_failed";
    return fallback;
}
bool cfd_refined_session_init(CfdRefinedSession *s, struct json_object *request) {
    if (!s)
        return false;
    clock_t setup_begin = clock();
    memset(s, 0, sizeof(*s));
    s->error = "invalid_refined_configuration";
    struct json_object *ch = get(request, "channel"), *fluid = get(request, "fluid");
    double grid[3], dims[3], rho, mu, mean, dt, limit = 512, cells = 200000;
    if (!vector(get(request, "grid"), 3, 1, 256, grid) || grid[0] < 4 || grid[1] < 4 ||
        grid[2] != 1 || floor(grid[0]) != grid[0] || floor(grid[1]) != grid[1] ||
        !vector(get(ch, "dimensions_m"), 3, .1, 100, dims) ||
        !number(get(fluid, "density_kg_m3"), 1e-9, 1e9, &rho) ||
        !number(get(fluid, "dynamic_viscosity_pa_s"), 1e-9, 1000, &mu) ||
        !number(get(ch, "inlet_mean_m_s"), .00001, 100, &mean) ||
        !number(get(request, "dt"), .00001, .1, &dt))
        return false;
    if (get(request, "numerical_memory_limit_mib") &&
        !number(get(request, "numerical_memory_limit_mib"), 1, 4096, &limit))
        return false;
    if (get(request, "solver_cell_budget") &&
        !number(get(request, "solver_cell_budget"), 1, 200000, &cells))
        return false;
    if (floor(limit) != limit || floor(cells) != cells || rho * mean * dims[1] / mu > 100)
        return false;
    const char *mode = json_object_get_string(get(ch, "solve_mode"));
    if (!mode)
        mode = "transient_navier_stokes";
    if (strcmp(mode, "steady_stokes") && strcmp(mode, "transient_navier_stokes"))
        return false;
    s->steady_stokes = !strcmp(mode, "steady_stokes");
    if (s->steady_stokes && json_object_get_int(get(request, "steps")) != 1)
        return false;
    s->span = dims[2];
    s->dt = dt;
    s->memory.limit_bytes = (size_t)limit * 1024 * 1024;
    struct json_object *bounds = get(ch, "obstacle_bounds_m");
    if (bounds) {
        if (!vector(bounds, 4, 0, 100, s->bounds) || s->bounds[0] <= 0 || s->bounds[1] <= 0 ||
            s->bounds[2] >= dims[0] || s->bounds[3] >= dims[1] || s->bounds[0] >= s->bounds[2] ||
            s->bounds[1] >= s->bounds[3])
            return false;
        s->has_obstacle = true;
    }
    CfdRefinementRegion regions[64];
    int count = 1;
    regions[0] =
        (CfdRefinementRegion){.25 * dims[0], .25 * dims[1], .75 * dims[0], .75 * dims[1], 1};
    struct json_object *rs = get(ch, "refinement_regions");
    if (rs) {
        if (json_object_get_type(rs) != json_type_array || json_object_array_length(rs) > 64)
            return false;
        count = (int)json_object_array_length(rs);
        for (int i = 0; i < count; i++) {
            struct json_object *r = json_object_array_get_idx(rs, i);
            double b[4], level;
            if (!vector(get(r, "bounds_m"), 4, 0, 100, b) ||
                !number(get(r, "level"), 0, 10, &level) || floor(level) != level)
                return false;
            regions[i] = (CfdRefinementRegion){b[0], b[1], b[2], b[3], (int)level};
        }
    }
    CfdMemoryBudget *previous = cfd_memory_scope(&s->memory);
    bool ok = cfd_refined_mesh_init_regions(&s->mesh, (int)grid[0], (int)grid[1], dims[0], dims[1],
                                            regions, count, (int)cells);
    if (ok && s->has_obstacle)
        ok = cfd_refined_mesh_remove_rectangle(&s->mesh, s->bounds[0], s->bounds[1], s->bounds[2],
                                               s->bounds[3]);
    if (ok)
        ok = cfd_refined_channel_init(&s->channel, &s->mesh, rho, mu, mean);
    if (ok) {
        int nc = s->mesh.cell_count, nb = s->mesh.nx * s->mesh.ny;
        s->heads = cfd_memory_malloc((size_t)nb * sizeof(int));
        s->next = cfd_memory_malloc((size_t)nc * sizeof(int));
        s->gradient = cfd_memory_calloc((size_t)4 * nc, sizeof(double));
        ok = s->heads && s->next && s->gradient;
        if (ok) {
            for (int i = 0; i < nb; i++)
                s->heads[i] = -1;
            s->min_spacing_m = INFINITY;
            for (int i = 0; i < nc; i++) {
                int level = 0;
                for (int span = s->mesh.cells[i].span; span < s->mesh.lattice_scale; span *= 2)
                    level++;
                s->cells_per_level[level]++;
                double h = fmin(s->mesh.dx, s->mesh.dy) * s->mesh.cells[i].span / s->mesh.lattice_scale;
                s->min_spacing_m = fmin(s->min_spacing_m, h);
                s->max_spacing_m = fmax(s->max_spacing_m, h);
                int p = s->mesh.cells[i].parent;
                s->next[i] = s->heads[p];
                s->heads[p] = i;
            }
        }
    }
    cfd_memory_scope(previous);
    if (!ok) {
        s->error = failure(s, "refined_mesh_or_operator_rejected");
        s->failed = true;
        cfd_refined_session_destroy(s);
        return false;
    }
    s->setup_cpu_ms = 1000.0 * (clock() - setup_begin) / CLOCKS_PER_SEC;
    s->ready = true;
    s->error = NULL;
    return true;
}
static void observe(CfdRefinedSession *s) {
    const CfdRefinedMesh *m = &s->mesh;
    CfdRefinedChannel *c = &s->channel;
    int nc = m->cell_count;
    memset(s->gradient, 0, (size_t)4 * nc * sizeof(double));
    for (int f = 0; f < m->face_count; f++) {
        const CfdRefinedFace *p = &m->faces[f];
        int cells[2] = {p->lo, p->hi};
        for (int k = 0; k < 2; k++)
            if (cells[k] >= 0) {
                int id = cells[k];
                double w = (k == 0 ? 1 : -1) * p->area / m->cells[id].volume;
                s->gradient[p->axis * nc + id] += w * c->face_u[f];
                s->gradient[(2 + p->axis) * nc + id] += w * c->face_v[f];
            }
    }
    s->observed =
        cfd_refined_mixed_boundary_force(c->mixed_operator, CFD_REFINED_SOLID, c->rho, s->span,
                                         &s->body_force) &&
        cfd_refined_mixed_energy(c->mixed_operator, c->rho, s->span, NULL, NULL, &s->energy);
}
bool cfd_refined_session_step(CfdRefinedSession *s) {
    if (!s || !s->ready || s->failed || (s->steady_stokes && s->channel.tick))
        return false;
    CfdMemoryBudget *previous = cfd_memory_scope(&s->memory);
    CfdRefinedChannel *c = &s->channel;
    const CfdRefinedMesh *m = &s->mesh;
    bool ok;
    if (s->steady_stokes) {
        int nc = m->cell_count, nf = m->face_count;
        memset(c->rhs, 0, (size_t)nc * sizeof(double));
        memset(c->bc_adv, 0, (size_t)nf * sizeof(double));
        for (int f = 0; f < nf; f++) {
            double y = m->faces[f].cy / (m->ny * m->dy);
            c->bc[f] = m->faces[f].boundary == CFD_REFINED_SOLID ? 0 : 6 * c->mean * y * (1 - y);
        }
        CfdRefinedMixedReport report;
        clock_t mixed_begin = clock();
        ok = cfd_refined_mixed_solve(c->mixed_operator, 0, c->rhs, c->rhs, c->bc, c->bc_adv, c->u,
                                     c->v, c->face_u, c->face_v, c->p, &report);
        c->mixed_solve_cpu_ms = 1000.0 * (clock() - mixed_begin) / CLOCKS_PER_SEC;
        if (ok) {
            c->mixed_iterations = report.iterations;
            c->mixed_storage_bytes = report.storage_bytes;
            s->linear_residual = report.relative_residual;
            c->divergence = report.divergence;
            for (int i = 0; i < nc; i++)
                c->p[i] *= c->rho;
            for (int f = 0; f < nf; f++) {
                c->normal[f] = m->faces[f].axis == 0 ? c->face_u[f] : c->face_v[f];
                c->face_p[f] = NAN;
            }
            c->tick = 1; /* One stationary solve; physical time remains zero. */
        }
    } else {
        ok = cfd_refined_channel_step(c, s->dt);
        s->linear_residual = c->mixed_relative_residual;
    }
    clock_t observation_begin = clock();
    if (ok)
        observe(s);
    s->observation_cpu_ms = 1000.0 * (clock() - observation_begin) / CLOCKS_PER_SEC;
    cfd_memory_scope(previous);
    if (!ok) {
        s->failed = true;
        s->observed = false;
        s->error = failure(s, "refined_solver_failure_or_cfl_bound");
    }
    return ok;
}
