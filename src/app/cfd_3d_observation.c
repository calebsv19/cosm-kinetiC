#define _DARWIN_C_SOURCE 1
#include "app/cfd_3d_session.h"
#include "app/cfd_channel_observation.h"
#include <math.h>
#include <string.h>
#include <sys/resource.h>
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
void cfd_3d_session_snapshot(const Cfd3dSession *s, struct json_object *out, bool full) {
    if(s->obstacle){cfd_obstacle3d_session_snapshot(s,out,full);return;}
    if (s->transient_kind) {
        cfd_transient3d_session_snapshot(s, out, full);
        return;
    }
    if (s->open) {
        cfd_open3d_session_snapshot(s, out, full);
        return;
    }
    const CfdCartesian3d *g = &s->grid;
    const CfdDuct3d *d = &s->duct;
    const CfdPeriodic3d *t = &s->transient;
    text(out, "model", "incompressible_cartesian3d_v1");
    text(out, "solve_mode", s->steady ? "steady_duct" : "manufactured_transient");
    text(out, "model_limitations",
         "Uniform Cartesian laminar verification: fixed-flow periodic-X duct or fully periodic "
         "manufactured 3D transient. Open outlets, obstacles, moving bodies and turbulence are not "
         "qualified.");
    text(out, "tick_semantics",
         s->steady
             ? "one stationary solve; physical time stays zero"
             : "constant-dt BDF2 with BE startup; exact commuting periodic pressure coupling");
    double grid[3] = {g->n[0], g->n[1], g->n[2]};
    json_object_object_add(out, "effective_grid", array(grid, 3));
    num(out, "voxel_size_m", s->ready ? fmin(g->h[0], fmin(g->h[1], g->h[2])) : NAN);
    num(out, "estimated_dense_bytes", (double)g->count * 4 * sizeof(double));
    text(out, "dense_estimate_scope",
         "stored face velocity and cell pressure only; excludes cached matrices, hierarchies, "
         "Krylov and JSON");
    text(out, "grid_kind", "uniform staggered Cartesian faces");
    json_object_object_add(out, "geometry", json_object_new_array());
    struct json_object *physics = json_object_new_object(), *health = json_object_new_object(),
                       *memory = json_object_new_object(), *cost = json_object_new_object(),
                       *energy = json_object_new_object(), *force = json_object_new_object(),
                       *qualification = json_object_new_object(), *ref = json_object_new_object();
    json_object_object_add(physics, "dimensions_m", array(g->length, 3));
    num(physics, "density_kg_m3", s->rho);
    num(physics, "dynamic_viscosity_pa_s", s->mu);
    text(physics, "boundary_model",
         s->steady ? "periodic X with solved pressure jump; no-slip Y/Z; prescribed volume flow"
                   : "fully periodic XYZ; continuous manufactured forcing");
    num(physics, "pressure_drop_pa",
        s->steady && s->observed ? d->gradient_pa_m * g->length[0] : NAN);
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#ifdef __APPLE__
        num(health, "process_peak_rss_bytes", (double)usage.ru_maxrss);
#else
        num(health, "process_peak_rss_bytes", (double)usage.ru_maxrss * 1024);
#endif
    }
    num(health, "fluid_cells", g->count);
    num(health, "shared_faces", 3 * g->count + (s->steady ? g->n[0] * (g->n[1] + g->n[2]) : 0));
    num(health, "stored_face_values", 3 * g->count);
    num(health, "linear_relative_residual",
        s->observed ? (s->steady ? d->relative_residual : t->true_residual) : NAN);
    num(health, "max_abs_divergence_s_inv",
        s->observed ? (s->steady ? d->max_divergence : t->max_divergence) : NAN);
    num(health, "linear_iterations", s->observed ? (s->steady ? d->iterations : t->iterations) : 0);
    num(health, "volume_flux_m3_s", s->steady && s->observed ? d->flow_m3_s : NAN);
    num(health, "transport_cfl", !s->steady && s->observed ? t->cfl : NAN);
    num(health, "transport_cfl_limit", s->steady ? NAN : .25);
    text(health, "projection_status",
         s->failed     ? "failed"
         : s->observed ? "converged"
                       : "not_solved");
    num(memory, "limit_bytes", s->memory.limit_bytes);
    num(memory, "live_bytes", s->memory.live_bytes);
    num(memory, "peak_bytes", s->memory.peak_bytes);
    num(memory, "rejected_allocations", s->memory.rejected_allocations);
    text(memory, "scope",
         "owned numerical allocations plus headers and overlapping setup; excludes JSON and total "
         "RSS");
    json_object_object_add(health, "numerical_memory", memory);
    num(cost, "setup_cpu_ms", s->ready ? (s->steady ? d->setup_cpu_ms : t->setup_cpu_ms) : NAN);
    num(cost, "transport_cpu_ms", s->observed && !s->steady ? t->transport_cpu_ms : NAN);
    num(cost, "mixed_solve_cpu_ms",
        s->observed ? (s->steady ? d->solve_cpu_ms : t->solve_cpu_ms) : NAN);
    text(cost, "scope",
         "process CPU; last accepted solve includes physical observation; JSON/sampling "
         "publication measured separately");
    json_object_object_add(health, "phase_cost", cost);
    flag(energy, "available", s->observed);
    flag(energy, "transient_energy_rate_available", s->observed && !s->steady);
    num(energy, "physical_strain_dissipation_w",
        s->observed ? (s->steady ? d->dissipation_w : t->physical_dissipation_w) : NAN);
    num(energy, "discrete_diffusion_w", s->observed && s->steady ? d->discrete_dissipation_w : NAN);
    num(energy, "physical_boundary_power_w",
        s->observed && s->steady ? d->gradient_pa_m * g->length[0] * d->flow_m3_s : NAN);
    num(energy, "kinetic_energy_j", s->observed && !s->steady ? t->kinetic_j : NAN);
    num(energy, "body_force_power_w", s->observed && !s->steady ? t->forcing_power_w : NAN);
    num(energy, "kinetic_energy_rate_w", s->observed && !s->steady ? t->energy_rate_w : NAN);
    num(energy, "residual_w", s->observed && !s->steady ? t->energy_residual_w : NAN);
    flag(force, "available", s->observed && s->steady);
    if (s->observed && s->steady)
        json_object_object_add(force, "wall_drag_n", array(d->wall_force_n, 4));
    text(force, "wall_order", "y_min,y_max,z_min,z_max; force on walls in +X");
    flag(ref, "applicable", s->steady);
    bool passed = false;
    if (s->observed && s->steady) {
        double wall = 0;
        for (int a = 0; a < 4; a++)
            wall = fmax(wall, d->wall_relative_error[a]);
        num(ref, "velocity_relative_l2", d->velocity_relative_l2);
        num(ref, "pressure_relative_error", d->pressure_relative_error);
        num(ref, "volume_flow_relative_error",
            fabs(d->flow_m3_s - d->requested_flow) / d->requested_flow);
        num(ref, "max_wall_relative_error", wall);
        num(ref, "energy_relative_error", d->energy_relative_error);
        passed = d->velocity_relative_l2 <= .01 && d->pressure_relative_error <= .01 &&
                 wall <= .02 && d->energy_relative_error <= .02 && d->relative_residual <= 1e-11 &&
                 d->max_divergence < 1e-8;
    }
    flag(ref, "passed", passed);
    json_object_object_add(qualification, "duct_reference_gate", ref);
    flag(qualification, "physical_accuracy_certified", false);
    if (s->observed && !s->steady) {
        num(qualification, "manufactured_velocity_l2_error_m_s", t->velocity_l2_error);
        num(qualification, "manufactured_pressure_l2_error_pa", t->pressure_l2_error);
    }
    text(qualification, "evidence", "docs/cfd_3d_goal.md; build/c3d");
    text(qualification, "scope",
         "continuous rectangular-duct reference and periodic manufactured transient; no arbitrary "
         "3D CFD certificate");
    json_object_object_add(out, "physics", physics);
    json_object_object_add(out, "health", health);
    json_object_object_add(out, "energy_budget", energy);
    json_object_object_add(out, "boundary_force_budget", force);
    json_object_object_add(out, "qualification", qualification);
    if (full && s->ready) {
        struct json_object *fields = json_object_new_object(), *velocity = json_object_new_array(),
                           *pressure = json_object_new_array();
        text(fields, "schema", "physics_sim_cartesian3d_fields_v1");
        text(fields, "ordering",
             "q=(k*Ny+j)*Nx+i; velocity row is lower X/Y/Z face m/s; pressure cell-center Pa; duct "
             "upper Y/Z normal wall faces implicitly zero");
        json_object_object_add(fields, "spacing_m", array(g->h, 3));
        const double *v = s->steady ? d->velocity : t->velocity,
                     *p = s->steady ? d->pressure : t->pressure;
        for (int q = 0; q < g->count; q++) {
            double row[3] = {v[q], v[g->count + q], v[2 * g->count + q]};
            json_object_array_add(velocity, array(row, 3));
            json_object_array_add(pressure, s->observed ? json_object_new_double(p[q]) : NULL);
        }
        json_object_object_add(fields, "velocity_faces_m_s", velocity);
        json_object_object_add(fields, "pressure_pa", pressure);
        json_object_object_add(out, "cartesian_fields", fields);
    }
}
static void values(const Cfd3dSession *s, const double xyz[3], double out[11]) {
    if(s->obstacle){cfd_obstacle3d_sample_values(s,xyz,out);return;}
    if (s->transient_kind) {
        cfd_transient3d_sample_values(s, xyz, out);
        return;
    }
    memset(out, 0, 11 * sizeof(double));
    out[6] = out[9] = out[7] = out[8] = out[10] = NAN;
    const CfdCartesian3d *g = &s->grid;
    int q = 0, stride = 1;
    for (int a = 0; a < 3; a++) {
        int c = (int)floor(xyz[a] / g->h[a]);
        c = c < 0 ? 0 : c >= g->n[a] ? g->n[a] - 1 : c;
        q += c * stride;
        stride *= g->n[a];
    }
    const double *v = s->open     ? s->open_duct.velocity
                      : s->steady ? s->duct.velocity
                                  : s->transient.velocity,
                 *p = s->open     ? s->open_duct.pressure
                      : s->steady ? s->duct.pressure
                                  : s->transient.pressure;
    for (int a = 0; a < 3; a++)
        out[3 + a] =
            .5 * (v[a * g->count + q] + v[a * g->count + cfd_cartesian3d_neighbor(g, q, a, 1)]);
    if (s->open) {
        int i = q % g->n[0], j = (q / g->n[0]) % g->n[1], k = q / (g->n[0] * g->n[1]);
        for (int a = 0; a < 3; a++) {
            int c[3] = {i, j, k}, hi[3] = {i, j, k};
            hi[a]++;
            out[3 + a] = .5 * (cfd_open3d_face(&s->open_duct, a, c[0], c[1], c[2]) +
                               cfd_open3d_face(&s->open_duct, a, hi[0], hi[1], hi[2]));
        }
    }
    if (s->steady &&
        (xyz[1] <= 0 || xyz[1] >= g->length[1] || xyz[2] <= 0 || xyz[2] >= g->length[2]))
        out[3] = out[4] = out[5] = 0;
    out[0] = sqrt(out[3] * out[3] + out[4] * out[4] + out[5] * out[5]);
    if (!s->observed)
        return;
    out[9] = s->open ? p[q] : s->steady ? s->duct.gradient_pa_m * (g->length[0] - xyz[0]) : p[q];
    double derivative[3][3];
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) {
            int plus = cfd_cartesian3d_neighbor(g, q, b, 1),
                minus = cfd_cartesian3d_neighbor(g, q, b, -1);
            double up = .5 * (v[a * g->count + plus] +
                              v[a * g->count + cfd_cartesian3d_neighbor(g, plus, a, 1)]);
            double um = .5 * (v[a * g->count + minus] +
                              v[a * g->count + cfd_cartesian3d_neighbor(g, minus, a, 1)]);
            if (s->steady && b > 0) {
                int st = b == 1 ? g->n[0] : g->n[0] * g->n[1], c = (q / st) % g->n[b];
                if (c == 0)
                    um = -out[3 + a];
                if (c == g->n[b] - 1)
                    up = -out[3 + a];
            }
            derivative[a][b] = a == b ? (v[a * g->count + cfd_cartesian3d_neighbor(g, q, a, 1)] -
                                         v[a * g->count + q]) /
                                            g->h[a]
                                      : (up - um) / (2 * g->h[b]);
        }
    if (s->open)
        cfd_open3d_derivatives(&s->open_duct, q % g->n[0], (q / g->n[0]) % g->n[1],
                               q / (g->n[0] * g->n[1]), derivative);
    out[7] = derivative[0][0] + derivative[1][1] + derivative[2][2];
    double curl[3] = {derivative[2][1] - derivative[1][2], derivative[0][2] - derivative[2][0],
                      derivative[1][0] - derivative[0][1]};
    out[8] = sqrt(curl[0] * curl[0] + curl[1] * curl[1] + curl[2] * curl[2]);
    out[10] = s->mu * (derivative[0][1] + derivative[1][0]);
}
struct json_object *cfd_3d_session_sample(const Cfd3dSession *s, struct json_object *req) {
    if (!s || !s->ready) {
        struct json_object *o = json_object_new_object();
        text(o, "error", "cartesian3d_session_not_ready");
        return o;
    }
    CfdChannel layout = {0};
    layout.length = s->grid.length[0];
    layout.height = s->grid.length[1];
    layout.width = s->grid.length[2];
    layout.n = s->grid.n[1];
    layout.rho = s->rho;
    layout.mu = s->mu;
    struct json_object *o = cfd_channel_sample(&layout, req), *samples = get(o, "samples");
    int ua = json_object_get_int(get(o, "u_axis")), va = json_object_get_int(get(o, "v_axis")),
        na = json_object_get_int(get(o, "normal_axis"));
    int width = json_object_get_int(get(o, "width")),
        height = json_object_get_int(get(o, "height")), count = req ? 11 : 3;
    double lo[11], hi[11], sum[11] = {0};
    int finite[11] = {0};
    for (int a = 0; a < count; a++) {
        lo[a] = INFINITY;
        hi[a] = -INFINITY;
    }
    for (int j = 0; j < height; j++)
        for (int i = 0; i < width; i++) {
            double xyz[3] = {0}, v[11];
            xyz[na] = json_object_get_double(get(o, "slice_world_m"));
            xyz[ua] = (i + .5) * s->grid.length[ua] / width;
            xyz[va] = (j + .5) * s->grid.length[va] / height;
            values(s, xyz, v);
            json_object_array_put_idx(samples, j * width + i, array(v, count));
            for (int a = 0; a < count; a++)
                if (isfinite(v[a])) {
                    lo[a] = fmin(lo[a], v[a]);
                    hi[a] = fmax(hi[a], v[a]);
                    sum[a] += v[a];
                    finite[a]++;
                }
        }
    struct json_object *fields = get(o, "fields"), *stats = get(o, "statistics"),
                       *probes = get(o, "probes");
    for (int a = 0; a < count; a++) {
        if (a == 6)
            continue; /* unavailable pressure_proxy has no statistics object */
        struct json_object *stat =
            get(stats, json_object_get_string(json_object_array_get_idx(fields, a)));
        num(stat, "min", lo[a]);
        num(stat, "max", hi[a]);
        num(stat, "mean", finite[a] ? sum[a] / finite[a] : NAN);
        num(stat, "finite_samples", finite[a]);
    }
    for (size_t i = 0; i < json_object_array_length(probes); i++) {
        struct json_object *p = json_object_array_get_idx(probes, i);
        if (!json_object_get_boolean(get(p, "inside")))
            continue;
        struct json_object *position = get(p, "requested_world_m");
        double xyz[3], v[11];
        for (int a = 0; a < 3; a++)
            xyz[a] = json_object_get_double(json_object_array_get_idx(position, a));
        values(s, xyz, v);
        json_object_object_add(p, "values", array(v, 11));
    }
    num(o, "speed_max", hi[0]);
    double grid[3] = {s->grid.n[0], s->grid.n[1], s->grid.n[2]};
    json_object_object_add(o, "grid", array(grid, 3));
    text(o, "sampling",
         s->transient_kind ? "containing cell Pa pressure; adjacent physical MAC faces; Y/Z "
                             "no-slip; mode-specific physical cell strain; no advance"
         : s->open
             ? "containing cell solved pressure Pa; open MAC adjacent-face velocity; physical "
               "strain gradients; no advance"
             : "containing cell; adjacent staggered-face average velocity; cell gradients "
               "diagnostic; XY "
               "shear component; duct linear pressure from solved flow multiplier; no advance");
    return o;
}
