#define _DARWIN_C_SOURCE 1
#include "app/cfd_3d_session.h"
#include <math.h>
#include <sys/resource.h>
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
void cfd_open3d_session_snapshot(const Cfd3dSession *s, struct json_object *out, bool full) {
    const CfdOpen3d *d = &s->open_duct;
    const CfdCartesian3d *g = &s->grid;
    text(out, "model", "incompressible_cartesian3d_v1");
    text(out, "solve_mode", "steady_open_duct");
    text(out, "model_limitations",
         "Stationary uniform MAC Stokes straight duct only. Analytic developed inlet; natural "
         "vector-Laplacian outlet. No transient, obstacle, turbulence or general open CFD "
         "qualification.");
    text(out, "tick_semantics",
         "one stationary coupled pressure/velocity solve; physical time stays zero");
    double grid[3] = {g->n[0], g->n[1], g->n[2]};
    json_object_object_add(out, "effective_grid", array(grid, 3));
    text(out, "grid_kind", "uniform staggered Cartesian faces; independent open outlet X face");
    num(out, "voxel_size_m", s->ready ? fmin(g->h[0], fmin(g->h[1], g->h[2])) : NAN);
    num(out, "estimated_dense_bytes", (double)(4 * g->count + g->n[1] * g->n[2]) * sizeof(double));
    text(out, "dense_estimate_scope",
         "published lower faces, upper outlet and cell pressure only; excludes matrices, MG/Krylov "
         "and JSON");
    json_object_object_add(out, "geometry", json_object_new_array());
    struct json_object *physics = json_object_new_object(), *health = json_object_new_object(),
                       *memory = json_object_new_object(), *cost = json_object_new_object(),
                       *energy = json_object_new_object(), *force = json_object_new_object(),
                       *qualification = json_object_new_object(), *ref = json_object_new_object();
    json_object_object_add(physics, "dimensions_m", array(g->length, 3));
    num(physics, "density_kg_m3", s->rho);
    num(physics, "dynamic_viscosity_pa_s", s->mu);
    text(physics, "equations",
         "stationary incompressible Stokes: -mu laplacian(u)+grad(p)=0; div(u)=0; no body forcing");
    text(physics, "boundary_model",
         "X=0 prescribed continuous face-area-average developed velocity; Y/Z no-slip; X=L mu "
         "du/dx - p e_x = 0. Vector-Laplacian traction, not zero symmetric Cauchy traction. No "
         "independently pinned pressure.");
    num(physics, "pressure_drop_pa", s->observed ? d->pressure_drop_pa : NAN);
    num(physics, "inlet_pressure_trace_pa", s->observed ? d->pressure_in_pa : NAN);
    num(physics, "outlet_pressure_trace_pa", s->observed ? d->pressure_out_pa : NAN);
    num(physics, "upstream_gradient_pa_m", s->observed ? d->upstream_gradient_pa_m : NAN);
    text(physics, "pressure_trace_method",
         "second-order extrapolation of solved cross-section cell means; upstream least-squares "
         "fit at cell x in [1,3] m");
    num(health, "fluid_cells", g->count);
    num(health, "stored_face_values", 3 * g->count + g->n[1] * g->n[2]);
    num(health, "linear_relative_residual", s->observed ? d->relative_residual : NAN);
    num(health, "max_abs_divergence_s_inv", s->observed ? d->max_divergence : NAN);
    num(health, "linear_iterations", s->observed ? d->iterations : 0);
    num(health, "velocity_inner_iterations", s->observed ? d->velocity_iterations : 0);
    num(health, "volume_flux_m3_s", s->observed ? d->outlet_flow : NAN);
    num(health, "inlet_volume_flux_m3_s", s->observed ? d->inlet_flow : NAN);
    num(health, "flux_conservation_relative_error", s->observed ? d->flux_relative_error : NAN);
    text(health, "projection_status",
         s->failed     ? "failed"
         : s->observed ? "converged"
                       : "not_solved");
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#ifdef __APPLE__
        num(health, "process_peak_rss_bytes", usage.ru_maxrss);
#else
        num(health, "process_peak_rss_bytes", (double)usage.ru_maxrss * 1024);
#endif
    }
    num(memory, "limit_bytes", s->memory.limit_bytes);
    num(memory, "live_bytes", s->memory.live_bytes);
    num(memory, "peak_bytes", s->memory.peak_bytes);
    num(memory, "rejected_allocations", s->memory.rejected_allocations);
    text(memory, "scope",
         "owned numerical allocations, headers and setup overlap; excludes JSON and process RSS");
    json_object_object_add(health, "numerical_memory", memory);
    num(cost, "setup_cpu_ms", s->ready ? d->setup_cpu_ms : NAN);
    num(cost, "mixed_solve_cpu_ms", s->observed ? d->solve_cpu_ms : NAN);
    text(cost, "scope",
         "process CPU; cached MG velocity inverses, matrix-free Schur CG and physical observation; "
         "controls serviced between solves");
    json_object_object_add(health, "phase_cost", cost);
    flag(energy, "available", s->observed);
    flag(energy, "transient_energy_rate_available", false);
    num(energy, "physical_strain_dissipation_w", s->observed ? d->dissipation_w : NAN);
    num(energy, "physical_boundary_power_w", s->observed ? d->boundary_power_w : NAN);
    num(energy, "physical_relative_imbalance", s->observed ? d->energy_imbalance : NAN);
    num(energy, "residual_w", s->observed ? d->boundary_power_w - d->dissipation_w : NAN);
    num(energy, "discrete_diffusion_w", s->observed ? d->discrete_diffusion_w : NAN);
    text(energy, "physical_work_method",
         "solved pressure traces and independently reconstructed symmetric stress work; cell 2mu "
         "S:S volume quadrature, separate from matrix diffusion");
    flag(force, "available", s->observed);
    if (s->observed)
        json_object_object_add(force, "wall_drag_n", array(d->wall_force_n, 4));
    text(force, "wall_order", "y_min,y_max,z_min,z_max; force on stationary walls in +X");
    flag(ref, "applicable", true);
    flag(ref, "passed", s->observed && cfd_open3d_gate(d));
    num(ref, "velocity_relative_l2", s->observed ? d->velocity_relative_l2 : NAN);
    num(ref, "pressure_relative_error", s->observed ? d->pressure_relative_error : NAN);
    num(ref, "volume_flow_relative_error",
        s->observed ? fmax(fabs(d->inlet_flow - d->requested_flow),
                           fabs(d->outlet_flow - d->requested_flow)) /
                          d->requested_flow
                    : NAN);
    double wall = 0;
    for (int a = 0; a < 4; a++)
        wall = fmax(wall, d->wall_relative_error[a]);
    num(ref, "max_wall_relative_error", s->observed ? wall : NAN);
    if (s->observed)
        json_object_object_add(ref, "wall_relative_error", array(d->wall_relative_error, 4));
    num(ref, "energy_relative_error", s->observed ? d->energy_relative_error : NAN);
    num(ref, "energy_imbalance", s->observed ? d->energy_imbalance : NAN);
    num(ref, "flux_relative_error", s->observed ? d->flux_relative_error : NAN);
    json_object_object_add(qualification, "duct_reference_gate", ref);
    flag(qualification, "physical_accuracy_certified", false);
    text(qualification, "evidence",
         "docs/cfd_open3d_gate.md; docs/cfd_open3d_completion.md; build/c3d-open");
    text(qualification, "scope",
         "independent continuous straight open-duct reference only; steady outlet-distance screen "
         "is a separate multi-run gate");
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
             "q=(k*Ny+j)*Nx+i; lower X/Y/Z face rows; cell-center pressure Pa; upper Y/Z wall "
             "faces zero; upper X outlet stored separately as k*Ny+j");
        json_object_object_add(fields, "spacing_m", array(g->h, 3));
        for (int q = 0; q < g->count; q++) {
            double row[3] = {d->velocity[q], d->velocity[g->count + q],
                             d->velocity[2 * g->count + q]};
            json_object_array_add(v, array(row, 3));
            json_object_array_add(p, s->observed ? json_object_new_double(d->pressure[q]) : NULL);
        }
        json_object_object_add(fields, "velocity_faces_m_s", v);
        json_object_object_add(fields, "pressure_pa", p);
        json_object_object_add(fields, "outlet_x_velocity_m_s",
                               array(d->outlet_velocity, g->n[1] * g->n[2]));
        json_object_object_add(out, "cartesian_fields", fields);
    }
}
