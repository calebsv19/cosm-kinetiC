#include "app/cfd_startup3d.h"
#include "app/cfd_duct3d.h"
#include <math.h>
#include <string.h>
#include <time.h>
static double face(const CfdStartup3d *s, int a, int i, int j, int k) {
    int q = cfd_mixed3d_index(s->mixed, a, i, j, k);
    return q >= 0 ? s->velocity[q] : 0;
}
static double center(const CfdStartup3d *s, int a, int i, int j, int k) {
    int hi[3] = {i, j, k};
    hi[a]++;
    return .5 * (face(s, a, i, j, k) + face(s, a, hi[0], hi[1], hi[2]));
}
static double kinetic(const CfdStartup3d *s) {
    double e = 0;
    for (int q = 0; q < s->count; q++)
        e += s->velocity[q] * s->velocity[q] * cfd_mixed3d_volume(s->mixed, q);
    return .5 * s->rho * e;
}
void cfd_startup3d_destroy(CfdStartup3d *s) {
    if (!s)
        return;
    cfd_mixed3d_destroy(s->mixed);
    cfd_memory_free(s->storage);
    s->mixed = NULL;
    s->storage = NULL;
}
bool cfd_startup3d_init(CfdStartup3d *s, const int n[3], const double length[3], double rho,
                        double mu, double dt, double flow) {
    if (!s)
        return false;
    memset(s, 0, sizeof(*s));
    clock_t start = clock();
    if (!cfd_cartesian3d_init(&s->grid, n, length) || !isfinite(rho) || rho <= 0 || !isfinite(mu) ||
        mu <= 0 || !isfinite(dt) || dt <= 0 || dt > .1 || !isfinite(flow) || flow <= 0)
        return false;
    s->rho = rho;
    s->mu = mu;
    s->dt = dt;
    s->gradient =
        mu * flow / (length[1] * length[2] * cfd_duct3d_reference_mean(length[1], length[2], 1024));
    s->mixed = cfd_mixed3d_create(&s->grid, mu, rho / dt, false);
    if (!s->mixed)
        return false;
    s->count = cfd_mixed3d_count(s->mixed);
    int m = s->count, N = s->grid.count, yz = n[1] * n[2];
    s->storage = cfd_memory_calloc((size_t)5 * m + 2 * N + 2 * yz, sizeof(double));
    if (!s->storage) {
        cfd_startup3d_destroy(s);
        return false;
    }
    s->velocity = s->storage;
    s->previous = s->velocity + m;
    s->candidate = s->previous + m;
    s->rhs = s->candidate + m;
    s->pressure = s->velocity + 5 * m;
    s->candidate_pressure = s->pressure + N;
    s->steady_profile = s->candidate_pressure + N;
    const double pi = 3.14159265358979323846;
    for (int k = 0; k < n[2]; k++)
        for (int j = 0; j < n[1]; j++) {
            double y = (j + .5) * s->grid.h[1], z = (k + .5) * s->grid.h[2], sum = 0;
            for (int a = 0; a < 1024; a++) {
                double n = 2 * a + 1, ky = n * pi / length[1];
                double sy = sin(ky * y) * sin(ky * s->grid.h[1] / 2) / (ky * s->grid.h[1] / 2);
                double ratio =
                    (exp(-ky * (z - s->grid.h[2] / 2)) - exp(-ky * (z + s->grid.h[2] / 2)) +
                     exp(-ky * (length[2] - z - s->grid.h[2] / 2)) -
                     exp(-ky * (length[2] - z + s->grid.h[2] / 2))) /
                    (ky * s->grid.h[2] * (1 + exp(-ky * length[2])));
                sum += sy * (1 - ratio) / (n * n * n);
            }
            s->steady_profile[k * s->grid.n[1] + j] =
                s->gradient / s->mu * 4 * length[1] * length[1] / (pi * pi * pi) * sum;
        }
    s->setup_cpu_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    return true;
}
static void observe(CfdStartup3d *s) {
    const CfdCartesian3d *g = &s->grid;
    double *ref = s->steady_profile + g->n[1] * g->n[2];
    cfd_startup3d_reference(s, s->time, 128, ref);
    double error = 0, norm = 0, pin1 = 0, pin2 = 0, pout1 = 0, pout2 = 0;
    s->flow = s->dissipation = s->max_divergence = 0;
    memset(s->wall_force, 0, sizeof(s->wall_force));
    for (int q = 0; q < s->count; q++) {
        int a, c[3];
        double x[3];
        cfd_mixed3d_position(s->mixed, q, &a, x, c);
        double exact = a == 0 ? ref[c[2] * g->n[1] + c[1]] : 0, du = s->velocity[q] - exact,
               vol = cfd_mixed3d_volume(s->mixed, q);
        error += du * du * vol;
        norm += exact * exact * vol;
    }
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int c[3] = {i, j, k};
                double d[3][3], u = center(s, 0, i, j, k);
                for (int a = 0; a < 3; a++)
                    for (int b = 0; b < 3; b++) {
                        int hi[3] = {i, j, k}, lo[3] = {i, j, k};
                        hi[b]++;
                        lo[b]--;
                        if (a == b)
                            d[a][b] =
                                (face(s, a, hi[0], hi[1], hi[2]) - face(s, a, i, j, k)) / g->h[b];
                        else {
                            double uc = center(s, a, i, j, k),
                                   up = c[b] == g->n[b] - 1 ? (b == 0 ? uc : -uc)
                                                            : center(s, a, hi[0], hi[1], hi[2]),
                                   um = c[b] == 0 ? (b == 0 ? uc : -uc)
                                                  : center(s, a, lo[0], lo[1], lo[2]);
                            d[a][b] = (up - um) / (2 * g->h[b]);
                        }
                    }
                s->max_divergence = fmax(s->max_divergence, fabs(d[0][0] + d[1][1] + d[2][2]));
                for (int a = 0; a < 3; a++)
                    for (int b = 0; b < 3; b++) {
                        double strain = .5 * (d[a][b] + d[b][a]);
                        s->dissipation += 2 * s->mu * strain * strain * g->volume;
                    }
                if (j == 0)
                    s->wall_force[0] += 2 * s->mu * u / g->h[1] * g->area[1];
                if (j == g->n[1] - 1)
                    s->wall_force[1] += 2 * s->mu * u / g->h[1] * g->area[1];
                if (k == 0)
                    s->wall_force[2] += 2 * s->mu * u / g->h[2] * g->area[2];
                if (k == g->n[2] - 1)
                    s->wall_force[3] += 2 * s->mu * u / g->h[2] * g->area[2];
                int q = (k * g->n[1] + j) * g->n[0] + i;
                if (i == 0) {
                    pin1 += s->pressure[q];
                    s->flow += face(s, 0, 0, j, k) * g->area[0];
                }
                if (i == 1)
                    pin2 += s->pressure[q];
                if (i == g->n[0] - 1)
                    pout1 += s->pressure[q];
                if (i == g->n[0] - 2)
                    pout2 += s->pressure[q];
            }
    s->pressure_drop = (3 * pin1 - pin2 - 3 * pout1 + pout2) / (2 * g->n[1] * g->n[2]);
    s->velocity_error = sqrt(error / norm);
    s->flow_error = fabs(s->flow / s->reference_flow - 1);
    s->pressure_error = fabs(s->pressure_drop / (s->gradient * g->length[0]) - 1);
    for (int a = 0; a < 4; a++)
        s->wall_error[a] = fabs(s->wall_force[a] / s->reference_wall[a] - 1);
    s->dissipation_error = fabs(s->dissipation / s->reference_dissipation - 1);
    s->kinetic = kinetic(s);
    s->energy_rate =
        (s->steps == 1 ? s->kinetic - s->previous_energy
                       : 1.5 * s->kinetic - 2 * s->previous_energy + .5 * s->older_energy) /
        s->dt;
    /* Physical stress work from solved pressure traces. The reference and
     * discrete solutions have zero X derivatives, checked by full divergence. */
    s->boundary_power = 0;
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++) {
            int q = (k * g->n[1] + j) * g->n[0];
            double pin = (3 * s->pressure[q] - s->pressure[q + 1]) / 2,
                   pout = (3 * s->pressure[q + g->n[0] - 1] - s->pressure[q + g->n[0] - 2]) / 2;
            double ux0 = (face(s, 0, 1, j, k) - face(s, 0, 0, j, k)) / g->h[0],
                   ux1 = (face(s, 0, g->n[0], j, k) - face(s, 0, g->n[0] - 1, j, k)) / g->h[0];
            s->boundary_power += ((pin - 2 * s->mu * ux0) * face(s, 0, 0, j, k) +
                                  (-pout + 2 * s->mu * ux1) * face(s, 0, g->n[0], j, k)) *
                                 g->area[0];
        }
    s->energy_residual = s->boundary_power - s->dissipation - s->energy_rate;
    s->older_energy = s->previous_energy;
    s->previous_energy = s->kinetic;
}
bool cfd_startup3d_step(CfdStartup3d *s) {
    if (!s || !s->mixed || s->failed)
        return false;
    clock_t start = clock();
    double alpha = s->steps ? 1.5 : 1;
    if (!cfd_mixed3d_set_mass(s->mixed, s->rho * alpha / s->dt))
        goto fail;
    for (int q = 0; q < s->count; q++) {
        int a, c[3];
        double x[3];
        cfd_mixed3d_position(s->mixed, q, &a, x, c);
        double history = s->steps ? 2 * s->velocity[q] - .5 * s->previous[q] : s->velocity[q];
        s->rhs[q] = s->rho / s->dt * history * cfd_mixed3d_volume(s->mixed, q);
        if (a == 0 && c[0] == 0)
            s->rhs[q] += s->gradient * s->grid.length[0] * s->grid.area[0];
    }
    memcpy(s->candidate_pressure, s->pressure, (size_t)s->grid.count * sizeof(double));
    if (!cfd_mixed3d_solve(s->mixed, s->rhs, s->candidate, s->candidate_pressure))
        goto fail;
    memcpy(s->previous, s->velocity, (size_t)s->count * sizeof(double));
    memcpy(s->velocity, s->candidate, (size_t)s->count * sizeof(double));
    memcpy(s->pressure, s->candidate_pressure, (size_t)s->grid.count * sizeof(double));
    s->steps++;
    s->time += s->dt;
    s->true_residual = cfd_mixed3d_residual(s->mixed);
    s->iterations = cfd_mixed3d_iterations(s->mixed);
    s->inner_iterations = cfd_mixed3d_inner_iterations(s->mixed);
    observe(s);
    s->solve_cpu_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    return true;
fail:
    s->failed = true;
    s->error =
        cfd_mixed3d_error(s->mixed) ? cfd_mixed3d_error(s->mixed) : "startup_mass_or_solve_failed";
    return false;
}
