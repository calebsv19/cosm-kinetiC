#define _DARWIN_C_SOURCE 1
#include "app/cfd_3d_session.h"
#include <math.h>
#include <string.h>
#include <sys/resource.h>
static void number(struct json_object *o, const char *key, double value) {
    json_object_object_add(o, key, isfinite(value) ? json_object_new_double(value) : NULL);
}
static void text(struct json_object *o, const char *key, const char *value) {
    json_object_object_add(o, key, json_object_new_string(value));
}
static void flag(struct json_object *o, const char *key, bool value) {
    json_object_object_add(o, key, json_object_new_boolean(value));
}
static struct json_object *array(const double *value, int count) {
    struct json_object *out = json_object_new_array();
    for (int q = 0; q < count; q++)
        json_object_array_add(out, isfinite(value[q]) ? json_object_new_double(value[q]) : NULL);
    return out;
}
static bool startup(const Cfd3dSession *s) { return s->transient_kind == CFD_3D_PRESSURE_STARTUP; }
static const double *velocity(const Cfd3dSession *s) {
    return startup(s) ? s->startup.velocity : s->wall.velocity;
}
static const double *pressure(const Cfd3dSession *s) {
    return startup(s) ? s->startup.pressure : s->wall.pressure;
}
static CfdMixed3d *mixed(const Cfd3dSession *s) {
    return startup(s) ? s->startup.mixed : s->wall.mixed;
}
static double face(const Cfd3dSession *s, int a, int i, int j, int k) {
    int q = cfd_mixed3d_index(mixed(s), a, i, j, k);
    return q >= 0 ? velocity(s)[q] : 0;
}
static double center(const Cfd3dSession *s, int a, int i, int j, int k) {
    int hi[3] = {i, j, k};
    hi[a]++;
    return .5 * (face(s, a, i, j, k) + face(s, a, hi[0], hi[1], hi[2]));
}
/* Startup sampling reproduces that model's published physical cell strain;
 * wall-MMS sampling uses its separately verified higher-order reconstruction. */
static void derivatives(const Cfd3dSession *s, int i, int j, int k, double d[3][3]) {
    if (!startup(s)) {
        cfd_wall3d_derivatives(&s->wall, i, j, k, d);
        return;
    }
    int c[3] = {i, j, k};
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) {
            int hi[3] = {i, j, k}, lo[3] = {i, j, k};
            hi[b]++;
            lo[b]--;
            if (a == b)
                d[a][b] = (face(s, a, hi[0], hi[1], hi[2]) - face(s, a, i, j, k)) / s->grid.h[b];
            else {
                double uc = center(s, a, i, j, k);
                double up = c[b] == s->grid.n[b] - 1 ? (b == 0 ? uc : -uc)
                                                     : center(s, a, hi[0], hi[1], hi[2]);
                double um = c[b] == 0 ? (b == 0 ? uc : -uc) : center(s, a, lo[0], lo[1], lo[2]);
                d[a][b] = (up - um) / (2 * s->grid.h[b]);
            }
        }
}
void cfd_transient3d_sample_values(const Cfd3dSession *s, const double xyz[3], double out[11]) {
    memset(out, 0, 11 * sizeof(double));
    out[6] = out[7] = out[8] = out[9] = out[10] = NAN;
    int c[3];
    for (int a = 0; a < 3; a++) {
        c[a] = (int)floor(xyz[a] / s->grid.h[a]);
        if (c[a] < 0)
            c[a] = 0;
        if (c[a] >= s->grid.n[a])
            c[a] = s->grid.n[a] - 1;
    }
    for (int a = 0; a < 3; a++)
        out[3 + a] = center(s, a, c[0], c[1], c[2]);
    if (xyz[1] <= 0 || xyz[1] >= s->grid.length[1] || xyz[2] <= 0 || xyz[2] >= s->grid.length[2])
        out[3] = out[4] = out[5] = 0;
    out[0] = hypot(out[3], hypot(out[4], out[5]));
    if (!s->observed)
        return;
    int q = (c[2] * s->grid.n[1] + c[1]) * s->grid.n[0] + c[0];
    out[9] = pressure(s)[q];
    double d[3][3];
    derivatives(s, c[0], c[1], c[2], d);
    out[7] = d[0][0] + d[1][1] + d[2][2];
    out[8] = hypot(d[2][1] - d[1][2], hypot(d[0][2] - d[2][0], d[1][0] - d[0][1]));
    out[10] = s->mu * (d[0][1] + d[1][0]);
}
static void wall_shear(const Cfd3dSession *s, double rms[8]) {
    memset(rms, 0, 8 * sizeof(double));
    const CfdCartesian3d *g = &s->grid;
    int count[8] = {0};
    for (int b = 1; b < 3; b++)
        for (int side = 0; side < 2; side++)
            for (int k = 0; k < g->n[2]; k++)
                for (int j = 0; j < g->n[1]; j++)
                    for (int i = 0; i < g->n[0]; i++) {
                        int c[3] = {i, j, k};
                        if (c[b] != (side ? g->n[b] - 1 : 0))
                            continue;
                        for (int a = 0, tangent = 0; a < 3; a++)
                            if (a != b) {
                                int slot = 4 * (b - 1) + 2 * side + tangent++;
                                double shear = -2 * s->mu * center(s, a, i, j, k) / g->h[b];
                                rms[slot] += shear * shear;
                                count[slot]++;
                            }
                    }
    for (int a = 0; a < 8; a++)
        rms[a] = sqrt(rms[a] / count[a]);
}
void cfd_transient3d_session_snapshot(const Cfd3dSession *s, struct json_object *out, bool full) {
    const CfdCartesian3d *g = &s->grid;
    const CfdWall3d *w = &s->wall;
    const CfdStartup3d *d = &s->startup;
    bool st = startup(s), solved = s->observed, open = st || w->open;
    text(out, "model", "incompressible_cartesian3d_v1");
    text(out, "solve_mode", cfd_3d_session_mode(s));
    text(out, "model_limitations",
         "Uniform Cartesian laminar known-answer wall flow or physical pressure startup. No "
         "obstacle, moving-body, turbulence, axial entrance, wake/backflow or arbitrary nonlinear "
         "open-outlet certificate.");
    text(out, "tick_semantics",
         "one accepted coupled BE/BDF2 velocity/pressure step; rejected/cancelled candidates never "
         "advance fields or time");
    double grid[3] = {g->n[0], g->n[1], g->n[2]};
    json_object_object_add(out, "effective_grid", array(grid, 3));
    number(out, "voxel_size_m", s->ready ? fmin(g->h[0], fmin(g->h[1], g->h[2])) : NAN);
    number(out, "estimated_dense_bytes",
           s->ready ? (double)((st ? d->count : w->count) + g->count) * sizeof(double) : NAN);
    text(out, "dense_estimate_scope",
         "owned velocity/pressure only; excludes matrices, references, MG, Krylov, JSON and RSS");
    text(out, "grid_kind",
         "uniform staggered Cartesian faces; no-slip Y/Z; explicit upper X face for open modes");
    json_object_object_add(out, "geometry", json_object_new_array());
    struct json_object *physics = json_object_new_object(), *health = json_object_new_object(),
                       *energy = json_object_new_object(), *force = json_object_new_object(),
                       *qualification = json_object_new_object(),
                       *reference = json_object_new_object();
    json_object_object_add(physics, "dimensions_m", array(g->length, 3));
    number(physics, "density_kg_m3", s->rho);
    number(physics, "dynamic_viscosity_pa_s", s->mu);
    text(physics, "equations",
         st ? "rho du/dt-mu laplacian(u)+grad(p)=0; div(u)=0; pressure-driven from rest"
         : w->transport
             ? "rho(du/dt+div(uu))-mu laplacian(u)+grad(p)=f; div(u)=0; continuous manufactured "
               "forcing"
             : "rho du/dt-mu laplacian(u)+grad(p)=f; div(u)=0; continuous manufactured forcing");
    text(physics, "boundary_model",
         st     ? "no-slip Y/Z; natural X pressure traction Pin=G*L/Pout=0; both normal face "
                  "velocities solved"
         : open ? "no-slip Y/Z; independent continuous mu du/dn-p n at both X ends; fixed 4 m "
                  "physical X wavelength"
                : "periodic X, no-slip Y/Z; zero-mean physical Pa pressure; no artificial wall "
                  "pressure-Neumann constraint");
    number(physics, "pressure_drop_pa", st && solved ? d->pressure_drop : NAN);
    number(health, "fluid_cells", g->count);
    number(health, "stored_face_values", s->ready ? (st ? d->count : w->count) : 0);
    number(health, "linear_relative_residual",
           solved ? (st ? d->true_residual : w->true_residual) : NAN);
    number(health, "max_abs_divergence_s_inv",
           solved ? (st ? d->max_divergence : w->max_divergence) : NAN);
    number(health, "linear_iterations", solved ? (st ? d->iterations : w->iterations) : 0);
    number(health, "velocity_inner_iterations",
           solved ? (st ? d->inner_iterations : w->inner_iterations) : 0);
    number(health, "volume_flux_m3_s", st && solved ? d->flow : NAN);
    number(health, "transport_cfl",
           !st && w->transport && solved ? s->accepted_transport_cfl : NAN);
    number(health, "transport_cfl_limit", !st && w->transport ? .25 : NAN);
    number(health, "velocity_amplitude_projection", !st && solved ? w->amplitude : NAN);
    number(health, "velocity_orthogonal_relative_error", !st && solved ? w->orthogonal_error : NAN);
    double projection = 0, norm = 0;
    if (!st && solved)
        for (int q = 0; q < g->count; q++) {
            projection += w->pressure[q] * w->pressure_reference[q];
            norm += w->pressure_reference[q] * w->pressure_reference[q];
        }
    number(health, "pressure_amplitude_projection_pa", !st && solved ? projection / norm : NAN);
    text(health, "projection_status", s->failed ? "failed" : solved ? "converged" : "not_solved");
    text(health, "last_step_outcome",
         s->cancelled ? "cancelled_preserved_last_accepted"
         : s->failed  ? "rejected"
         : solved     ? "accepted"
                      : "not_started");
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#ifdef __APPLE__
        number(health, "process_peak_rss_bytes", usage.ru_maxrss);
#else
        number(health, "process_peak_rss_bytes", (double)usage.ru_maxrss * 1024);
#endif
    }
    struct json_object *memory = json_object_new_object(), *cost = json_object_new_object();
    number(memory, "limit_bytes", s->memory.limit_bytes);
    number(memory, "live_bytes", s->memory.live_bytes);
    number(memory, "peak_bytes", s->memory.peak_bytes);
    number(memory, "rejected_allocations", s->memory.rejected_allocations);
    text(memory, "scope",
         "owned numerical arrays/headers/setup overlap; JSON and process RSS excluded");
    json_object_object_add(health, "numerical_memory", memory);
    number(cost, "setup_cpu_ms", s->ready ? (st ? d->setup_cpu_ms : w->setup_cpu_ms) : NAN);
    number(cost, "mixed_solve_cpu_ms", solved ? (st ? d->solve_cpu_ms : w->solve_cpu_ms) : NAN);
    text(cost, "scope",
         "process CPU for last accepted step including native physical observation; "
         "publication/export costs separate");
    json_object_object_add(health, "phase_cost", cost);
    flag(energy, "available", solved);
    flag(energy, "transient_energy_rate_available", solved);
    number(energy, "kinetic_energy_j", solved ? (st ? d->kinetic : w->kinetic_j) : NAN);
    number(energy, "physical_strain_dissipation_w",
           solved ? (st ? d->dissipation : w->dissipation_w) : NAN);
    number(energy, "physical_boundary_power_w",
           solved ? (st ? d->boundary_power : w->boundary_power_w) : NAN);
    number(energy, "body_force_power_w", solved ? (st ? 0 : w->forcing_power_w) : NAN);
    number(energy, "transport_power_w", solved ? s->accepted_transport_power_w : NAN);
    number(energy, "transport_self_power_w", solved ? s->accepted_transport_self_power_w : NAN);
    number(energy, "kinetic_energy_rate_w",
           solved ? (st ? d->energy_rate : w->energy_rate_w) : NAN);
    number(energy, "residual_w", solved ? (st ? d->energy_residual : w->energy_residual_w) : NAN);
    double scale =
        st ? d->reference_power : fmax(fabs(w->reference_power_w), w->reference_dissipation_w);
    double imbalance = solved ? fabs(st ? d->energy_residual : w->energy_residual_w) / scale : NAN;
    number(energy, "physical_relative_imbalance", imbalance);
    flag(force, "available", solved);
    flag(force, "body_force_available", false);
    if (st && solved) {
        json_object_object_add(force, "wall_drag_n", array(d->wall_force, 4));
        text(force, "wall_order", "y_min,y_max,z_min,z_max; integrated +X loads on walls");
    } else if (solved) {
        double rms[8];
        wall_shear(s, rms);
        json_object_object_add(force, "wall_tangential_shear_rms_pa", array(rms, 8));
        text(force, "wall_order",
             "y_min(u,w),y_max(u,w),z_min(u,v),z_max(u,v); signed reference components compared by "
             "nonzero RMS");
    }
    flag(reference, "applicable", true);
    double wall = 0;
    if (solved)
        for (int a = 0; a < (st ? 4 : 8); a++)
            wall = fmax(wall, st ? d->wall_error[a] : w->wall_error[a]);
    number(reference, "velocity_relative_l2",
           solved ? (st ? d->velocity_error : w->velocity_error) : NAN);
    number(reference, "pressure_relative_error",
           solved ? (st ? d->pressure_error : w->pressure_error) : NAN);
    number(reference, "volume_flow_relative_error", st && solved ? d->flow_error : NAN);
    number(reference, "max_wall_relative_error", solved ? wall : NAN);
    if (solved)
        json_object_object_add(reference, "wall_relative_error",
                               array(st ? d->wall_error : w->wall_error, st ? 4 : 8));
    double dissipation_error =
        solved
            ? (st ? d->dissipation_error : fabs(w->dissipation_w / w->reference_dissipation_w - 1))
            : NAN;
    number(reference, "energy_relative_error", dissipation_error);
    number(reference, "energy_imbalance", imbalance);
    double min_time = st ? .0125 * fmin(g->length[1], g->length[2]) *
                               fmin(g->length[1], g->length[2]) * s->rho / s->mu
                         : 0;
    number(reference, "minimum_qualified_startup_time_s", min_time);
    bool resolved_time = !st || s->time >= min_time - 1e-10;
    flag(reference, "reference_time_resolved", resolved_time);
    bool passed =
        solved && !s->failed && resolved_time &&
        (st ? d->velocity_error : w->velocity_error) <= (st ? .01 : .03) &&
        (st ? d->pressure_error : w->pressure_error) <= (st ? .01 : .03) &&
        wall <= (st ? .02 : .05) && dissipation_error <= (st ? .02 : .05) &&
        imbalance <= (st ? .02 : .05) && (st ? d->true_residual : w->true_residual) <= 1e-11 &&
        (st ? d->max_divergence : w->max_divergence) < 1e-8 && (!st || d->flow_error <= .01);
    flag(reference, "passed", passed);
    text(reference, "pressure_error_scale",
         st ? "continuous physical pressure drop"
            : "fixed nonzero .01 Pa reference peak RMS; solved cell-centre pressure");
    json_object_object_add(qualification, "transient_reference_gate", reference);
    json_object_object_add(qualification, "harmonic_reference", cfd_3d_harmonic_snapshot(s));
    flag(qualification, "physical_accuracy_certified", false);
    text(qualification, "scope",
         "per-run independent continuous reference; no inherited multi-run convergence/outlet or "
         "arbitrary CFD certification");
    text(qualification, "evidence", "docs/cfd_wall3d_goal.md; build/c3d-wall");
    json_object_object_add(out, "physics", physics);
    json_object_object_add(out, "health", health);
    json_object_object_add(out, "energy_budget", energy);
    json_object_object_add(out, "boundary_force_budget", force);
    json_object_object_add(out, "qualification", qualification);
    if (full && s->ready) {
        struct json_object *fields = json_object_new_object(), *v = json_object_new_array(),
                           *p = json_object_new_array();
        text(fields, "schema", "physics_sim_cartesian3d_fields_v1");
        text(fields, "ordering",
             "q=(k*Ny+j)*Nx+i; row=lower X/Y/Z face m/s; known Y/Z normal walls zero; upper X "
             "stored separately when open");
        json_object_object_add(fields, "spacing_m", array(g->h, 3));
        struct json_object *periodic = json_object_new_array();
        for (int a = 0; a < 3; a++)
            json_object_array_add(periodic, json_object_new_boolean(a == 0 && !open));
        json_object_object_add(fields, "periodic_axes", periodic);
        for (int k = 0; k < g->n[2]; k++)
            for (int j = 0; j < g->n[1]; j++)
                for (int i = 0; i < g->n[0]; i++) {
                    double row[3] = {face(s, 0, i, j, k), face(s, 1, i, j, k), face(s, 2, i, j, k)};
                    json_object_array_add(v, array(row, 3));
                    int q = (k * g->n[1] + j) * g->n[0] + i;
                    json_object_array_add(p,
                                          solved ? json_object_new_double(pressure(s)[q]) : NULL);
                }
        json_object_object_add(fields, "velocity_faces_m_s", v);
        json_object_object_add(fields, "pressure_pa", p);
        if (open) {
            struct json_object *upper = json_object_new_array();
            for (int k = 0; k < g->n[2]; k++)
                for (int j = 0; j < g->n[1]; j++)
                    json_object_array_add(upper, json_object_new_double(face(s, 0, g->n[0], j, k)));
            json_object_object_add(fields, "outlet_x_velocity_m_s", upper);
        }
        json_object_object_add(out, "cartesian_fields", fields);
    }
}
