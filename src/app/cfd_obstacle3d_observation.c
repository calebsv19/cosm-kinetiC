#define _DARWIN_C_SOURCE 1
#include "app/cfd_3d_session.h"
#include <math.h>
#include <string.h>
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
    struct json_object *o = json_object_new_array();
    for (int a = 0; a < n; a++)
        json_object_array_add(o, isfinite(v[a]) ? json_object_new_double(v[a]) : NULL);
    return o;
}
void cfd_obstacle3d_session_snapshot(const Cfd3dSession *s, struct json_object *out, bool full) {
    const CfdObstacle3d *d = &s->obstacle_duct;
    const CfdCartesian3d *g = &s->grid;
    text(out, "model", "incompressible_cartesian3d_v1");
    text(out, "solve_mode", s->box ? "steady_box_duct" : "steady_obstacle_duct");
    text(out, "model_limitations", s->box
         ? "stationary grid-aligned rectangular box in 2x2 m duct; creeping Stokes only; independent force/refinement qualification required; no moving/STL/turbulent CFD claim"
         : "stationary aligned 1 m cube in 2x2 m duct; creeping Stokes only; independent "
         "force/refinement qualification required; no moving/STL/turbulent CFD claim");
    text(out, "tick_semantics",
         "one stationary coupled solve with flow-constraint pressure response; physical time zero");
    double grid[3] = {g->n[0], g->n[1], g->n[2]};
    json_object_object_add(out, "effective_grid", array(grid, 3));
    text(out, "grid_kind",
         "compact masked staggered Cartesian fluid topology, eliminated normal body faces and "
         "no-slip tangential half cells");
    num(out, "voxel_size_m", s->ready ? g->h[1] : NAN);
    num(out, "estimated_dense_bytes", (double)(4 * g->count + g->n[1] * g->n[2]) * sizeof(double));
    struct json_object *geometry = json_object_new_array(), *body = json_object_new_object();
    double lo[3] = {d->center_x - .5, .5, .5}, hi[3] = {d->center_x + .5, 1.5, 1.5};
    double characteristic = 1, projected_area = 1;
    if (s->box) {
        characteristic = 0;
        for (int a = 0; a < 3; a++) {
            lo[a] = d->lo[a] * g->h[a];
            hi[a] = d->hi[a] * g->h[a];
            characteristic = fmax(characteristic, hi[a] - lo[a]);
        }
        projected_area = (hi[1] - lo[1]) * (hi[2] - lo[2]);
        num(body, "projected_area_m2", projected_area);
        num(body, "characteristic_length_m", characteristic);
    }
    text(body, "kind", s->box ? "stationary_aligned_box" : "stationary_aligned_cube");
    json_object_object_add(body, "min_m", array(lo, 3));
    json_object_object_add(body, "max_m", array(hi, 3));
    json_object_array_add(geometry, body);
    json_object_object_add(out, "geometry", geometry);
    struct json_object *physics = json_object_new_object(), *health = json_object_new_object(),
                       *memory = json_object_new_object(), *cost = json_object_new_object(),
                       *energy = json_object_new_object(), *force = json_object_new_object(),
                       *qualification = json_object_new_object();
    json_object_object_add(physics, "dimensions_m", array(g->length, 3));
    num(physics, "density_kg_m3", s->rho);
    num(physics, "dynamic_viscosity_pa_s", s->mu);
    num(physics, "pressure_drop_pa", s->observed ? d->inlet_pressure : NAN);
    num(physics, "mean_body_reynolds", s->rho * d->requested_flow * characteristic / (4 * s->mu));
    num(physics, "max_speed_body_reynolds", s->observed ? s->rho * d->max_speed * characteristic / s->mu : NAN);
    text(physics, "equations",
         "steady incompressible Stokes; physical Pa pressure solved jointly; no body forcing or "
         "inertial transport");
    text(physics, "boundary_model",
         "Y/Z and six body planes no-slip; both X ends natural mu du/dn-p n=-P n; Pin solved from "
         "Q; Pout=0");
    num(health, "fluid_cells", d->cells);
    num(health, "stored_face_values", d->count);
    num(health, "linear_relative_residual", s->observed ? d->relative_residual : NAN);
    num(health, "max_abs_divergence_s_inv", s->observed ? d->max_divergence : NAN);
    num(health, "linear_iterations", s->observed ? d->iterations : 0);
    num(health, "velocity_inner_iterations", s->observed ? d->inner_iterations : 0);
    num(health, "volume_flux_m3_s", s->observed ? d->outlet_flow : NAN);
    num(health, "inlet_volume_flux_m3_s", s->observed ? d->inlet_flow : NAN);
    num(health, "flux_conservation_relative_error", s->observed ? d->flux_error : NAN);
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
         "owned numerical topology, CSR/MG/Krylov and fields; excludes JSON/process RSS");
    json_object_object_add(health, "numerical_memory", memory);
    num(cost, "setup_cpu_ms", d->setup_cpu_ms);
    num(cost, "mixed_solve_cpu_ms", s->observed ? d->solve_cpu_ms : NAN);
    text(cost, "scope",
         "process CPU including force/energy observation; serialization/export separate");
    json_object_object_add(health, "phase_cost", cost);
    flag(energy, "available", s->observed);
    flag(energy, "transient_energy_rate_available", false);
    text(energy, "physical_strain_scope",
         "exact strain-square quadrature of a continuous half-cell trilinear velocity interpolant "
         "with actual no-slip planes and edges; independent of matrix work");
    num(energy, "reconstruction_divergence_l2_s_inv_m_3_2",
        s->observed ? d->reconstruction_divergence_l2 : NAN);
    text(energy, "reconstruction_divergence_scope",
         "L2 divergence of interpolated velocity; distinct from exact MAC cell-integrated "
         "continuity");
    num(energy, "legacy_cell_gradient_dissipation_w",
        s->observed ? d->legacy_physical_dissipation : NAN);
    num(energy, "physical_strain_dissipation_w", s->observed ? d->physical_dissipation : NAN);
    num(energy, "physical_boundary_power_w", s->observed ? d->physical_power : NAN);
    num(energy, "natural_traction_power_w", s->observed ? d->natural_power : NAN);
    num(energy, "discrete_diffusion_w", s->observed ? d->discrete_dissipation : NAN);
    num(energy, "physical_relative_imbalance", s->observed ? d->physical_energy_imbalance : NAN);
    num(energy, "discrete_relative_imbalance", s->observed ? d->discrete_energy_imbalance : NAN);
    num(energy, "residual_w", s->observed ? d->physical_power - d->physical_dissipation : NAN);
    flag(force, "available", s->observed);
    text(force, "scope", s->box
         ? "all six closed box planes; force of fluid on body; pressure and symmetric physical viscous stress separately"
         : "all six closed cube planes; force of fluid on body; pressure and symmetric physical "
         "viscous stress separately");
    text(force, "normal_viscous_trace_policy",
         "zero from exact flat stationary no-slip incompressibility identity; not the raw "
         "normal stress of the non-solenoidal energy interpolant");
    flag(force, "shares_unrestricted_stress_with_energy_interpolant", false);
    if (s->observed) {
        double total[3], closure[3] = {0, 0, 0};
        for (int a = 0; a < 3; a++)
            total[a] = d->pressure_force[a] + d->viscous_force[a];
        json_object_object_add(force, "discrete_body_pressure_force_n",
                               array(d->discrete_pressure_force, 3));
        json_object_object_add(force, "discrete_body_viscous_force_n",
                               array(d->discrete_viscous_force, 3));
        json_object_object_add(force, "discrete_momentum_residual_n",
                               array(d->discrete_momentum_residual, 3));
        text(force, "discrete_reaction_scope",
             "sum of integrated vector-Laplacian boundary rows; separate from reconstructed "
             "physical Cauchy stress");
        json_object_object_add(force, "body_pressure_force_n", array(d->pressure_force, 3));
        json_object_object_add(force, "body_viscous_force_n", array(d->viscous_force, 3));
        json_object_object_add(force, "legacy_quadratic_viscous_force_n",
                               array(d->legacy_viscous_force, 3));
        text(force, "wall_stress_scope",
             "integrated half-cell no-slip trace; tangential derivative of zero wall trace and "
             "normal derivative from incompressibility are zero; legacy quadratic trace separate");
        json_object_object_add(force, "body_total_force_n", array(total, 3));
        json_object_object_add(force, "closed_area_vector_m2", array(closure, 3));
        struct json_object *sides = json_object_new_array();
        for (int a = 0; a < 6; a++) {
            struct json_object *v = json_object_new_object();
            num(v, "axis", a / 2);
            num(v, "side", a % 2);
            double area = 1;
            if (s->box)
                for (int b = 0; b < 3; b++)
                    if (b != a / 2)
                        area *= hi[b] - lo[b];
            num(v, "area_m2", area);
            json_object_object_add(v, "pressure_force_n", array(d->side_pressure[a], 3));
            json_object_object_add(v, "viscous_force_n", array(d->side_viscous[a], 3));
            json_object_array_add(sides, v);
        }
        json_object_object_add(force, "body_sides", sides);
        struct json_object *walls = json_object_new_array();
        for (int a = 0; a < 4; a++)
            json_object_array_add(walls, array(d->wall_force[a], 3));
        json_object_object_add(force, "outer_wall_force_n", walls);
        json_object_object_add(force, "momentum_residual_n", array(d->momentum_residual, 3));
        double momentum_max = fabs(d->momentum_residual[0]);
        if (s->box)
            for (int a = 1; a < 3; a++)
                momentum_max = fmax(momentum_max, fabs(d->momentum_residual[a]));
        num(force, "momentum_relative_residual", momentum_max / (4 * d->inlet_pressure));
        num(force, "drag_coefficient", 2 * total[0] / (s->rho * pow(d->requested_flow / 4, 2) * projected_area));
        text(force, "drag_coefficient_scope", s->box
             ? "reference area=actual YZ body projection; mean duct speed=Q/4; confined creeping box, not free-space or high-Re Cd"
             : "reference area=1 m2 and mean duct speed=Q/4; confined creeping cube, not free-space "
             "or high-Re Cd");
    }
    struct json_object *wake = json_object_new_array();
    for (double x = s->box ? hi[0] + .25 : d->center_x + .75; s->ready && x < g->length[0]; x += .25) {
        double xyz[3] = {x, s->box ? .5*(lo[1]+hi[1]) : 1, s->box ? .5*(lo[2]+hi[2]) : 1}, v[11];
        cfd_obstacle3d_sample_values(s, xyz, v);
        struct json_object *point = json_object_new_object();
        num(point, "x_m", x);
        if (s->box) { num(point, "y_m", xyz[1]); num(point, "z_m", xyz[2]); }
        num(point, "vx_m_s", v[3]);
        num(point, "pressure_pa", v[9]);
        json_object_array_add(wake, point);
    }
    json_object_object_add(out, "centerline_recovery", wake);
    flag(qualification, "physical_accuracy_certified", false);
    text(qualification, "reference_status", "not_established");
    text(qualification, "scope",
         "independent converged same-domain reference plus multi-grid and boundary-distance audit "
         "required");
    text(qualification, "evidence", s->box ? "docs/cfd_3d_box_checkpoint.md; build/c3d-box" : "docs/cfd_obstacle3d_goal.md; build/c3d-obstacle");
    json_object_object_add(out, "physics", physics);
    json_object_object_add(out, "health", health);
    json_object_object_add(out, "energy_budget", energy);
    json_object_object_add(out, "boundary_force_budget", force);
    json_object_object_add(out, "qualification", qualification);
    if (full && s->ready) {
        struct json_object *fields = json_object_new_object(), *v = json_object_new_array(),
                           *p = json_object_new_array(), *mask = json_object_new_array(),
                           *upper = json_object_new_array();
        text(fields, "schema", "physics_sim_cartesian3d_fields_v1");
        text(fields, "ordering",
             "q=(k*Ny+j)*Nx+i; lower XYZ faces, cell pressure; solid pressure null and mask true; "
             "upper Y/Z wall zero, upper X separate");
        json_object_object_add(fields, "spacing_m", array(g->h, 3));
        for (int k = 0; k < g->n[2]; k++)
            for (int j = 0; j < g->n[1]; j++)
                for (int i = 0; i < g->n[0]; i++) {
                    double row[3];
                    for (int a = 0; a < 3; a++)
                        row[a] = cfd_obstacle3d_face(d, a, i, j, k);
                    json_object_array_add(v, array(row, 3));
                    bool solid = cfd_obstacle_mixed3d_cell(d->mixed, i, j, k) < 0;
                    json_object_array_add(mask, json_object_new_boolean(solid));
                    json_object_array_add(
                        p, !solid && s->observed
                               ? json_object_new_double(cfd_obstacle3d_pressure(d, i, j, k))
                               : NULL);
                }
        for (int k = 0; k < g->n[2]; k++)
            for (int j = 0; j < g->n[1]; j++)
                json_object_array_add(
                    upper, json_object_new_double(cfd_obstacle3d_face(d, 0, g->n[0], j, k)));
        json_object_object_add(fields, "velocity_faces_m_s", v);
        json_object_object_add(fields, "pressure_pa", p);
        json_object_object_add(fields, "solid_mask", mask);
        json_object_object_add(fields, "outlet_x_velocity_m_s", upper);
        json_object_object_add(out, "cartesian_fields", fields);
    }
}
void cfd_obstacle3d_sample_values(const Cfd3dSession *s, const double xyz[3], double out[11]) {
    memset(out, 0, 11 * sizeof(double));
    out[6] = out[7] = out[8] = out[9] = out[10] = NAN;
    const CfdObstacle3d *d = &s->obstacle_duct;
    if (!s->ready || !d->mixed)
        return;
    int c[3];
    for (int a = 0; a < 3; a++) {
        c[a] = (int)floor(xyz[a] / s->grid.h[a]);
        if (c[a] < 0)
            c[a] = 0;
        if (c[a] >= s->grid.n[a])
            c[a] = s->grid.n[a] - 1;
    }
    bool solid = cfd_obstacle_mixed3d_cell(d->mixed, c[0], c[1], c[2]) < 0;
    out[2] = solid;
    if (solid)
        return;
    double div = 0;
    for (int a = 0; a < 3; a++) {
        int hi[3] = {c[0], c[1], c[2]};
        hi[a]++;
        double lo = cfd_obstacle3d_face(d, a, c[0], c[1], c[2]),
               upper = cfd_obstacle3d_face(d, a, hi[0], hi[1], hi[2]);
        out[3 + a] = .5 * (lo + upper);
        div += (upper - lo) / s->grid.h[a];
    }
    out[0] = sqrt(out[3] * out[3] + out[4] * out[4] + out[5] * out[5]);
    if (s->observed) {
        out[7] = div;
        out[9] = cfd_obstacle3d_pressure(d, c[0], c[1], c[2]);
        double grad[3][3];
        cfd_obstacle3d_derivatives(d, c[0], c[1], c[2], grad);
        double curl[3] = {grad[2][1] - grad[1][2], grad[0][2] - grad[2][0],
                          grad[1][0] - grad[0][1]};
        out[8] = sqrt(curl[0] * curl[0] + curl[1] * curl[1] + curl[2] * curl[2]);
        out[10] = s->mu * (grad[0][1] + grad[1][0]);
    }
}
