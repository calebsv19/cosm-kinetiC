#include "app/cfd_obstacle3d.h"
#include <math.h>
#include <string.h>
#include <time.h>
static double face(const CfdObstacle3d *s, const double *u, int a, const int c[3]) {
    int q = cfd_obstacle_mixed3d_index(s->mixed, a, c[0], c[1], c[2]);
    return q < 0 ? 0 : u[q];
}
static double cell_velocity(const CfdObstacle3d *s, const double *u, int a, const int c[3]) {
    int hi[3] = {c[0], c[1], c[2]};
    hi[a]++;
    return .5 * (face(s, u, a, c) + face(s, u, a, hi));
}
static double pressure(const CfdObstacle3d *s, const double *p, const int c[3]) {
    int q = cfd_obstacle_mixed3d_cell(s->mixed, c[0], c[1], c[2]);
    return q < 0 ? 0 : p[q];
}
/* Physical stress reconstructed independently of matrix rows. Each body/wall
 * patch lies on an actual geometric plane; stationary velocity trace is zero. */
static void patch_force_legacy(const CfdObstacle3d *s, const double *u, const double *p, int a,
                               int direction, const int fluid_cell[3], double fp[3], double fv[3]) {
    const CfdCartesian3d *g = &s->grid;
    int c2[3] = {fluid_cell[0], fluid_cell[1], fluid_cell[2]};
    c2[a] += direction;
    int c3[3] = {c2[0], c2[1], c2[2]};
    c3[a] += direction;
    bool third = cfd_obstacle_mixed3d_cell(s->mixed, c3[0], c3[1], c3[2]) >= 0;
    double pt =
        third
            ? (11 * pressure(s, p, fluid_cell) - 7 * pressure(s, p, c2) + 2 * pressure(s, p, c3)) /
                  6
            : 1.5 * pressure(s, p, fluid_cell) - .5 * pressure(s, p, c2);
    fp[a] -= pt * direction * g->area[a];
    for (int b = 0; b < 3; b++) {
        double derivative;
        if (a == b) {
            /* On a stationary flat no-slip face, tangential trace derivatives
             * vanish. Incompressibility fixes the normal derivative to zero;
             * extrapolating an interior normal face creates spurious stress. */
            derivative = 0;
        } else {
            /* Quadratic reconstruction of actual normal-interval averages,
             * with the exact zero trace; not point-value samples at h/2. */
            derivative = (7 * cell_velocity(s, u, b, fluid_cell) - cell_velocity(s, u, b, c2)) /
                         (2 * g->h[a]);
        }
        fv[b] += (a == b ? 2 : 1) * s->mu * derivative * g->area[a];
    }
}
static void patch_force(const CfdObstacle3d *s, const double *u, const double *p, int a,
                        int direction, const int fluid_cell[3], double fp[3], double fv[3],
                        double *legacy) {
    double old[3] = {0};
    patch_force_legacy(s, u, p, a, direction, fluid_cell, fp, old);
    if (legacy)
        for (int b = 0; b < 3; b++)
            legacy[b] += old[b];
    cfd_obstacle3d_trace_viscous(s, u, a, direction, fluid_cell, fv);
}
static double gradient(const CfdObstacle3d *s, const double *u, int a, int b, const int c[3]) {
    const CfdCartesian3d *g = &s->grid;
    if (a == b) {
        int hi[3] = {c[0], c[1], c[2]};
        hi[a]++;
        return (face(s, u, a, hi) - face(s, u, a, c)) / g->h[a];
    }
    int lo[3] = {c[0], c[1], c[2]}, hi[3] = {c[0], c[1], c[2]};
    lo[b]--;
    hi[b]++;
    bool left = cfd_obstacle_mixed3d_cell(s->mixed, lo[0], lo[1], lo[2]) >= 0;
    bool right = cfd_obstacle_mixed3d_cell(s->mixed, hi[0], hi[1], hi[2]) >= 0;
    double v = cell_velocity(s, u, a, c);
    if (left && right) {
        int lo2[3] = {c[0], c[1], c[2]}, hi2[3] = {c[0], c[1], c[2]};
        lo2[b] -= 2;
        hi2[b] += 2;
        if (cfd_obstacle_mixed3d_cell(s->mixed, lo2[0], lo2[1], lo2[2]) >= 0 &&
            cfd_obstacle_mixed3d_cell(s->mixed, hi2[0], hi2[1], hi2[2]) >= 0)
            return (cell_velocity(s, u, a, lo2) - 8 * cell_velocity(s, u, a, lo) +
                    8 * cell_velocity(s, u, a, hi) - cell_velocity(s, u, a, hi2)) /
                   (12 * g->h[b]);
        return (cell_velocity(s, u, a, hi) - cell_velocity(s, u, a, lo)) / (2 * g->h[b]);
    }
    if (b == 0 && (c[0] == 0 || c[0] == g->n[0] - 1)) {
        int d = c[0] == 0 ? 1 : -1, t[3] = {c[0], c[1], c[2]}, t2[3] = {c[0], c[1], c[2]};
        t[b] += d;
        t2[b] += 2 * d;
        return d * (-3 * v + 4 * cell_velocity(s, u, a, t) - cell_velocity(s, u, a, t2)) /
               (2 * g->h[b]);
    }
    if (left || right) {
        int d = right ? 1 : -1, t[3] = {c[0], c[1], c[2]}, t2[3] = {c[0], c[1], c[2]},
            t3[3] = {c[0], c[1], c[2]};
        t[b] += d;
        t2[b] += 2 * d;
        t3[b] += 3 * d;
        double v1 = cell_velocity(s, u, a, t);
        if (cfd_obstacle_mixed3d_cell(s->mixed, t2[0], t2[1], t2[2]) < 0)
            return d * (v1 - v) / g->h[b];
        double v2 = cell_velocity(s, u, a, t2);
        if (cfd_obstacle_mixed3d_cell(s->mixed, t3[0], t3[1], t3[2]) < 0)
            return d * (-3 * v + 4 * v1 - v2) / (2 * g->h[b]);
        return d * (-11 * v + 18 * v1 - 9 * v2 + 2 * cell_velocity(s, u, a, t3)) / (6 * g->h[b]);
    }
    return 0;
}
void cfd_obstacle3d_measure(CfdObstacle3d *s, const double *u, const double *p) {
    const CfdCartesian3d *g = &s->grid;
    memset(s->pressure_force, 0, sizeof(s->pressure_force));
    memset(s->viscous_force, 0, sizeof(s->viscous_force));
    memset(s->legacy_viscous_force, 0, sizeof(s->legacy_viscous_force));
    memset(s->side_pressure, 0, sizeof(s->side_pressure));
    memset(s->side_viscous, 0, sizeof(s->side_viscous));
    memset(s->wall_force, 0, sizeof(s->wall_force));
    s->physical_dissipation = 0;
    s->max_divergence = 0;
    s->max_speed = 0;
    double flux_min = INFINITY, flux_max = -INFINITY;
    for (int a = 0; a < 3; a++)
        for (int side = 0; side < 2; side++) {
            int direction = side ? 1 : -1, plane = side ? s->hi[a] : s->lo[a] - 1;
            for (int k = s->lo[2]; k < s->hi[2]; k++)
                for (int j = s->lo[1]; j < s->hi[1]; j++)
                    for (int i = s->lo[0]; i < s->hi[0]; i++) {
                        int c[3] = {i, j, k};
                        if (c[a] != s->lo[a])
                            continue;
                        c[a] = plane;
                        patch_force(s, u, p, a, direction, c, s->side_pressure[2 * a + side],
                                    s->side_viscous[2 * a + side], s->legacy_viscous_force);
                    }
            for (int b = 0; b < 3; b++) {
                s->pressure_force[b] += s->side_pressure[2 * a + side][b];
                s->viscous_force[b] += s->side_viscous[2 * a + side][b];
            }
        }
    for (int i = 0; i <= g->n[0]; i++) {
        double flow = 0;
        for (int k = 0; k < g->n[2]; k++)
            for (int j = 0; j < g->n[1]; j++) {
                int c[3] = {i, j, k};
                flow += face(s, u, 0, c) * g->area[0];
            }
        flux_min = fmin(flux_min, flow);
        flux_max = fmax(flux_max, flow);
        if (i == 0)
            s->inlet_flow = flow;
        if (i == g->n[0])
            s->outlet_flow = flow;
    }
    s->flux_error = (flux_max - flux_min) / s->requested_flow;
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int c[3] = {i, j, k};
                if (cfd_obstacle_mixed3d_cell(s->mixed, i, j, k) < 0)
                    continue;
                double grad[3][3], div = 0, speed = 0;
                for (int a = 0; a < 3; a++) {
                    double v = cell_velocity(s, u, a, c);
                    speed += v * v;
                    for (int b = 0; b < 3; b++)
                        grad[a][b] = gradient(s, u, a, b, c);
                    div += grad[a][a];
                }
                s->max_speed = fmax(s->max_speed, sqrt(speed));
                s->max_divergence = fmax(s->max_divergence, fabs(div));
                for (int a = 0; a < 3; a++)
                    for (int b = 0; b < 3; b++) {
                        double eps = .5 * (grad[a][b] + grad[b][a]);
                        s->physical_dissipation += 2 * s->mu * eps * eps * g->volume;
                    }
                for (int a = 1; a < 3; a++)
                    for (int side = 0; side < 2; side++)
                        if (c[a] == (side ? g->n[a] - 1 : 0)) {
                            double fp[3] = {0}, fv[3] = {0};
                            patch_force(s, u, p, a, side ? -1 : 1, c, fp, fv, NULL);
                            for (int b = 0; b < 3; b++)
                                s->wall_force[2 * (a - 1) + side][b] += fp[b] + fv[b];
                        }
            }
    s->legacy_physical_dissipation = s->physical_dissipation;
    cfd_obstacle3d_reconstruct_strain(s, u);
    double end_force[3] = {4 * s->inlet_pressure, 0, 0}, extra_power = 0;
    for (int side = 0; side < 2; side++)
        for (int k = 0; k < g->n[2]; k++)
            for (int j = 0; j < g->n[1]; j++) {
                int c[3] = {side ? g->n[0] : 0, j, k}, ci[3] = {side ? g->n[0] - 1 : 0, j, k};
                int d = side ? -1 : 1, t[3] = {c[0] + d, j, k}, t2[3] = {c[0] + 2 * d, j, k};
                double deriv[3];
                deriv[0] = d * (-3 * face(s, u, 0, c) + 4 * face(s, u, 0, t) - face(s, u, 0, t2)) /
                           (2 * g->h[0]);
                for (int a = 1; a < 3; a++) {
                    int cm[3] = {c[0], j, k}, cp[3] = {c[0], j, k};
                    cm[a]--;
                    cp[a]++;
                    double vm = cm[a] < 0 ? -face(s, u, 0, c) : face(s, u, 0, cm);
                    double vp = cp[a] >= g->n[a] ? -face(s, u, 0, c) : face(s, u, 0, cp);
                    deriv[a] = (vp - vm) / (2 * g->h[a]);
                }
                for (int a = 0; a < 3; a++) {
                    double v = a == 0 ? face(s, u, a, c) : cell_velocity(s, u, a, ci);
                    double traction = s->mu * (side ? 1 : -1) * deriv[a];
                    end_force[a] += traction * g->area[0];
                    extra_power += v * traction * g->area[0];
                }
            }
    s->natural_power = s->inlet_pressure * s->inlet_flow;
    s->physical_power = s->natural_power + extra_power;
    cfd_obstacle_mixed3d_reaction(s->mixed, u, p, s->discrete_pressure_force,
                                  s->discrete_viscous_force, s->discrete_walls);
    for (int a = 0; a < 3; a++) {
        double residual = (a == 0 ? 4 * s->inlet_pressure : 0) - s->discrete_pressure_force[a] -
                          s->discrete_viscous_force[a];
        for (int b = 0; b < 4; b++)
            residual -= s->discrete_walls[b][a];
        s->discrete_momentum_residual[a] = residual;
    }
    s->discrete_dissipation = cfd_obstacle_mixed3d_work(s->mixed, u);
    s->physical_energy_imbalance =
        fabs(s->physical_power - s->physical_dissipation) / s->physical_power;
    s->discrete_energy_imbalance =
        fabs(s->natural_power - s->discrete_dissipation) / s->natural_power;
    for (int a = 0; a < 3; a++) {
        double net = end_force[a] - s->pressure_force[a] - s->viscous_force[a];
        for (int wall = 0; wall < 4; wall++)
            net -= s->wall_force[wall][a];
        s->momentum_residual[a] = net;
    }
}
void cfd_obstacle3d_destroy(CfdObstacle3d *s) {
    if (!s)
        return;
    cfd_obstacle_mixed3d_destroy(s->mixed);
    s->mixed = NULL;
    cfd_memory_free(s->u);
    cfd_memory_free(s->p);
    cfd_memory_free(s->candidate_u);
    cfd_memory_free(s->candidate_p);
    cfd_memory_free(s->rhs);
    cfd_memory_free(s->reconstruction_planes);
    s->reconstruction_planes = NULL;
    s->u = s->p = s->candidate_u = s->candidate_p = s->rhs = NULL;
}
bool cfd_obstacle3d_init(CfdObstacle3d *s, const int n[3], const double length[3], double rho,
                         double mu, double flow, double center) {
    if (!s)
        return false;
    memset(s, 0, sizeof(*s));
    clock_t start = clock();
    if (!cfd_cartesian3d_init(&s->grid, n, length) || fabs(length[1] - 2) > 1e-12 ||
        fabs(length[2] - 2) > 1e-12 || !isfinite(center) || center <= .5 ||
        center >= length[0] - .5 || !isfinite(rho) || !isfinite(mu) || !isfinite(flow) ||
        rho <= 0 || mu <= 0 || flow <= 0 || rho * flow / (4 * mu) > .1)
        return false;
    s->rho = rho;
    s->mu = mu;
    s->requested_flow = flow;
    s->center_x = center;
    for (int a = 0; a < 3; a++) {
        double low = a == 0 ? center - .5 : .5, high = low + 1;
        double l = low / s->grid.h[a], h = high / s->grid.h[a];
        if (fabs(l - round(l)) > 1e-9 || fabs(h - round(h)) > 1e-9)
            return false;
        s->lo[a] = (int)round(l);
        s->hi[a] = (int)round(h);
        if (s->lo[a] < 2 || s->hi[a] > n[a] - 2)
            return false;
    }
    s->mixed = cfd_obstacle_mixed3d_create(&s->grid, mu, s->lo, s->hi);
    if (!s->mixed)
        return false;
    s->count = cfd_obstacle_mixed3d_count(s->mixed);
    s->cells = cfd_obstacle_mixed3d_cells(s->mixed);
    s->u = cfd_memory_calloc(s->count, sizeof(double));
    s->p = cfd_memory_calloc(s->cells, sizeof(double));
    s->candidate_u = cfd_memory_calloc(s->count, sizeof(double));
    s->candidate_p = cfd_memory_calloc(s->cells, sizeof(double));
    s->rhs = cfd_memory_calloc(s->count, sizeof(double));
    s->reconstruction_planes =
        cfd_memory_calloc((size_t)9 * (2 * n[0] + 1) * (2 * n[1] + 1), sizeof(double));
    s->setup_cpu_ms = 1000. * (clock() - start) / CLOCKS_PER_SEC;
    return s->u && s->p && s->candidate_u && s->candidate_p && s->rhs && s->reconstruction_planes;
}
bool cfd_obstacle3d_solve(CfdObstacle3d *s) {
    if (!s || !s->mixed || !s->u || !s->p || !s->candidate_u || !s->candidate_p || !s->rhs)
        return false;
    clock_t start = clock();
    s->error = NULL;
    if (!isfinite(s->requested_flow) || s->requested_flow <= 0) {
        s->error = "invalid_requested_flow";
        return false;
    }
    memset(s->candidate_u, 0, s->count * sizeof(double));
    memset(s->candidate_p, 0, s->cells * sizeof(double));
    memset(s->rhs, 0, s->count * sizeof(double));
    for (int k = 0; k < s->grid.n[2]; k++)
        for (int j = 0; j < s->grid.n[1]; j++) {
            int q = cfd_obstacle_mixed3d_index(s->mixed, 0, 0, j, k);
            s->rhs[q] = s->grid.area[0];
        }
    if (!cfd_obstacle_mixed3d_solve(s->mixed, s->rhs, s->candidate_u, s->candidate_p)) {
        s->error = cfd_obstacle_mixed3d_error(s->mixed);
        return false;
    }
    double flow = 0;
    for (int k = 0; k < s->grid.n[2]; k++)
        for (int j = 0; j < s->grid.n[1]; j++) {
            int q = cfd_obstacle_mixed3d_index(s->mixed, 0, s->grid.n[0], j, k);
            flow += s->candidate_u[q] * s->grid.area[0];
        }
    if (!isfinite(flow) || flow <= 0) {
        s->error = "invalid_positive_flow_response";
        return false;
    }
    double scale = s->requested_flow / flow;
    if (!isfinite(scale) || scale <= 0) {
        s->error = "invalid_flow_constraint_scale";
        return false;
    }
    for (int q = 0; q < s->count; q++)
        s->candidate_u[q] *= scale;
    for (int q = 0; q < s->cells; q++)
        s->candidate_p[q] *= scale;
    /* Unit-pressure residual acceptance precedes the flow constraint. Check the
     * actual SI candidate as well: absolute continuity can change under scaling.
     * RHS is idle here and has at least one entry per compact fluid cell. */
    for (int q = 0; q < s->count; q++)
        if (!isfinite(s->candidate_u[q])) {
            s->error = "scaled_velocity_nonfinite";
            return false;
        }
    for (int q = 0; q < s->cells; q++)
        if (!isfinite(s->candidate_p[q])) {
            s->error = "scaled_pressure_nonfinite";
            return false;
        }
    cfd_obstacle_mixed3d_divergence(s->mixed, s->candidate_u, s->rhs);
    for (int q = 0; q < s->cells; q++)
        if (!isfinite(s->rhs[q]) || fabs(s->rhs[q]) / s->grid.volume >= 1e-8) {
            s->error = "scaled_continuity_residual_failed";
            return false;
        }
    /* No observer callback from measurement: accepted publication stays coherent. */
    s->inlet_pressure = scale;
    cfd_obstacle3d_measure(s, s->candidate_u, s->candidate_p);
    memcpy(s->u, s->candidate_u, s->count * sizeof(double));
    memcpy(s->p, s->candidate_p, s->cells * sizeof(double));
    s->solved = true;
    s->relative_residual = cfd_obstacle_mixed3d_residual(s->mixed);
    s->iterations = cfd_obstacle_mixed3d_iterations(s->mixed);
    s->inner_iterations = cfd_obstacle_mixed3d_inner_iterations(s->mixed);
    s->solve_cpu_ms = 1000. * (clock() - start) / CLOCKS_PER_SEC;
    return true;
}
double cfd_obstacle3d_face(const CfdObstacle3d *s, int a, int i, int j, int k) {
    int c[3] = {i, j, k};
    return face(s, s->u, a, c);
}
double cfd_obstacle3d_pressure(const CfdObstacle3d *s, int i, int j, int k) {
    int c[3] = {i, j, k};
    return pressure(s, s->p, c);
}

void cfd_obstacle3d_derivatives(const CfdObstacle3d *s, int i, int j, int k, double d[3][3]) {
    int c[3] = {i, j, k};
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++)
            d[a][b] = gradient(s, s->u, a, b, c);
}
