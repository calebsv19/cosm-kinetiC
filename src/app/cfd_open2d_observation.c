#include "app/cfd_open2d_observation.h"
#include "app/cfd_channel_observation.h"
#include <math.h>
#include <string.h>
static void number(struct json_object *o, const char *k, double x) {
    json_object_object_add(o, k, json_object_new_double(x));
}
static void text(struct json_object *o, const char *k, const char *x) {
    json_object_object_add(o, k, json_object_new_string(x));
}
static struct json_object *get(struct json_object *o, const char *k) {
    struct json_object *x = NULL;
    json_object_object_get_ex(o, k, &x);
    return x;
}
static struct json_object *array(const double *v, int n) {
    struct json_object *a = json_object_new_array();
    for (int i = 0; i < n; i++)
        json_object_array_add(a, json_object_new_double(v[i]));
    return a;
}
static struct json_object *grid(const CfdOpen2D *c) {
    struct json_object *a = json_object_new_array();
    json_object_array_add(a, json_object_new_int(c->nx));
    json_object_array_add(a, json_object_new_int(c->ny));
    json_object_array_add(a, json_object_new_int(1));
    return a;
}
static double uc(const CfdOpen2D *c, int i, int j) {
    i = i < 0 ? 0 : i >= c->nx ? c->nx - 1 : i;
    j = j < 0 ? 0 : j >= c->ny ? c->ny - 1 : j;
    return .5 * (c->u[j * (c->nx + 1) + i] + c->u[j * (c->nx + 1) + i + 1]);
}
static double vc(const CfdOpen2D *c, int i, int j) {
    i = i < 0 ? 0 : i >= c->nx ? c->nx - 1 : i;
    j = j < 0 ? 0 : j >= c->ny ? c->ny - 1 : j;
    return .5 * (c->v[j * c->nx + i] + c->v[(j + 1) * c->nx + i]);
}
static void open_values(const CfdOpen2D *c, double x, double y, double out[11]) {
    memset(out, 0, 11 * sizeof(double));
    int i = (int)floor(x * c->nx / c->length), j = (int)floor(y * c->ny / c->height);
    i = i < 0 ? 0 : i >= c->nx ? c->nx - 1 : i;
    j = j < 0 ? 0 : j >= c->ny ? c->ny - 1 : j;
    if (c->solid && c->solid[j * c->nx + i]) {
        out[2] = 1;
        return;
    }
    double dx = c->length / c->nx, dy = c->height / c->ny, u = uc(c, i, j), v = vc(c, i, j);
    if (y <= 0 || y >= c->height)
        u = v = 0;
    else if (x <= 0) {
        u = c->u[j * (c->nx + 1)];
        v = 0;
    } else if (x >= c->length)
        u = c->u[j * (c->nx + 1) + c->nx];
    double uy = (uc(c, i, j + 1) - uc(c, i, j - 1)) / (dy * (j > 0 && j + 1 < c->ny ? 2 : 1));
    double vx = (vc(c, i + 1, j) - vc(c, i - 1, j)) / (dx * (i > 0 && i + 1 < c->nx ? 2 : 1));
    out[0] = hypot(u, v);
    out[3] = u;
    out[4] = v;
    out[7] = (c->u[j * (c->nx + 1) + i + 1] - c->u[j * (c->nx + 1) + i]) / dx +
             (c->v[(j + 1) * c->nx + i] - c->v[j * c->nx + i]) / dy;
    out[8] = fabs(vx - uy);
    out[9] = x >= c->length ? 0 : c->p[j * c->nx + i];
    out[10] = c->mu * (uy + vx);
}
bool cfd_open2d_body_check(const CfdOpen2D *c, CfdMac2DForceCheck *out) {
    if (!c->solid)
        return false;
    int x0=c->obstacle_x0,y0=c->obstacle_y0,x1=c->obstacle_x1,y1=c->obstacle_y1;
    if(x1<=x0||y1<=y0)return false;
    return cfd_open2d_force_check(c, x0 - 2, y0 - 2, x1 + 2, y1 + 2, out);
}
void cfd_open2d_snapshot(const CfdOpen2D *c, struct json_object *out, bool full, bool rate_valid,
                         double rate) {
    text(out, "model", "incompressible_open2d_v1");
    text(out, "model_limitations",
         "2D incompressible laminar flow, stationary rectangle only. Case-specific verification is "
         "not arbitrary-scene drag certification; no turbulence, moving bodies or 3D CFD.");
    json_object_object_add(out, "effective_grid", grid(c));
    number(out, "voxel_size_m", c->height / c->ny);
    double spacing[] = {c->length / c->nx, c->height / c->ny, c->width};
    json_object_object_add(out, "cell_spacing_m", array(spacing, 3));
    json_object_object_add(out, "geometry", json_object_new_array());
    number(out, "estimated_dense_bytes",
           sizeof(*c) +
               (2 * (c->nx + 1) * c->ny + 2 * c->nx * (c->ny + 1) + 5 * c->nx * c->ny) *
                   sizeof(double) +
               (c->solid ? c->nx * c->ny : 0)+cfd_pressure_mg_storage_bytes(c->nx,c->ny));
    struct json_object *p = json_object_new_object(), *h = json_object_new_object(),
                       *f = json_object_new_object(), *q = json_object_new_object();
    double dims[] = {c->length, c->height, c->width};
    json_object_object_add(p, "dimensions_m", array(dims, 3));
    json_object_object_add(p, "stationary_obstacle", json_object_new_boolean(c->solid != NULL));
    number(p, "density_kg_m3", c->rho);
    number(p, "dynamic_viscosity_pa_s", c->mu);
    number(p, "kinematic_viscosity_m2_s", c->mu / c->rho);
    number(p, "mean_inlet_speed_m_s", c->inlet_mean);
    number(p, "height_reynolds", c->rho * c->height * c->inlet_mean / c->mu);
    text(p, "pressure_semantics",
         "Solved pressure in Pa, outlet datum zero; no periodic wrap or imposed affine pressure.");
    text(p, "boundary_model",
         "Parabolic velocity inlet, half-dual-volume momentum pressure outlet, stationary no-slip "
         "walls and optional rectangle.");
    text(p, "velocity_transport",
         "Conservative MC-limited face reconstruction; explicit viscosity and projection; unsafe "
         "timestep rejected.");
    text(p, "initial_condition",
         c->solid ? "Channel parabola clipped at solid faces; first projection resolves initial "
                    "divergence"
                  : "Supplied parabolic channel profile");
    double inlet = 0, outlet = 0, backflow = 0, divergence = 0;
    double dx = c->length / c->nx, dy = c->height / c->ny;
    for (int j = 0; j < c->ny; j++) {
        double uout = c->u[j * (c->nx + 1) + c->nx];
        inlet += c->u[j * (c->nx + 1)] * dy * c->width;
        outlet += uout * dy * c->width;
        backflow += fmax(-uout, 0) * dy * c->width;
        for (int i = 0; i < c->nx; i++) {
            if (c->solid && c->solid[j * c->nx + i])
                continue;
            double div = (c->u[j * (c->nx + 1) + i + 1] - c->u[j * (c->nx + 1) + i]) / dx +
                         (c->v[(j + 1) * c->nx + i] - c->v[j * c->nx + i]) / dy;
            divergence = fmax(divergence, fabs(div));
        }
    }
    number(h, "volume_flux_m3_s", inlet);
    number(h, "outlet_volume_flux_m3_s", outlet);
    number(h, "mass_balance_residual_kg_s", c->rho * (outlet - inlet));
    number(h, "outlet_backflow_m3_s", backflow);
    number(h, "max_divergence_s_inv", divergence);
    number(h, "true_pressure_residual_s_inv", c->residual);
    number(h, "pressure_iterations", c->iterations);
    number(h,"pressure_solve_ms",c->pressure_ms);
    number(h,"pressure_workspace_bytes",cfd_pressure_mg_storage_bytes(c->nx,c->ny));
    number(h,"predictor_ms",c->predictor_ms);
    text(h,"pressure_preconditioner",c->pressure_mg?"symmetric_aggregation_multigrid":"not_initialized");
    text(h, "projection_status", c->time > 0 ? "converged" : "not_started");
    text(h, "steady_status", "not_certified; require time windows and matched refinement");
    double energy = 0;
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++)
            energy += .5 * c->rho * (pow(uc(c, i, j), 2) + pow(vc(c, i, j), 2)) * c->length /
                      c->nx * c->height / c->ny * c->width;
    number(h, "kinetic_energy_j", energy);
    number(p, "mean_inlet_pressure_pa", c->inlet_pressure);
    text(f, "drag_status",
         "not_qualified_for_this_run; reference case screening is separate from run acceptance");
    text(f, "force_semantics",
         "Instantaneous quadratic pressure/cubic wall shear; CV force retains one-tick momentum "
         "change. Individual components lack a 2 percent reference guarantee.");
    CfdMac2DForceCheck check;
    bool available = c->time > 0 && cfd_open2d_body_check(c, &check);
    json_object_object_add(f, "available", json_object_new_boolean(available));
    if (available) {
        number(f, "surface_pressure_force_x_n", check.surface_pressure_x_n);
        number(f, "surface_viscous_force_x_n", check.surface_viscous_x_n);
        number(f, "surface_total_force_x_n",
               check.surface_pressure_x_n + check.surface_viscous_x_n);
        if (rate_valid) {
            double cv =
                check.cv_pressure_x_n + check.cv_viscous_x_n - check.cv_advective_x_n - rate;
            number(f, "control_volume_force_x_n", cv);
            number(f, "momentum_change_rate_x_n", rate);
            number(f, "surface_minus_control_volume_x_n",
                   check.surface_pressure_x_n + check.surface_viscous_x_n - cv);
        }
    }
    text(q, "status", "case_verified_family; current_run_not_automatically_qualified");
    text(q, "evidence", "docs/cfd_baseline_goal_status.md; docs/cfd_independent_reference.md");
    text(q, "scope",
         "Exact channel; bounded Re20 pulse energy; confined rectangle total drag near Re=.01 "
         "within 2 percent of independently refined reference, with outlet sensitivity screen.");
    text(q, "component_limits",
         "Individual pressure/viscous components, arbitrary geometry and high-Re behavior remain "
         "unqualified. Masked energy has bounded stationary-rectangle evidence only.");
    json_object_object_add(out, "physics", p);
    json_object_object_add(out, "health", h);
    json_object_object_add(out, "boundary_force_budget", f);
    json_object_object_add(out, "qualification", q);
    if (full) {
        struct json_object *fields = json_object_new_object(), *mask = json_object_new_array();
        for (int k = 0; k < c->nx * c->ny; k++)
            json_object_array_add(mask, json_object_new_int(c->solid ? c->solid[k] : 0));
        json_object_object_add(fields, "solid_cells", mask);
        json_object_object_add(fields, "u_x_faces_m_s", array(c->u, (c->nx + 1) * c->ny));
        json_object_object_add(fields, "v_y_faces_m_s", array(c->v, c->nx * (c->ny + 1)));
        json_object_object_add(fields, "pressure_pa", array(c->p, c->nx * c->ny));
        text(fields, "layout",
             "row-major Y then X; u [ny,nx+1] includes distinct inlet/outlet, v [ny+1,nx], p "
             "[ny,nx]");
        json_object_object_add(out, "staggered_fields", fields);
    }
}
struct json_object *cfd_open2d_sample(const CfdOpen2D *c, struct json_object *req) {
    CfdChannel layout = {0};
    layout.length = c->length;
    layout.height = c->height;
    layout.width = c->width;
    layout.n = c->ny;
    layout.rho = c->rho;
    layout.mu = c->mu;
    struct json_object *o = cfd_channel_sample(&layout, req), *samples = get(o, "samples");
    int u = json_object_get_int(get(o, "u_axis")), v = json_object_get_int(get(o, "v_axis")),
        n = json_object_get_int(get(o, "normal_axis"));
    int width = json_object_get_int(get(o, "width")),
        height = json_object_get_int(get(o, "height"));
    double dims[3] = {c->length, c->height, c->width};
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
            open_values(c, p[0], p[1], values);
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
        open_values(c, json_object_get_double(json_object_array_get_idx(p, 0)),
                    json_object_get_double(json_object_array_get_idx(p, 1)), values);
        struct json_object *cell = array(values, 11);
        json_object_array_put_idx(cell, 6, NULL);
        json_object_object_add(probe, "values", cell);
    }
    number(o, "speed_max", high[0]);
    json_object_object_add(o, "grid", grid(c));
    text(o, "sampling",
         "nearest pressure cell with averaged staggered velocities; exact wall velocity at Y "
         "boundaries; physical pressure with fixed outlet datum; derivative samples "
         "diagnostic only");
    return o;
}
