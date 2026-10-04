#include "app/sim_runtime_3d_solver.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Qualification-only collocated projection. D uses the existing centered,
 * clamped-domain divergence. Its transpose is assembled from those exact
 * coefficients, including boundary duplicates. F removes solid cells and
 * components adjacent to solid walls, plus prescribed inlet components. After
 * setting constrained values in u*, solve (D F D^T) lambda = D u*, then
 * u <- u* - F D^T lambda. This is not a MAC flux discretization or a
 * calibrated physical-pressure outlet boundary condition. */
static void divergence(const SimRuntime3DDomainDesc *d, const uint8_t *solid,
                       const double *u, double *out) {
    size_t n = d->cell_count;
    double scale = 0.5 / d->voxel_size;
    for (int z = 0; z < d->grid_d; ++z)
    for (int y = 0; y < d->grid_h; ++y)
    for (int x = 0; x < d->grid_w; ++x) {
        size_t i = sim_runtime_3d_volume_index(d, x, y, z);
        out[i] = 0;
        if (solid && solid[i]) continue;
        int c[3] = {x, y, z};
        for (int axis = 0; axis < 3; ++axis) {
            c[axis]++;
            size_t hi = sim_runtime_3d_volume_index_clamped(d, c[0], c[1], c[2]);
            c[axis] -= 2;
            size_t lo = sim_runtime_3d_volume_index_clamped(d, c[0], c[1], c[2]);
            c[axis]++;
            out[i] += (u[axis*n + hi] - u[axis*n + lo]) * scale;
        }
    }
}

static void transpose(const SimRuntime3DDomainDesc *d, const uint8_t *solid,
                      const uint8_t *free_component, const double *p, double *u) {
    size_t n = d->cell_count;
    double scale = 0.5 / d->voxel_size;
    memset(u, 0, 3*n*sizeof(*u));
    for (int z = 0; z < d->grid_d; ++z)
    for (int y = 0; y < d->grid_h; ++y)
    for (int x = 0; x < d->grid_w; ++x) {
        size_t i = sim_runtime_3d_volume_index(d, x, y, z);
        if (solid && solid[i]) continue;
        int c[3] = {x, y, z};
        for (int axis = 0; axis < 3; ++axis) {
            c[axis]++;
            size_t hi = axis*n + sim_runtime_3d_volume_index_clamped(d, c[0], c[1], c[2]);
            c[axis] -= 2;
            size_t lo = axis*n + sim_runtime_3d_volume_index_clamped(d, c[0], c[1], c[2]);
            c[axis]++;
            if (free_component[hi]) u[hi] += p[i]*scale;
            if (free_component[lo]) u[lo] -= p[i]*scale;
        }
    }
}

bool sim_runtime_3d_project_boundary(SimRuntime3DVolume *v,
    const uint8_t *solid, int iterations, const SimRuntime3DProjectionBoundary *boundary,
    SimRuntime3DSolverStepMetrics *metrics) {
    if (!v || !metrics || !(v->desc.voxel_size > 0) || !isfinite(v->desc.voxel_size) || iterations < 1) return false;
    if (boundary && boundary->prescribed_inlet &&
        (boundary->inlet_axis < 0 || boundary->inlet_axis > 2 ||
         !isfinite(boundary->inflow_speed) || boundary->inflow_speed < 0)) return false;
    const SimRuntime3DDomainDesc *d = &v->desc;
    size_t n = d->cell_count;
    if (!n || n > SIZE_MAX / (7*sizeof(double))) return false;
    for (size_t i = 0; i < n; ++i)
        if (!isfinite(v->velocity_x[i]) || !isfinite(v->velocity_y[i]) ||
            !isfinite(v->velocity_z[i])) return false;
    double *storage = calloc(7*n, sizeof(double));
    uint8_t *free_component = calloc(3*n, 1);
    if (!storage || !free_component) { free(storage); free(free_component); return false; }
    double *u = storage, *r = u+3*n, *direction = r+n, *ap = direction+n, *p = ap+n;
    float *velocity[3] = {v->velocity_x, v->velocity_y, v->velocity_z};
    for (int z = 0; z < d->grid_d; ++z)
    for (int y = 0; y < d->grid_h; ++y)
    for (int x = 0; x < d->grid_w; ++x) {
        size_t i = sim_runtime_3d_volume_index(d, x, y, z);
        int c[3] = {x, y, z};
        for (int axis = 0; axis < 3; ++axis) {
            c[axis]++;
            size_t hi = sim_runtime_3d_volume_index_clamped(d, c[0], c[1], c[2]);
            c[axis] -= 2;
            size_t lo = sim_runtime_3d_volume_index_clamped(d, c[0], c[1], c[2]);
            c[axis]++;
            bool allowed = !solid || (!solid[i] && !solid[hi] && !solid[lo]);
            free_component[axis*n+i] = allowed;
            if (!allowed) velocity[axis][i] = 0;
            if (boundary && boundary->prescribed_inlet) {
                int extent[3] = {d->grid_w, d->grid_h, d->grid_d};
                int inlet = boundary->inlet_at_max ? extent[boundary->inlet_axis]-1 : 0;
                if (c[boundary->inlet_axis] == inlet) {
                    /* Prescribed values form an affine constraint. Wall constraints
                     * take precedence at solid cells and solid-adjacent components. */
                    velocity[axis][i] = allowed && axis == boundary->inlet_axis
                        ? (boundary->inlet_at_max ? -1 : 1)*boundary->inflow_speed : 0;
                    free_component[axis*n+i] = 0;
                }
            }
            u[axis*n+i] = velocity[axis][i];
        }
    }
    divergence(d, solid, u, r);
    double rr = 0, initial_max = 0;
    for (size_t i = 0; i < n; ++i) {
        direction[i] = r[i]; rr += r[i]*r[i];
        initial_max = fmax(initial_max, fabs(r[i]));
    }
    metrics->projection_iterations_used = 0;
    metrics->projection_converged = false;
    metrics->max_abs_divergence_before_project = (float)initial_max;
    double tolerance = fmax(1e-7, initial_max*1e-6);
    for (int k = 0; k < iterations && rr > 0; ++k) {
        transpose(d, solid, free_component, direction, u);
        divergence(d, solid, u, ap);
        double pap = 0;
        for (size_t i = 0; i < n; ++i) pap += direction[i]*ap[i];
        if (!(pap > 0) || !isfinite(pap)) break;
        metrics->projection_iterations_used = k+1;
        double alpha = rr/pap, next_rr = 0, residual_max = 0;
        for (size_t i = 0; i < n; ++i) {
            p[i] += alpha*direction[i]; r[i] -= alpha*ap[i];
            next_rr += r[i]*r[i]; residual_max = fmax(residual_max, fabs(r[i]));
        }
        if (residual_max <= tolerance) break;
        double beta = next_rr/rr;
        for (size_t i = 0; i < n; ++i) direction[i] = r[i] + beta*direction[i];
        rr = next_rr;
    }
    transpose(d, solid, free_component, p, u);
    /* Recompute the true residual, rather than trust the recursive CG value. */
    divergence(d, solid, u, ap);
    for (size_t i = 0; i < 3*n; ++i) u[i] = velocity[i/n][i%n];
    divergence(d, solid, u, r);
    double residual_max = 0;
    for (size_t i = 0; i < n; ++i) residual_max = fmax(residual_max, fabs(r[i]-ap[i]));
    metrics->pressure_residual_linf = (float)residual_max;
    metrics->projection_converged = residual_max <= tolerance;
    transpose(d, solid, free_component, p, u);
    for (size_t i = 0; i < 3*n; ++i) {
        velocity[i/n][i%n] -= (float)u[i];
        u[i] = velocity[i/n][i%n];
    }
    divergence(d, solid, u, r);
    double final_max = 0;
    for (size_t i = 0; i < n; ++i) {
        final_max = fmax(final_max, fabs(r[i]));
        v->pressure[i] = (float)-p[i]; /* potential, not physical pressure */
    }
    metrics->max_abs_divergence_after_project = (float)final_max;
    free(storage); free(free_component);
    return isfinite(final_max) && isfinite(residual_max);
}

bool sim_runtime_3d_project_compatible(SimRuntime3DVolume *v,
    const uint8_t *solid, int iterations, SimRuntime3DSolverStepMetrics *metrics) {
    return sim_runtime_3d_project_boundary(v, solid, iterations, NULL, metrics);
}

/* Finite-volume diagnostic only: arithmetic interior face velocity, zero
 * solid-face flux, and boundary-cell velocity on exterior faces. This reads
 * the final snapshot; it does not change the solver's transport scheme. */
bool sim_runtime_3d_measure_conservation(const SimRuntime3DDomainDesc *d,
    const float *vx, const float *vy, const float *vz, const uint8_t *solid,
    SimRuntime3DConservation *out) {
    if (!out) return false;
    *out = (SimRuntime3DConservation){0};
    if (!d || !vx || !vy || !vz || !(d->voxel_size > 0) || !d->cell_count) return false;
    SimRuntime3DConservation result = {0};
    const float *v[3] = {vx, vy, vz};
    int extent[3] = {d->grid_w, d->grid_h, d->grid_d};
    double h = d->voxel_size, area = h*h, cell_volume = area*h;
    for (int z = 0; z < d->grid_d; ++z)
    for (int y = 0; y < d->grid_h; ++y)
    for (int x = 0; x < d->grid_w; ++x) {
        size_t i = sim_runtime_3d_volume_index(d,x,y,z);
        if (solid && solid[i]) continue;
        double flux = 0;
        int c[3] = {x,y,z};
        for (int a = 0; a < 3; ++a) {
            if (!isfinite(v[a][i])) return false;
            result.kinetic_energy_per_density += 0.5*v[a][i]*v[a][i]*cell_volume;
            for (int sign = -1; sign <= 1; sign += 2) {
                c[a] += sign;
                bool exterior = c[a] < 0 || c[a] >= extent[a];
                double face_velocity = v[a][i];
                if (!exterior) {
                    size_t j = sim_runtime_3d_volume_index(d,c[0],c[1],c[2]);
                    face_velocity = solid && solid[j] ? 0 : 0.5*((double)v[a][i]+v[a][j]);
                }
                c[a] -= sign;
                double outward_flux = sign*face_velocity*area;
                flux += outward_flux;
                if (exterior) result.boundary_flux_m3_s[2*a+(sign>0)] += outward_flux;
            }
        }
        result.integrated_divergence_m3_s += flux;
        result.max_abs_divergence_s_inv = fmax(result.max_abs_divergence_s_inv, fabs(flux/cell_volume));
    }
    for (int f = 0; f < 6; ++f) result.net_outward_flux_m3_s += result.boundary_flux_m3_s[f];
    result.valid = isfinite(result.integrated_divergence_m3_s) && isfinite(result.kinetic_energy_per_density);
    *out = result;
    return result.valid;
}
