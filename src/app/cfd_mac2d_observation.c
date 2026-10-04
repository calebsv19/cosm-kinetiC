#include "app/cfd_mac2d_observation.h"
#include "app/cfd_channel_observation.h"
#include "app/cfd_surface_force.h"
#include <math.h>
#include <string.h>
static void number(struct json_object *o, const char *k, double v) {
    json_object_object_add(o, k, json_object_new_double(v));
}
static void text(struct json_object *o, const char *k, const char *v) {
    json_object_object_add(o, k, json_object_new_string(v));
}
static struct json_object *get(struct json_object *o, const char *k) {
    struct json_object *v = NULL;
    json_object_object_get_ex(o, k, &v);
    return v;
}
static struct json_object *array(const double *v, int n) {
    struct json_object *a = json_object_new_array();
    for (int i = 0; i < n; i++)
        json_object_array_add(a, json_object_new_double(v[i]));
    return a;
}
static struct json_object *grid(const CfdMac2D *c) {
    struct json_object *a = json_object_new_array();
    json_object_array_add(a, json_object_new_int(c->nx));
    json_object_array_add(a, json_object_new_int(c->ny));
    json_object_array_add(a, json_object_new_int(1));
    return a;
}
void cfd_mac2d_snapshot(const CfdMac2D *c, struct json_object *out, bool full) {
    CfdChannel mean = c->parameters;
    mean.time = c->time;
    double dy = mean.height / c->ny, dx = mean.length / c->nx, flow = 0, energy = 0;
    for (int j = 0; j < c->ny; j++) {
        mean.u[j] = 0;
        for (int i = 0; i < c->nx; i++) {
            double u = c->u[j * c->nx + i];
            mean.u[j] += u / c->nx;
            energy += u * u;
        }
        flow += c->u[j * c->nx] * dy * mean.width;
    }
    for (int j = 1; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++)
            energy += pow(c->v[j * c->nx + i], 2);
    mean.tau[0] = 2 * mean.mu * (mean.u[0] - mean.wall_bottom) / dy;
    for (int j = 1; j < c->ny; j++)
        mean.tau[j] = mean.mu * (mean.u[j] - mean.u[j - 1]) / dy;
    mean.tau[c->ny] = 2 * mean.mu * (mean.wall_top - mean.u[c->ny - 1]) / dy;
    cfd_channel_snapshot(&mean, out);
    text(out, "model", "incompressible_mac2d_v1");
    text(out, "model_limitations",
         "2D staggered laminar solver: periodic X, no-slip Y, invariant Z. No obstacles, open "
         "outlets, turbulence or adaptive grids. First-order conservative upwind momentum and "
         "explicit diffusion/projection.");
    json_object_object_add(out, "effective_grid", grid(c));
    number(out, "estimated_dense_bytes",
           sizeof(*c) + (6 * c->nx * c->ny + 2 * c->nx * (c->ny + 1)) * sizeof(double) + (c->solid ? c->nx*c->ny : 0));
    struct json_object *p = get(out, "physics");
    text(p, "pressure_semantics",
         "Solved non-incremental projection pressure in Pa, zero-mean periodic correction; total "
         "p=correction+G(L-x). Mean pressure drop G*L remains prescribed, not an open-outlet "
         "solve.");
    text(p, "spatial_discretization",
         "MAC: cell-centred pressure, X-face u, Y-face v; periodic X faces shared, no penetration "
         "at Y walls");
    text(p, "viscosity_operator",
         "explicit conservative Laplacian, half-cell tangential wall ghosts, CFL/diffusion-limited "
         "substeps");
    text(p, "velocity_transport",
         "conservative donor-cell momentum flux on staggered dual volumes; first-order numerical "
         "dissipation");
    text(p, "boundary_model",
         "periodic X velocity and pressure correction; prescribed mean pressure gradient; no-slip "
         "moving or stationary Y walls; invariant Z");
    json_object_object_add(p, "x_mean_profile_y_m_vx_m_s",
                           json_object_get(get(p, "profile_y_m_vx_m_s")));
    json_object_object_del(p, "profile_y_m_vx_m_s");
    json_object_object_add(p, "x_mean_face_y_m_tau_xy_pa",
                           json_object_get(get(p, "face_y_m_tau_xy_pa")));
    json_object_object_del(p, "face_y_m_tau_xy_pa");
    struct json_object *budget = json_object_new_object();
    double wall_force[2] = {0}, advective[2] = {0};
    for (int side = 0; side < 2; side++) {
        double normal[3] = {0, side ? 1 : -1, 0};
        double velocity[3] = {side ? mean.wall_top : mean.wall_bottom, 0, 0};
        double gradient[3][3] = {{0}};
        gradient[0][1] = mean.tau[side ? c->ny : 0] / mean.mu;
        CfdSurfaceFlux face;
        if (cfd_surface_flux(mean.rho, mean.mu, 0, mean.length * mean.width, normal, velocity,
                             velocity, (const double (*)[3])gradient, &face))
            wall_force[side] = face.viscous_on_fluid_n[0];
    }
    for (int j = 0; j < c->ny; j++) {
        double u = c->u[j * c->nx];
        advective[0] -= mean.rho * u * u * dy * mean.width;
        advective[1] += mean.rho * u * u * dy * mean.width;
    }
    number(budget, "bottom_fluid_on_wall_shear_force_x_n", -wall_force[0]);
    number(budget, "top_fluid_on_wall_shear_force_x_n", -wall_force[1]);
    number(budget, "mean_pressure_drive_force_x_n",
           mean.gradient * mean.length * mean.height * mean.width);
    number(budget, "instantaneous_drive_plus_wall_force_x_n",
           mean.gradient * mean.length * mean.height * mean.width + wall_force[0] + wall_force[1]);
    json_object_object_add(budget, "periodic_cut_outward_advective_momentum_x_n",
                           array(advective, 2));
    text(budget, "semantics",
         "Current-state wall shear reactions and equivalent mean pressure drive; instantaneous "
         "force sum is NOT the substep-integrated transient momentum residual. Periodic cut fluxes "
         "cancel by construction, not outlet validation.");
    text(budget, "wall_shear_status",
         "channel_verified; arbitrary obstacle traction not qualified");
    text(budget, "obstacle_status",
         "unsupported; no obstacle mask or surface quadrature in MAC model");
    text(budget, "outlet_status", "unsupported; periodic X is not an open outlet");
    text(budget, "drag_status", "not_qualified; no body pressure or viscous drag coefficient");
    if(c->solid) {
        text(out,"model_limitations","Exploratory 2D periodic stationary rectangle; no open outlets or qualified physical drag. Stair-step dual volumes and corner traction errors remain.");
        text(p,"pressure_semantics","Periodic correction in Pa for body-forced flow; no affine mean pressure added. Pressure surface reconstruction differs from discrete impulse reaction.");
        text(p,"boundary_model","Periodic X, no-slip Y and stationary grid-aligned solid rectangle");
        json_object_object_del(p,"pressure_drop_pa");
        number(budget,"mean_pressure_drive_force_x_n",c->mean_drive_force_x_n);
        number(budget,"continuum_body_drive_force_x_n",c->continuum_drive_force_x_n);
        number(budget,"unresolved_drive_force_x_n",c->unresolved_drive_force_x_n);
        number(budget,"obstacle_discrete_pressure_force_x_n",c->obstacle_mean_pressure_force_x_n);
        number(budget,"obstacle_discrete_viscous_force_x_n",c->obstacle_viscous_force_x_n);
        if(c->surface_pressure_valid)number(budget,"obstacle_reconstructed_pressure_force_x_n",c->obstacle_mean_surface_pressure_force_x_n);
        else json_object_object_add(budget,"obstacle_reconstructed_pressure_force_x_n",NULL);
        json_object_object_del(budget,"instantaneous_drive_plus_wall_force_x_n");
        text(budget,"semantics","Substep-averaged discrete reactions and separately reconstructed pressure; neither is a qualified drag coefficient. Periodic body forcing, not a physical pressure outlet.");
        text(budget,"obstacle_status","exploratory; full timesteps verified, surface/control-volume force accuracy gate failing");
        text(budget,"wall_shear_status","outer channel walls calibrated; obstacle surface shear/corner accuracy not qualified");
        text(budget,"drag_status","not_qualified; fixed-case force consistency is separate from reference drag validation for this run");
    }
    json_object_object_add(out, "boundary_force_budget", budget);
    struct json_object *h = json_object_new_object();
    number(h, "volume_flux_m3_s", flow);
    number(h, "mass_flux_kg_s", mean.rho * flow);
    double flux[6] = {-flow, flow, 0, 0, 0, 0};
    json_object_object_add(h, "outward_face_volume_flux_m3_s", array(flux, 6));
    number(h, "max_divergence_s_inv", cfd_mac2d_divergence(c));
    number(h, "max_substep_divergence_before_s_inv", c->divergence_before);
    number(h, "max_substep_divergence_after_s_inv", c->divergence_after);
    number(h, "true_pressure_residual_s_inv", c->pressure_residual);
    number(h, "pressure_iterations", c->pressure_iterations);
    number(h, "substeps", c->substeps);
    text(h, "projection_status", c->time > 0 ? "converged" : "not_started");
    number(h, "streamwise_momentum_balance_residual_n", c->momentum_residual);
    text(h, "momentum_balance_scope",
         "full tick: momentum change minus integrated mean-pressure force and explicit wall "
         "stresses; periodic convective and pressure fluxes cancel");
    number(h, "kinetic_energy_j", .5 * mean.rho * dx * dy * mean.width * energy);
    number(h, "projection_energy_change_j", c->projection_energy_change);
    number(h, "max_predictor_acceleration_m_s2", c->max_acceleration);
    text(h, "steady_status", "not_certified; requires time-window and refinement evidence");
    json_object_object_add(out, "health", h);
    if (full) {
        struct json_object *f = json_object_new_object();
        struct json_object *mask=json_object_new_array();
        for(int k=0;k<c->nx*c->ny;k++)json_object_array_add(mask,json_object_new_int(c->solid?c->solid[k]:0));
        json_object_object_add(f,"solid_cells",mask);
        json_object_object_add(f, "u_x_faces_m_s", array(c->u, c->nx * c->ny));
        json_object_object_add(f, "v_y_faces_m_s", array(c->v, c->nx * (c->ny + 1)));
        json_object_object_add(f, "pressure_correction_pa", array(c->p, c->nx * c->ny));
        text(f, "layout",
             "row-major Y then X; u shape [ny,nx] unique periodic faces, v [ny+1,nx], p [ny,nx]");
        json_object_object_add(out, "staggered_fields", f);
    }
}
struct json_object *cfd_mac2d_sample(const CfdMac2D *c, struct json_object *req) {
    struct json_object *o = cfd_channel_sample(&c->parameters, req), *samples = get(o, "samples");
    int u = json_object_get_int(get(o, "u_axis")), v = json_object_get_int(get(o, "v_axis")),
        n = json_object_get_int(get(o, "normal_axis"));
    int width = json_object_get_int(get(o, "width")),
        height = json_object_get_int(get(o, "height"));
    double dims[3] = {c->parameters.length, c->parameters.height, c->parameters.width};
    int count = req ? 11 : 3;
    double low[11], high[11], sum[11] = {0};
    for (int k = 0; k < count; k++) {
        low[k] = INFINITY;
        high[k] = -INFINITY;
    }
    for (int j = 0; j < height; j++)
        for (int i = 0; i < width; i++) {
            double p[3] = {0}, values[11];
            p[n] = json_object_get_double(get(o, "slice_world_m"));
            p[u] = (i + .5) / width * dims[u];
            p[v] = (j + .5) / height * dims[v];
            cfd_mac2d_values(c, p[0], p[1], values);
            struct json_object *cell = array(values, count);
            if (req)
                json_object_array_put_idx(cell, 6, NULL);
            json_object_array_put_idx(samples, j * width + i, cell);
            for (int k = 0; k < count; k++) {
                low[k] = fmin(low[k], values[k]);
                high[k] = fmax(high[k], values[k]);
                sum[k] += values[k];
            }
        }
    struct json_object *stats = get(o, "statistics"), *fields = get(o, "fields");
    for (int k = 0; k < count; k++)
        if (k != 6) {
            struct json_object *s =
                get(stats, json_object_get_string(json_object_array_get_idx(fields, k)));
            number(s, "min", low[k]);
            number(s, "max", high[k]);
            number(s, "mean", sum[k] / (width * height));
        }
    struct json_object *probes = get(o, "probes");
    for (size_t i = 0; i < json_object_array_length(probes); i++) {
        struct json_object *probe = json_object_array_get_idx(probes, i);
        if (!json_object_get_boolean(get(probe, "inside")))
            continue;
        struct json_object *p = get(probe, "requested_world_m");
        double values[11];
        cfd_mac2d_values(c, json_object_get_double(json_object_array_get_idx(p, 0)),
                         json_object_get_double(json_object_array_get_idx(p, 1)), values);
        struct json_object *cell = array(values, 11);
        json_object_array_put_idx(cell, 6, NULL);
        json_object_object_add(probe, "values", cell);
    }
    number(o, "speed_max", high[0]);
    json_object_object_add(o, "grid", grid(c));
    text(o, "sampling",
         "nearest pressure cell with averaged staggered velocities; exact wall velocity at Y "
         "boundaries; cell pressure correction plus imposed affine pressure; derivative samples "
         "diagnostic only");
    return o;
}
