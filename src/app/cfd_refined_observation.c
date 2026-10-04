#include "app/cfd_channel_observation.h"
#include "app/cfd_refined_session.h"
#include <math.h>
#include <string.h>
static struct json_object *get(struct json_object *o, const char *k) {
    struct json_object *v = NULL;
    if (o)
        json_object_object_get_ex(o, k, &v);
    return v;
}
static void num(struct json_object *o, const char *k, double v) {
    json_object_object_add(o, k, isfinite(v) ? json_object_new_double(v) : NULL);
}
static void text(struct json_object *o, const char *k, const char *v) {
    json_object_object_add(o, k, json_object_new_string(v));
}
static void flag(struct json_object *o, const char *k, bool v) {
    json_object_object_add(o, k, json_object_new_boolean(v));
}
static struct json_object *array(const double *v, int n) {
    struct json_object *a = json_object_new_array();
    for (int i = 0; i < n; i++)
        json_object_array_add(a, isfinite(v[i]) ? json_object_new_double(v[i]) : NULL);
    return a;
}
static struct json_object *grid(const CfdRefinedSession *s) {
    double v[] = {s->mesh.nx, s->mesh.ny, 1};
    return array(v, 3);
}
void cfd_refined_session_snapshot(const CfdRefinedSession *s, struct json_object *out, bool full) {
    const CfdRefinedMesh *m = &s->mesh;
    const CfdRefinedChannel *c = &s->channel;
    text(out, "model", "incompressible_refined2d_v1");
    text(out, "model_limitations",
         "Fixed 2D balanced rectangular leaves, stationary aligned rectangle, laminar flow. No "
         "moving bodies, turbulence or 3D. Steady Stokes reference checks do not certify arbitrary "
         "transients.");
    text(out, "solve_mode", s->steady_stokes ? "steady_stokes" : "transient_navier_stokes");
    text(out, "tick_semantics",
         s->steady_stokes ? "one stationary linear solve; physical time remains zero"
                          : "one constant-dt BDF2 step with implicit viscosity and explicit "
                            "transport; BE startup");
    json_object_object_add(out, "effective_grid", grid(s));
    text(out, "grid_kind", "base grid with nonuniform balanced leaves");
    num(out, "voxel_size_m", s->ready ? s->min_spacing_m : NAN);
    num(out, "estimated_dense_bytes",
        (double)m->nx * m->ny * m->lattice_scale * m->lattice_scale * 3 * sizeof(double));
    text(out, "dense_estimate_scope",
         "hypothetical finest uniform cell u/v/p arrays only, not solver workspace or actual "
         "allocation");
    struct json_object *geometry = json_object_new_array();
    if (s->has_obstacle) {
        struct json_object *body = json_object_new_object();
        text(body, "kind", "stationary_rectangle");
        json_object_object_add(body, "bounds_m", array(s->bounds, 4));
        json_object_array_add(geometry, body);
    }
    json_object_object_add(out, "geometry", geometry);
    struct json_object *physics = json_object_new_object(), *health = json_object_new_object(),
                       *memory = json_object_new_object(), *energy = json_object_new_object(),
                       *force = json_object_new_object(), *qualification = json_object_new_object();
    double dims[] = {m->nx * m->dx, m->ny * m->dy, s->span};
    json_object_object_add(physics, "dimensions_m", array(dims, 3));
    num(physics, "density_kg_m3", c->rho);
    num(physics, "dynamic_viscosity_pa_s", c->rho * c->nu);
    num(physics, "inlet_mean_m_s", c->mean);
    text(physics, "outlet", "zero external vector-Laplacian traction: nu du/dn - p n = 0");
    num(health, "fluid_leaf_cells", m->cell_count);
    num(health, "shared_faces", m->face_count);
    num(health, "lattice_scale", m->lattice_scale);
    struct json_object *levels = json_object_new_array(), *cost = json_object_new_object();
    for (int level = 0; level <= 10; level++)
        json_object_array_add(levels, json_object_new_int(s->cells_per_level[level]));
    json_object_object_add(health, "fluid_cells_per_level", levels);
    num(health, "minimum_cell_spacing_m", s->ready ? s->min_spacing_m : NAN);
    num(health, "maximum_cell_min_spacing_m", s->ready ? s->max_spacing_m : NAN);
    num(cost, "setup_cpu_ms", s->ready ? s->setup_cpu_ms : NAN);
    num(cost, "transport_cpu_ms", s->observed && !s->steady_stokes ? c->transport_cpu_ms : NAN);
    num(cost, "mixed_solve_cpu_ms", s->observed ? c->mixed_solve_cpu_ms : NAN);
    num(cost, "observation_cpu_ms", s->observed ? s->observation_cpu_ms : NAN);
    text(cost, "scope", "process CPU time; setup once, other phases last accepted step; "
         "mixed solve includes hierarchy preparation; excludes sampling and JSON publication");
    json_object_object_add(health, "phase_cost", cost);
    num(health, "max_abs_divergence_s_inv", s->observed ? c->divergence : NAN);
    num(health, "linear_relative_residual", s->observed ? s->linear_residual : NAN);
    num(health, "linear_iterations", c->mixed_iterations);
    num(health, "transport_cfl", s->steady_stokes ? NAN : c->transport_cfl);
    num(health, "transport_cfl_limit", s->steady_stokes ? NAN : .25);
    num(health, "observed_transport_dt_limit_s",
        !s->steady_stokes && s->observed && c->transport_cfl > 0
            ? .25 * s->dt / c->transport_cfl : NAN);
    text(health, "timestep_limit_scope",
         "CFL bound from the last accepted step input fluxes only; not future stability or "
         "accuracy assurance. Constant dt is required within a run; compare new runs to "
         "establish temporal accuracy. Viscosity is implicit.");
    text(health, "projection_status",
         s->failed     ? "failed"
         : s->observed ? "converged"
                       : "not_solved");
    num(memory, "limit_bytes", s->memory.limit_bytes);
    num(memory, "live_bytes", s->memory.live_bytes);
    num(memory, "peak_bytes", s->memory.peak_bytes);
    num(memory, "rejected_allocations", s->memory.rejected_allocations);
    text(memory, "scope",
         "numerical requested allocations plus headers and overlapping setup/resizes; excludes "
         "JSON and total process RSS");
    json_object_object_add(health, "numerical_memory", memory);
    flag(force, "available", s->observed && s->has_obstacle);
    if (s->observed && s->has_obstacle) {
        json_object_object_add(force, "pressure_force_n", array(s->body_force.pressure, 2));
        json_object_object_add(force, "viscous_force_n", array(s->body_force.viscous, 2));
        json_object_object_add(force, "total_force_n", array(s->body_force.total, 2));
    }
    text(force, "method",
         "closed solid-boundary weak reaction; pressure and viscous components separate");
    flag(energy, "available", s->observed);
    flag(energy, "transient_energy_rate_available", false);
    double imbalance = NAN;
    if (s->observed) {
        num(energy, "kinetic_energy_j", s->energy.kinetic_j);
        num(energy, "physical_strain_dissipation_w", s->energy.strain_dissipation_w);
        num(energy, "physical_boundary_power_w", s->energy.stress_boundary_power_w);
        num(energy, "discrete_diffusion_w", s->energy.discrete_diffusion_w);
        num(energy, "stabilization_w", s->energy.stabilization_w);
        num(energy, "outward_kinetic_flux_w", s->energy.outward_kinetic_flux_w);
        if (s->steady_stokes)
            imbalance = fabs(s->energy.stress_boundary_power_w - s->energy.strain_dissipation_w) /
                        fmax(fabs(s->energy.stress_boundary_power_w), 1e-30);
    }
    num(energy, "steady_stokes_relative_residual", imbalance);
    flag(qualification, "physical_accuracy_certified", false);
    text(qualification, "evidence", "docs/cfd_pre3d_resolution_goal.md");
    text(qualification, "scope",
         "Verified channel/transient operators and fixed confined steady Stokes rectangle; per-run "
         "reference gate is separate from general CFD certification");
    bool reference = s->steady_stokes && s->has_obstacle && fabs(dims[0] - 4) < 1e-12 &&
                     fabs(dims[1] - 2) < 1e-12 && fabs(dims[2] - .5) < 1e-12 &&
                     fabs(c->rho - 1) < 1e-12 && fabs(c->rho * c->nu - .1) < 1e-12 &&
                     fabs(c->mean - .002) < 1e-12;
    const double bounds[] = {1.5, .75, 2.5, 1.25};
    for (int i = 0; i < 4; i++)
        reference = reference && fabs(s->bounds[i] - bounds[i]) < 1e-12;
    struct json_object *ref = json_object_new_object();
    flag(ref, "applicable", reference);
    if (reference && s->observed) {
        double pe = fabs(s->body_force.pressure[0] / .003633153361585 - 1),
               ve = fabs(s->body_force.viscous[0] / .002347529940743 - 1),
               te = fabs(s->body_force.total[0] / (.003633153361585 + .002347529940743) - 1);
        num(ref, "pressure_relative_error", pe);
        num(ref, "viscous_relative_error", ve);
        num(ref, "total_relative_error", te);
        num(ref, "limit", .02);
        flag(ref, "passed",
             pe <= .02 && ve <= .02 && te <= .02 && imbalance <= .02 &&
                 s->linear_residual <= 1e-11 && c->divergence < 1e-8);
    } else
        flag(ref, "passed", false);
    json_object_object_add(qualification, "fixed_case_reference_gate", ref);
    json_object_object_add(out, "physics", physics);
    json_object_object_add(out, "health", health);
    json_object_object_add(out, "boundary_force_budget", force);
    json_object_object_add(out, "energy_budget", energy);
    json_object_object_add(out, "qualification", qualification);
    if (full && s->ready) {
        struct json_object *fields = json_object_new_object(), *leaves = json_object_new_array(),
                           *faces = json_object_new_array();
        text(fields, "schema", "physics_sim_refined2d_fields_v1");
        text(fields, "leaf_columns", "cx_m,cy_m,width_m,height_m,u_m_s,v_m_s,pressure_pa");
        text(fields, "face_columns",
             "lo_cell,hi_cell,axis,cx_m,cy_m,area_m,boundary_kind,u_m_s,v_m_s; -1 means exterior; "
             "boundary kind 0 interior, 1 domain, 2 solid");
        for (int i = 0; i < m->cell_count; i++) {
            const CfdRefinedCell *p = &m->cells[i];
            double row[] = {p->cx,
                            p->cy,
                            p->span * m->dx / m->lattice_scale,
                            p->span * m->dy / m->lattice_scale,
                            c->u[i],
                            c->v[i],
                            s->observed ? c->p[i] : NAN};
            json_object_array_add(leaves, array(row, 7));
        }
        for (int i = 0; i < m->face_count; i++) {
            const CfdRefinedFace *p = &m->faces[i];
            double row[] = {p->lo,
                            p->hi,
                            p->axis,
                            p->cx,
                            p->cy,
                            p->area,
                            p->boundary,
                            s->observed ? c->face_u[i] : NAN,
                            s->observed ? c->face_v[i] : NAN};
            json_object_array_add(faces, array(row, 9));
        }
        json_object_object_add(fields, "leaves", leaves);
        json_object_object_add(fields, "faces", faces);
        json_object_object_add(out, "refined_fields", fields);
    }
}
static void values(const CfdRefinedSession *s, double x, double y, double out[11]) {
    memset(out, 0, 11 * sizeof(double));
    out[6] = out[9] = out[7] = out[8] = out[10] = NAN;
    const CfdRefinedMesh *m = &s->mesh;
    const CfdRefinedChannel *c = &s->channel;
    if (!s->ready)
        return;
    if (s->has_obstacle && x >= s->bounds[0] && x <= s->bounds[2] && y >= s->bounds[1] &&
        y <= s->bounds[3]) {
        out[2] = 1;
        return;
    }
    int i = (int)floor(x / m->dx), j = (int)floor(y / m->dy);
    i = i < 0 ? 0 : i >= m->nx ? m->nx - 1 : i;
    j = j < 0 ? 0 : j >= m->ny ? m->ny - 1 : j;
    for (int k = s->heads[j * m->nx + i]; k >= 0; k = s->next[k]) {
        const CfdRefinedCell *p = &m->cells[k];
        double hx = .5 * p->span * m->dx / m->lattice_scale,
               hy = .5 * p->span * m->dy / m->lattice_scale;
        if (fabs(x - p->cx) > hx + 1e-12 || fabs(y - p->cy) > hy + 1e-12)
            continue;
        out[3] = c->u[k];
        out[4] = c->v[k];
        if (y <= 0 || y >= m->ny * m->dy)
            out[3] = out[4] = 0;
        else if (x <= 0) {
            double eta = y / (m->ny * m->dy);
            out[3] = 6 * c->mean * eta * (1 - eta);
            out[4] = 0;
        }
        out[0] = hypot(out[3], out[4]);
        if (s->observed) {
            int n = m->cell_count;
            out[9] = c->p[k];
            out[7] = s->gradient[k] + s->gradient[3 * n + k];
            out[8] = fabs(s->gradient[2 * n + k] - s->gradient[n + k]);
            out[10] = c->rho * c->nu * (s->gradient[2 * n + k] + s->gradient[n + k]);
        }
        return;
    }
}
struct json_object *cfd_refined_session_sample(const CfdRefinedSession *s,
                                               struct json_object *req) {
    if (!s || !s->ready) {
        struct json_object *o = json_object_new_object();
        text(o, "error", "refined_session_not_ready");
        return o;
    }
    CfdChannel layout = {0};
    layout.length = s->mesh.nx * s->mesh.dx;
    layout.height = s->mesh.ny * s->mesh.dy;
    layout.width = s->span;
    layout.n = s->mesh.ny;
    layout.rho = s->channel.rho;
    layout.mu = s->channel.rho * s->channel.nu;
    struct json_object *o = cfd_channel_sample(&layout, req), *samples = get(o, "samples");
    int ua = json_object_get_int(get(o, "u_axis")), va = json_object_get_int(get(o, "v_axis")),
        na = json_object_get_int(get(o, "normal_axis"));
    int width = json_object_get_int(get(o, "width")),
        height = json_object_get_int(get(o, "height")), count = req ? 11 : 3;
    double dims[] = {layout.length, layout.height, layout.width}, low[11], high[11], sum[11] = {0};
    int finite[11] = {0};
    for (int k = 0; k < count; k++) {
        low[k] = INFINITY;
        high[k] = -INFINITY;
    }
    for (int j = 0; j < height; j++)
        for (int i = 0; i < width; i++) {
            double p[3] = {0}, v[11];
            p[na] = json_object_get_double(get(o, "slice_world_m"));
            p[ua] = (i + .5) * dims[ua] / width;
            p[va] = (j + .5) * dims[va] / height;
            values(s, p[0], p[1], v);
            json_object_array_put_idx(samples, j * width + i, array(v, count));
            for (int k = 0; k < count; k++)
                if (isfinite(v[k])) {
                    low[k] = fmin(low[k], v[k]);
                    high[k] = fmax(high[k], v[k]);
                    sum[k] += v[k];
                    finite[k]++;
                }
        }
    struct json_object *stats = get(o, "statistics"), *fields = get(o, "fields"),
                       *probes = get(o, "probes");
    for (int k = 0; k < count; k++) {
        const char *key = json_object_get_string(json_object_array_get_idx(fields, k));
        if (k == 6)
            continue;
        struct json_object *stat = get(stats, key);
        num(stat, "min", low[k]);
        num(stat, "max", high[k]);
        num(stat, "mean", finite[k] ? sum[k] / finite[k] : NAN);
        num(stat, "finite_samples", finite[k]);
    }
    for (size_t i = 0; i < json_object_array_length(probes); i++) {
        struct json_object *probe = json_object_array_get_idx(probes, i);
        if (!json_object_get_boolean(get(probe, "inside")))
            continue;
        struct json_object *p = get(probe, "requested_world_m");
        double v[11];
        values(s, json_object_get_double(json_object_array_get_idx(p, 0)),
               json_object_get_double(json_object_array_get_idx(p, 1)), v);
        json_object_object_add(probe, "values", array(v, 11));
    }
    num(o, "speed_max", high[0]);
    json_object_object_add(o, "grid", grid(s));
    text(o, "sampling",
         "containing leaf center; Gauss derivatives diagnostic only; solid and unsolved pressure "
         "unavailable; exact inlet and Y-wall velocity; no face-pressure trace");
    return o;
}
