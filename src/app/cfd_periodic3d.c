#include "app/cfd_periodic3d.h"
#include <math.h>
#include <string.h>
#include <time.h>
static const double pi = 3.14159265358979323846;
void cfd_periodic3d_exact(const double length[3], const double xyz[3], double t, double rho,
                          double nu, double v[3], double *pressure, double f[3]) {
    double k[3], sn[3], cs[3];
    for (int a = 0; a < 3; a++) {
        k[a] = 2 * pi / length[a];
        sn[a] = sin(k[a] * xyz[a]);
        cs[a] = cos(k[a] * xyz[a]);
    }
    double amp = .01 * (1 + .2 * sin(4 * t)), rate = .008 * cos(4 * t), pamp = .02 * cos(4 * t);
    double base[3] = {sn[2] + cs[1], sn[0] + cs[2], sn[1] + cs[0]};
    for (int a = 0; a < 3; a++)
        v[a] = amp * base[a];
    if (pressure)
        *pressure = rho * pamp * sn[0] * sn[1] * sn[2];
    if (f) {
        double conv[3] = {v[1] * (-amp * k[1] * sn[1]) + v[2] * amp * k[2] * cs[2],
                          v[0] * amp * k[0] * cs[0] + v[2] * (-amp * k[2] * sn[2]),
                          v[0] * (-amp * k[0] * sn[0]) + v[1] * amp * k[1] * cs[1]};
        double lap[3] = {-amp * (k[2] * k[2] * sn[2] + k[1] * k[1] * cs[1]),
                         -amp * (k[0] * k[0] * sn[0] + k[2] * k[2] * cs[2]),
                         -amp * (k[1] * k[1] * sn[1] + k[0] * k[0] * cs[0])};
        for (int a = 0; a < 3; a++) {
            double grad = pamp * k[a] * cs[a];
            for (int b = 0; b < 3; b++)
                if (a != b)
                    grad *= sn[b];
            f[a] = rate * base[a] + conv[a] - nu * lap[a] + grad;
        }
    }
}
void cfd_periodic3d_transport(const CfdCartesian3d *g, const double *v, double *out) {
    int n = g->count;
    for (int a = 0; a < 3; a++)
        for (int q = 0; q < n; q++) {
            double sum = 0;
            for (int b = 0; b < 3; b++) {
                int plus = cfd_cartesian3d_neighbor(g, q, b, 1),
                    minus = cfd_cartesian3d_neighbor(g, q, b, -1);
                double wp, wm;
                if (a == b) {
                    wp = .5 * (v[b * n + q] + v[b * n + plus]);
                    wm = .5 * (v[b * n + q] + v[b * n + minus]);
                } else {
                    wp = .5 *
                         (v[b * n + plus] + v[b * n + cfd_cartesian3d_neighbor(g, plus, a, -1)]);
                    wm = .5 * (v[b * n + q] + v[b * n + cfd_cartesian3d_neighbor(g, q, a, -1)]);
                }
                sum += (wp * .5 * (v[a * n + q] + v[a * n + plus]) -
                        wm * .5 * (v[a * n + q] + v[a * n + minus])) /
                       g->h[b];
            }
            out[a * n + q] = sum;
        }
}
void cfd_periodic3d_destroy(CfdPeriodic3d *s) {
    if (!s)
        return;
    cfd_cartesian3d_linear_destroy(s->momentum);
    cfd_cartesian3d_linear_destroy(s->poisson);
    cfd_memory_free(s->storage);
    s->momentum = s->poisson = NULL;
    s->storage = NULL;
}
static double kinetic(const CfdPeriodic3d *s) {
    double e = 0;
    for (int q = 0; q < 3 * s->grid.count; q++)
        e += s->velocity[q] * s->velocity[q];
    return .5 * s->rho * s->grid.volume * e;
}
bool cfd_periodic3d_init(CfdPeriodic3d *s, const int n[3], const double length[3], double rho,
                         double mu, double dt) {
    if (!s)
        return false;
    memset(s, 0, sizeof(*s));
    clock_t start = clock();
    if (!cfd_cartesian3d_init(&s->grid, n, length) || !isfinite(rho) || rho <= 0 || !isfinite(mu) ||
        mu <= 0 || !isfinite(dt) || dt <= 0 || dt > .1)
        return false;
    s->rho = rho;
    s->nu = mu / rho;
    s->dt = dt;
    s->mass = 1 / dt;
    int count = s->grid.count;
    s->storage = cfd_memory_calloc((size_t)27 * count, sizeof(double));
    if (!s->storage)
        return false;
    s->velocity = s->storage;
    s->previous = s->velocity + 3 * count;
    s->pressure = s->previous + 3 * count;
    s->rhs = s->pressure + count;
    s->star = s->rhs + 3 * count;
    s->gradient = s->star + 3 * count;
    s->phi = s->gradient + 3 * count;
    s->divergence = s->phi + count;
    s->transport = s->divergence + count;
    s->extrapolated = s->transport + 3 * count;
    s->forcing = s->extrapolated + 3 * count;
    /* storage count: 3+3+1+3+3+3+1+1+3+3+3=27 */
    bool periodic[3] = {true, true, true};
    s->momentum = cfd_cartesian3d_linear_create(&s->grid, periodic, s->mass, s->nu, false);
    s->poisson = cfd_cartesian3d_linear_create(&s->grid, periodic, 0, 1, true);
    if (!s->momentum || !s->poisson) {
        cfd_periodic3d_destroy(s);
        return false;
    }
    for (int q = 0; q < count; q++) {
        double xyz[3], v[3];
        for (int a = 0; a < 3; a++) {
            cfd_cartesian3d_position(&s->grid, q, a, xyz);
            cfd_periodic3d_exact(length, xyz, 0, rho, s->nu, v, NULL, NULL);
            s->velocity[a * count + q] = v[a];
        }
        cfd_cartesian3d_position(&s->grid, q, -1, xyz);
        cfd_periodic3d_exact(length, xyz, 0, rho, s->nu, v, &s->pressure[q], NULL);
    }
    memcpy(s->previous, s->velocity, (size_t)3 * count * sizeof(double));
    s->previous_energy = s->older_energy = kinetic(s);
    s->setup_cpu_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    return true;
}
bool cfd_periodic3d_init_unforced(CfdPeriodic3d *s, const int n[3],
                                  const double length[3], double rho, double mu,
                                  double dt, const double *velocity, size_t values) {
    CfdCartesian3d grid;
    if (!s || !velocity || !cfd_cartesian3d_init(&grid, n, length) ||
        values != (size_t)3 * grid.count)
        return false;
    for (size_t q = 0; q < values; q++)
        if (!isfinite(velocity[q]))
            return false;
    if (!cfd_periodic3d_init(s, n, length, rho, mu, dt))
        return false;
    cfd_cartesian3d_divergence(&s->grid, velocity, s->divergence);
    for (int q = 0; q < grid.count; q++) {
        if (!isfinite(s->divergence[q]) || fabs(s->divergence[q]) >= 1e-8) {
            cfd_periodic3d_destroy(s);
            return false;
        }
        s->max_divergence = fmax(s->max_divergence, fabs(s->divergence[q]));
    }
    memcpy(s->velocity, velocity, values * sizeof(double));
    memcpy(s->previous, velocity, values * sizeof(double));
    memset(s->pressure, 0, (size_t)grid.count * sizeof(double));
    s->previous_energy = s->older_energy = kinetic(s);
    s->kinetic_j = s->previous_energy;
    s->unforced = true;
    s->velocity_l2_error = s->pressure_l2_error = NAN;
    return true;
}
static void observe(CfdPeriodic3d *s) {
    CfdCartesian3d *g = &s->grid;
    int n = g->count;
    double verr = 0, perr = 0;
    s->forcing_power_w = s->physical_dissipation_w = 0;
    for (int q = 0; q < n; q++) {
        double xyz[3], v[3], p;
        for (int a = 0; a < 3; a++) {
            cfd_cartesian3d_position(g, q, a, xyz);
            cfd_periodic3d_exact(g->length, xyz, s->time, s->rho, s->nu, v, NULL, NULL);
            double d = s->velocity[a * n + q] - v[a];
            verr += d * d;
            s->forcing_power_w +=
                s->rho * s->velocity[a * n + q] * s->forcing[a * n + q] * g->volume;
        }
        cfd_cartesian3d_position(g, q, -1, xyz);
        cfd_periodic3d_exact(g->length, xyz, s->time, s->rho, s->nu, v, &p, NULL);
        double d = s->pressure[q] - p;
        perr += d * d;
        double derivative[3][3];
        for (int a = 0; a < 3; a++)
            for (int b = 0; b < 3; b++) {
                if (a == b)
                    derivative[a][b] = (s->velocity[a * n + cfd_cartesian3d_neighbor(g, q, a, 1)] -
                                        s->velocity[a * n + q]) /
                                       g->h[a];
                else {
                    int plus = cfd_cartesian3d_neighbor(g, q, b, 1),
                        minus = cfd_cartesian3d_neighbor(g, q, b, -1);
                    double up = .5 * (s->velocity[a * n + plus] +
                                      s->velocity[a * n + cfd_cartesian3d_neighbor(g, plus, a, 1)]);
                    double um =
                        .5 * (s->velocity[a * n + minus] +
                              s->velocity[a * n + cfd_cartesian3d_neighbor(g, minus, a, 1)]);
                    derivative[a][b] = (up - um) / (2 * g->h[b]);
                }
            }
        for (int a = 0; a < 3; a++)
            for (int b = 0; b < 3; b++) {
                double strain = .5 * (derivative[a][b] + derivative[b][a]);
                s->physical_dissipation_w += 2 * s->rho * s->nu * strain * strain * g->volume;
            }
    }
    s->velocity_l2_error = s->unforced ? NAN : sqrt(verr / (3 * n));
    s->pressure_l2_error = s->unforced ? NAN : sqrt(perr / n);
    s->kinetic_j = kinetic(s);
    s->energy_rate_w =
        s->steps == 1 ? (s->kinetic_j - s->previous_energy) / s->dt
                      : (3 * s->kinetic_j - 4 * s->previous_energy + s->older_energy) / (2 * s->dt);
    s->energy_residual_w = s->forcing_power_w - s->physical_dissipation_w - s->energy_rate_w;
    s->older_energy = s->previous_energy;
    s->previous_energy = s->kinetic_j;
}
bool cfd_periodic3d_step(CfdPeriodic3d *s) {
    if (!s || !s->storage || s->failed)
        return false;
    CfdCartesian3d *g = &s->grid;
    int n = g->count;
    clock_t start = clock();
    s->cfl = 0;
    for (int q = 0; q < 3 * n; q++)
        s->extrapolated[q] = s->steps ? 2 * s->velocity[q] - s->previous[q] : s->velocity[q];
    for (int a = 0; a < 3; a++) {
        double maximum = 0;
        for (int q = 0; q < n; q++)
            maximum = fmax(maximum, fabs(s->extrapolated[a * n + q]));
        s->cfl += maximum * s->dt / g->h[a];
    }
    if (!isfinite(s->cfl) || s->cfl > .25) {
        s->failed = true;
        return false;
    }
    double mass = s->steps ? 1.5 / s->dt : 1 / s->dt;
    if (mass != s->mass) {
        bool periodic[3] = {true, true, true};
        CfdCartesian3dLinear *next = cfd_cartesian3d_linear_create(g, periodic, mass, s->nu, false);
        if (!next) {
            s->failed = true;
            return false;
        }
        cfd_cartesian3d_linear_destroy(s->momentum);
        s->momentum = next;
        s->mass = mass;
    }
    for (int q = 0; q < 3 * n; q++)
        s->extrapolated[q] = s->steps ? 2 * s->velocity[q] - s->previous[q] : s->velocity[q];
    cfd_periodic3d_transport(g, s->extrapolated, s->transport);
    double next_time = s->time + s->dt;
    for (int a = 0; a < 3; a++)
        for (int q = 0; q < n; q++) {
            double xyz[3], v[3], f[3];
            cfd_cartesian3d_position(g, q, a, xyz);
            if (s->unforced)
                f[0] = f[1] = f[2] = 0;
            else
                cfd_periodic3d_exact(g->length, xyz, next_time, s->rho, s->nu, v, NULL, f);
            s->forcing[a * n + q] = f[a];
            double history =
                s->steps ? (2 * s->velocity[a * n + q] - .5 * s->previous[a * n + q]) / s->dt
                         : s->velocity[a * n + q] / s->dt;
            s->rhs[a * n + q] = history - s->transport[a * n + q] + f[a];
        }
    s->transport_cpu_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    start = clock();
    s->true_residual = 0;
    s->iterations = 0;
    for (int a = 0; a < 3; a++) {
        int it;
        double residual;
        if (!cfd_cartesian3d_linear_solve(s->momentum, s->rhs + a * n, s->star + a * n, &it,
                                          &residual))
            goto fail;
        s->iterations += it;
        s->true_residual = fmax(s->true_residual, residual);
    }
    cfd_cartesian3d_divergence(g, s->star, s->divergence);
    for (int q = 0; q < n; q++)
        s->divergence[q] = -s->divergence[q];
    s->divergence[0] = 0;
    s->phi[0] = 0;
    int it;
    double residual;
    if (!cfd_cartesian3d_linear_solve(s->poisson, s->divergence, s->phi, &it, &residual))
        goto fail;
    s->iterations += it;
    s->true_residual = fmax(s->true_residual, residual);
    double mean = 0;
    for (int q = 0; q < n; q++)
        mean += s->phi[q] / n;
    for (int q = 0; q < n; q++)
        s->phi[q] -= mean;
    cfd_cartesian3d_gradient(g, s->phi, s->gradient);
    /* H and grad commute on uniform periodic grids. The physical kinematic
     * pressure is H*phi, not phi/dt from an approximate projection split. */

    for (int q = 0; q < 3 * n; q++)
        s->extrapolated[q] = s->star[q] - s->gradient[q];
    cfd_cartesian3d_divergence(g, s->extrapolated, s->divergence);
    s->max_divergence = 0;
    for (int q = 0; q < n; q++)
        s->max_divergence = fmax(s->max_divergence, fabs(s->divergence[q]));
    if (!isfinite(s->max_divergence) || s->max_divergence >= 1e-8)
        goto fail;
    /* Independently check the complete momentum equation, not only subsolves.
     * gradient scratch now holds candidate kinematic pressure and its gradient. */
    cfd_cartesian3d_linear_apply(s->momentum, s->phi, s->transport);
    cfd_cartesian3d_gradient(g, s->transport, s->gradient);
    double norm_rhs = 0, norm_error = 0;
    for (int a = 0; a < 3; a++) {
        cfd_cartesian3d_linear_apply(s->momentum, s->extrapolated + a * n, s->divergence);
        for (int q = 0; q < n; q++) {
            double r = s->divergence[q] + s->gradient[a * n + q] - s->rhs[a * n + q];
            norm_error += r * r;
            norm_rhs += s->rhs[a * n + q] * s->rhs[a * n + q];
        }
    }
    double full = sqrt(norm_error / fmax(norm_rhs, 1e-60));
    s->true_residual = fmax(s->true_residual, full);
    if (!isfinite(full) || full > 1e-11)
        goto fail;
    for (int q = 0; q < n; q++)
        s->pressure[q] = s->rho * s->transport[q];
    /* Do not publish partially solved candidate state. */
    memcpy(s->previous, s->velocity, (size_t)3 * n * sizeof(double));
    memcpy(s->velocity, s->extrapolated, (size_t)3 * n * sizeof(double));
    s->time = next_time;
    s->steps++;
    observe(s);
    s->solve_cpu_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    return true;
fail:
    s->failed = true;
    return false;
}
