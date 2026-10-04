#include "app/cfd_wall3d.h"
#include <math.h>
#include <string.h>
#include <time.h>
static const double pi = 3.14159265358979323846;
static double dot(const double *a, const double *b, int n) {
    double v = 0;
    for (int q = 0; q < n; q++)
        v += a[q] * b[q];
    return v;
}
double cfd_wall3d_face(const CfdWall3d *s, const double *u, int axis, int i, int j, int k) {
    int q = cfd_mixed3d_index(s->mixed, axis, i, j, k);
    return q >= 0 ? u[q] : 0;
}
static double center(const CfdWall3d *s, int a, int i, int j, int k) {
    int hi[3] = {i, j, k};
    hi[a]++;
    return .5 * (cfd_wall3d_face(s, s->velocity, a, i, j, k) +
                 cfd_wall3d_face(s, s->velocity, a, hi[0], hi[1], hi[2]));
}
void cfd_wall3d_derivatives(const CfdWall3d *s, int i, int j, int k, double d[3][3]) {
    const CfdCartesian3d *g = &s->grid;
    int c[3] = {i, j, k};
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) {
            int lo[3] = {i, j, k}, hi[3] = {i, j, k};
            lo[b]--;
            hi[b]++;
            if (a == b)
                d[a][b] = (cfd_wall3d_face(s, s->velocity, a, hi[0], hi[1], hi[2]) -
                           cfd_wall3d_face(s, s->velocity, a, i, j, k)) /
                          g->h[b];
            else {
                if (b == 0 && !s->open) {
                    lo[0] = (i + g->n[0] - 1) % g->n[0];
                    hi[0] = (i + 1) % g->n[0];
                }
                /* Reconstruct the derivative of cell-centred face averages.
                 * Use cubic one-sided stencils near real boundaries and the
                 * fourth-order centred stencil elsewhere; no point-value odd
                 * ghosts are inferred from cell averages. Physical area-average
                 * smoothing remains a reported second-order reconstruction error. */
                bool bounded = b > 0 || s->open;
                if (bounded && (c[b] < 2 || c[b] >= g->n[b] - 2)) {
                    int side = c[b] < 2 ? 0 : 1, direction = side ? -1 : 1;
                    int first[3] = {i, j, k};
                    first[b] = side ? g->n[b] - 1 : 0;
                    double v[4];
                    for (int m = 0; m < 4; m++) {
                        int at[3] = {first[0], first[1], first[2]};
                        at[b] += direction * m;
                        v[m] = center(s, a, at[0], at[1], at[2]);
                    }
                    bool outer = c[b] == first[b];
                    d[a][b] = direction *
                              (outer ? -11 * v[0] + 18 * v[1] - 9 * v[2] + 2 * v[3]
                                     : -2 * v[0] - 3 * v[1] + 6 * v[2] - v[3]) /
                              (6 * g->h[b]);
                } else {
                    int lo2[3] = {i, j, k}, hi2[3] = {i, j, k};
                    lo2[b] -= 2;
                    hi2[b] += 2;
                    d[a][b] = (center(s, a, lo2[0], lo2[1], lo2[2]) -
                               8 * center(s, a, lo[0], lo[1], lo[2]) +
                               8 * center(s, a, hi[0], hi[1], hi[2]) -
                               center(s, a, hi2[0], hi2[1], hi2[2])) /
                              (12 * g->h[b]);
                }
            }
        }
}
static double kinetic(const CfdWall3d *s, const double *u) {
    double e = 0;
    for (int q = 0; q < s->count; q++)
        e += u[q] * u[q] * cfd_mixed3d_volume(s->mixed, q);
    return .5 * s->rho * e;
}
/* Analytical separable integrals of curl(A) and its gradient. Cross terms
 * vanish by orthogonality over full/half X periods and complete Y/Z envelopes.
 * For zero boundary physical work, integral 2 S:S equals integral grad(u):grad(u). */
static void exact_integrals(CfdWall3d *s) {
    double norm[3][3];
    for (int a = 0; a < 3; a++) {
        double k = 2 * pi / s->reference_length[a], L = s->grid.length[a];
        norm[a][0] = a == 0 ? L / 2 : 3 * L / 8;
        norm[a][1] = k * k * L / (a == 0 ? 2 : 8);
        norm[a][2] = k * k * k * k * L / (a == 0 ? 2 : 8);
    }
    const double coef[3][2] = {{.013, -.007}, {.011, -.013}, {.007, -.011}};
    const int derivative[3][2] = {{1, 2}, {2, 0}, {0, 1}};
    for (int c = 0; c < 3; c++)
        for (int term = 0; term < 2; term++) {
            double e = coef[c][term] * coef[c][term];
            for (int a = 0; a < 3; a++)
                e *= norm[a][a == derivative[c][term]];
            s->kinetic_base += .5 * s->rho * e;
            for (int b = 0; b < 3; b++) {
                double d = coef[c][term] * coef[c][term];
                for (int a = 0; a < 3; a++)
                    d *= norm[a][(a == derivative[c][term]) + (a == b)];
                s->dissipation_base += s->mu * d;
            }
        }
}
void cfd_wall3d_destroy(CfdWall3d *s) {
    if (!s)
        return;
    cfd_mixed3d_destroy(s->mixed);
    cfd_memory_free(s->storage);
    s->mixed = NULL;
    s->storage = NULL;
}
static bool init(CfdWall3d *s, const int n[3], const double length[3], double rho, double mu,
                 double dt, bool transport, bool open) {
    if (!s)
        return false;
    memset(s, 0, sizeof(*s));
    clock_t start = clock();
    if (!cfd_cartesian3d_init(&s->grid, n, length) || !isfinite(rho) || rho <= 0 || !isfinite(mu) ||
        mu <= 0 || !isfinite(dt) || dt <= 0 || dt > .1)
        return false;
    s->rho = rho;
    s->mu = mu;
    s->dt = dt;
    s->transport = transport;
    s->open = open;
    memcpy(s->reference_length, length, sizeof(s->reference_length));
    if (open) {
        if (length[0] < 4 || fabs(length[0] / 2 - round(length[0] / 2)) > 1e-12 || transport)
            return false;
        s->reference_length[0] = 4;
    }
    s->mixed = cfd_mixed3d_create(&s->grid, mu, rho / dt, !open);
    if (!s->mixed)
        return false;
    s->count = cfd_mixed3d_count(s->mixed);
    int m = s->count, N = s->grid.count;
    s->storage = cfd_memory_calloc((size_t)(open ? 13 : 11) * m + 3 * N, sizeof(double));
    if (!s->storage) {
        cfd_wall3d_destroy(s);
        return false;
    }
    s->velocity = s->storage;
    s->previous = s->velocity + m;
    s->rhs = s->previous + m;
    s->candidate = s->rhs + m;
    s->face_reference = s->candidate + m;
    s->dual_reference = s->face_reference + m;
    s->lap_reference = s->dual_reference + m;
    s->grad_pressure_reference = s->lap_reference + m;
    s->transport_reference = s->grad_pressure_reference + m;
    s->transport_work = s->transport_reference + m;
    s->extrapolated = s->transport_work + m;
    s->pressure = s->velocity + 11 * m;
    s->candidate_pressure = s->pressure + N;
    s->pressure_reference = s->candidate_pressure + N;
    if (open) {
        s->traction_velocity = s->pressure_reference + N;
        s->traction_pressure = s->traction_velocity + m;
    }
    const int zero[3] = {0};
    for (int q = 0; q < m; q++) {
        double x[3], facewidth[3], v[3], lap = 0;
        int a, c[3];
        cfd_mixed3d_position(s->mixed, q, &a, x, c);
        for (int b = 0; b < 3; b++)
            facewidth[b] = a == b ? 0 : s->grid.h[b];
        cfd_wall3d_reference(s->reference_length, x, facewidth, zero, v);
        s->face_reference[q] = s->velocity[q] = v[a];
        double dualwidth[3] = {s->grid.h[0], s->grid.h[1], s->grid.h[2]};
        if (open && a == 0 && (c[0] == 0 || c[0] == n[0])) {
            dualwidth[0] /= 2;
            x[0] += c[0] == 0 ? s->grid.h[0] / 4 : -s->grid.h[0] / 4;
        }
        cfd_wall3d_reference(s->reference_length, x, dualwidth, zero, v);
        s->dual_reference[q] = v[a];
        for (int b = 0; b < 3; b++) {
            int d[3] = {0};
            d[b] = 2;
            cfd_wall3d_reference(s->reference_length, x, dualwidth, d, v);
            lap += v[a];
        }
        s->lap_reference[q] = lap;
        s->grad_pressure_reference[q] =
            cfd_wall3d_reference_pressure(s->reference_length, x, dualwidth, a);
        if (transport) {
            cfd_wall3d_reference_transport(s->reference_length, x, s->grid.h, v);
            s->transport_reference[q] = v[a];
        }
        if (open && (c[0] == 0 || c[0] == n[0] - (a != 0))) {
            int side = c[0] == 0 ? -1 : 1, dx[3] = {1, 0, 0};
            x[0] = side < 0 ? 0 : length[0];
            double width[3] = {0, s->grid.h[1], s->grid.h[2]};
            cfd_wall3d_reference(s->reference_length, x, width, dx, v);
            s->traction_velocity[q] = side * mu * v[a] * s->grid.area[0];
            if (a == 0)
                s->traction_pressure[q] =
                    -side * s->grid.area[0] *
                    cfd_wall3d_reference_pressure(s->reference_length, x, width, -1);
        }
    }
    for (int q = 0; q < N; q++) {
        double x[3], width[3] = {0};
        cfd_cartesian3d_position(&s->grid, q, -1, x);
        s->pressure_reference[q] = cfd_wall3d_reference_pressure(s->reference_length, x, width, -1);
        s->pressure[q] = .01 * cos(.3) * s->pressure_reference[q];
    }
    memcpy(s->previous, s->velocity, (size_t)m * sizeof(double));
    exact_integrals(s);
    s->previous_energy = s->older_energy = kinetic(s, s->velocity);
    s->setup_cpu_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    return true;
}
bool cfd_wall3d_init(CfdWall3d *s, const int n[3], const double length[3], double rho, double mu,
                     double dt, bool transport) {
    return init(s, n, length, rho, mu, dt, transport, false);
}
bool cfd_wall3d_init_open(CfdWall3d *s, const int n[3], const double length[3], double rho,
                          double mu, double dt) {
    return init(s, n, length, rho, mu, dt, false, true);
}
/* Physical symmetric stress work from solved pressure/velocities, distinct
 * from the prescribed vector-Laplacian traction used by the mixed equations. */
static double boundary_work(const CfdWall3d *s) {
    const CfdCartesian3d *g = &s->grid;
    double power = 0;
    for (int side = 0; side < 2; side++)
        for (int k = 0; k < g->n[2]; k++)
            for (int j = 0; j < g->n[1]; j++) {
                int i = side ? g->n[0] - 1 : 0, direction = side ? -1 : 1;
                int q = (k * g->n[1] + j) * g->n[0] + i;
                double p = (3 * s->pressure[q] - s->pressure[q + direction]) / 2;
                int face = side ? g->n[0] : 0;
                double u = cfd_wall3d_face(s, s->velocity, 0, face, j, k);
                double ux =
                    direction *
                    (-3 * u + 4 * cfd_wall3d_face(s, s->velocity, 0, face + direction, j, k) -
                     cfd_wall3d_face(s, s->velocity, 0, face + 2 * direction, j, k)) /
                    (2 * g->h[0]);
                double work = (2 * s->mu * ux - p) * u;
                for (int a = 1; a < 3; a++) {
                    double v0 = center(s, a, i, j, k), v1 = center(s, a, i + direction, j, k),
                           v2 = center(s, a, i + 2 * direction, j, k);
                    double vb = (3 * v0 - v1) / 2,
                           vx = direction * (-2 * v0 + 3 * v1 - v2) / g->h[0];
                    int lo[3] = {face, j, k}, hi[3] = {face, j, k};
                    lo[a]--;
                    hi[a]++;
                    int c = a == 1 ? j : k;
                    double um =
                        c == 0 ? -u : cfd_wall3d_face(s, s->velocity, 0, lo[0], lo[1], lo[2]);
                    double up = c == g->n[a] - 1
                                    ? -u
                                    : cfd_wall3d_face(s, s->velocity, 0, hi[0], hi[1], hi[2]);
                    work += s->mu * (vx + (up - um) / (2 * g->h[a])) * vb;
                }
                power += (side ? 1 : -1) * work * g->area[0];
            }
    return power;
}
static void observe(CfdWall3d *s) {
    const CfdCartesian3d *g = &s->grid;
    double amp = 1 + .2 * sin(2 * pi * s->time), rate = .4 * pi * cos(2 * pi * s->time),
           pamp = .01 * cos(2 * pi * s->time + .3);
    double err = 0, norm = 0, perr = 0, pnorm = 0;
    for (int q = 0; q < s->count; q++) {
        double exact = amp * s->face_reference[q], d = s->velocity[q] - exact,
               vol = cfd_mixed3d_volume(s->mixed, q);
        err += d * d * vol;
        norm += exact * exact * vol;
    }
    for (int q = 0; q < g->count; q++) {
        double exact = pamp * s->pressure_reference[q], d = s->pressure[q] - exact;
        perr += d * d;
        pnorm += .0001 * s->pressure_reference[q] * s->pressure_reference[q];
    }
    s->velocity_error = sqrt(err / norm);
    s->pressure_error = sqrt(perr / fmax(pnorm, 1e-30));
    s->amplitude = dot(s->velocity, s->face_reference, s->count) /
                   dot(s->face_reference, s->face_reference, s->count);
    double orth = 0;
    for (int q = 0; q < s->count; q++) {
        double d = s->velocity[q] - s->amplitude * s->face_reference[q];
        orth += d * d;
    }
    s->orthogonal_error = sqrt(orth / dot(s->face_reference, s->face_reference, s->count));
    s->dissipation_w = s->max_divergence = 0;
    double we[8] = {0}, wn[8] = {0};
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                double d[3][3];
                cfd_wall3d_derivatives(s, i, j, k, d);
                s->max_divergence = fmax(s->max_divergence, fabs(d[0][0] + d[1][1] + d[2][2]));
                for (int a = 0; a < 3; a++)
                    for (int b = 0; b < 3; b++) {
                        double strain = .5 * (d[a][b] + d[b][a]);
                        s->dissipation_w += 2 * s->mu * strain * strain * g->volume;
                    }
                int c[3] = {i, j, k};
                for (int b = 1; b < 3; b++)
                    if (c[b] == 0 || c[b] == g->n[b] - 1) {
                        int side = c[b] == 0 ? 0 : 1, which = 2 * (b - 1) + side;
                        double x[3] = {(i + .5) * g->h[0], (j + .5) * g->h[1], (k + .5) * g->h[2]},
                               width[3] = {0}, v[3];
                        x[b] = side ? g->length[b] : 0;
                        int du[3] = {0};
                        du[b] = 1;
                        cfd_wall3d_reference(s->reference_length, x, width, du, v);
                        for (int a = 0, tangent = 0; a < 3; a++)
                            if (a != b) {
                                int slot = 2 * which + tangent++;
                                double exact = s->mu * amp * (side ? 1 : -1) * v[a],
                                       native = -s->mu * 2 * center(s, a, i, j, k) / g->h[b];
                                we[slot] += (native - exact) * (native - exact);
                                wn[slot] += exact * exact;
                            }
                    }
            }
    for (int a = 0; a < 8; a++)
        s->wall_error[a] = sqrt(we[a] / fmax(wn[a], 1e-30));
    s->kinetic_j = kinetic(s, s->velocity);
    s->energy_rate_w =
        (s->steps == 1 ? s->kinetic_j - s->previous_energy
                       : 1.5 * s->kinetic_j - 2 * s->previous_energy + .5 * s->older_energy) /
        s->dt;
    s->boundary_power_w = s->open ? boundary_work(s) : 0;
    s->energy_residual_w = s->forcing_power_w + s->boundary_power_w - s->transport_power_w -
                           s->dissipation_w - s->energy_rate_w;
    s->reference_kinetic_j = amp * amp * s->kinetic_base;
    s->reference_dissipation_w = amp * amp * s->dissipation_base;
    s->reference_energy_rate_w = 2 * amp * rate * s->kinetic_base;
    s->reference_power_w = s->reference_dissipation_w + s->reference_energy_rate_w;
    s->older_energy = s->previous_energy;
    s->previous_energy = s->kinetic_j;
}
bool cfd_wall3d_step(CfdWall3d *s) {
    if (!s || !s->mixed || s->failed)
        return false;
    clock_t start = clock();
    double next = s->time + s->dt, amp = 1 + .2 * sin(2 * pi * next),
           rate = .4 * pi * cos(2 * pi * next), pamp = .01 * cos(2 * pi * next + .3),
           alpha = s->steps ? 1.5 : 1;
    if (!cfd_mixed3d_set_mass(s->mixed, s->rho * alpha / s->dt))
        goto fail;
    s->cfl = s->transport_power_w = s->transport_self_power_w = 0;
    if (s->transport) {
        double maxv[3] = {0};
        for (int q = 0; q < s->count; q++) {
            int a;
            double x[3];
            cfd_mixed3d_position(s->mixed, q, &a, x, NULL);
            s->extrapolated[q] = s->steps ? 2 * s->velocity[q] - s->previous[q] : s->velocity[q];
            maxv[a] = fmax(maxv[a], fabs(s->extrapolated[q]));
        }
        for (int a = 0; a < 3; a++)
            s->cfl += s->dt * maxv[a] / s->grid.h[a];
        if (!isfinite(s->cfl) || s->cfl > .25) {
            s->error = "wall_transport_cfl_exceeded";
            goto fail;
        }
        cfd_wall3d_transport(s, s->extrapolated, s->transport_work);
        for (int q = 0; q < s->count; q++)
            s->transport_self_power_w += s->rho * s->extrapolated[q] * s->transport_work[q] *
                                         cfd_mixed3d_volume(s->mixed, q);
    }
    for (int q = 0; q < s->count; q++) {
        double vol = cfd_mixed3d_volume(s->mixed, q),
               history = s->steps ? 2 * s->velocity[q] - .5 * s->previous[q] : s->velocity[q];
        double f = s->rho * rate * s->dual_reference[q] - s->mu * amp * s->lap_reference[q] +
                   pamp * s->grad_pressure_reference[q] +
                   (s->transport ? s->rho * amp * amp * s->transport_reference[q] : 0);
        s->rhs[q] = vol * (s->rho / s->dt * history + f -
                           (s->transport ? s->rho * s->transport_work[q] : 0));
        if (s->open)
            s->rhs[q] += amp * s->traction_velocity[q] + pamp * s->traction_pressure[q];
    }
    memcpy(s->candidate_pressure, s->pressure, (size_t)s->grid.count * sizeof(double));
    if (!cfd_mixed3d_solve(s->mixed, s->rhs, s->candidate, s->candidate_pressure))
        goto fail;
    s->forcing_power_w = 0;
    for (int q = 0; q < s->count; q++) {
        double f = s->rho * rate * s->dual_reference[q] - s->mu * amp * s->lap_reference[q] +
                   pamp * s->grad_pressure_reference[q] +
                   (s->transport ? s->rho * amp * amp * s->transport_reference[q] : 0);
        s->forcing_power_w += f * s->candidate[q] * cfd_mixed3d_volume(s->mixed, q);
        if (s->transport)
            s->transport_power_w +=
                s->rho * s->candidate[q] * s->transport_work[q] * cfd_mixed3d_volume(s->mixed, q);
    }
    memcpy(s->previous, s->velocity, (size_t)s->count * sizeof(double));
    memcpy(s->velocity, s->candidate, (size_t)s->count * sizeof(double));
    memcpy(s->pressure, s->candidate_pressure, (size_t)s->grid.count * sizeof(double));
    s->time = next;
    s->steps++;
    s->true_residual = cfd_mixed3d_residual(s->mixed);
    s->iterations = cfd_mixed3d_iterations(s->mixed);
    s->inner_iterations = cfd_mixed3d_inner_iterations(s->mixed);
    observe(s);
    s->solve_cpu_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    return true;
fail:
    s->failed = true;
    if (!s->error)
        s->error =
            cfd_mixed3d_error(s->mixed) ? cfd_mixed3d_error(s->mixed) : "wall_setup_or_step_failed";
    return false;
}

/* Shared dual-face central momentum flux. Normal wall faces are zero; periodic
 * X neighbors share storage, tangential wall mass flux is exactly zero. */
void cfd_wall3d_transport(const CfdWall3d *s, const double *v, double *out) {
    for (int q = 0; q < s->count; q++) {
        int a, c[3];
        double x[3];
        cfd_mixed3d_position(s->mixed, q, &a, x, c);
        double sum = 0;
        for (int b = 0; b < 3; b++) {
            int hi[3] = {c[0], c[1], c[2]}, lo[3] = {c[0], c[1], c[2]};
            hi[b]++;
            lo[b]--;
            double wp, wm, up = cfd_wall3d_face(s, v, a, hi[0], hi[1], hi[2]),
                           um = cfd_wall3d_face(s, v, a, lo[0], lo[1], lo[2]);
            if (a == b) {
                wp = .5 * (v[q] + up);
                wm = .5 * (v[q] + um);
            } else {
                int hm[3] = {hi[0], hi[1], hi[2]}, cm[3] = {c[0], c[1], c[2]};
                hm[a]--;
                cm[a]--;
                wp = .5 * (cfd_wall3d_face(s, v, b, hi[0], hi[1], hi[2]) +
                           cfd_wall3d_face(s, v, b, hm[0], hm[1], hm[2]));
                wm = .5 * (cfd_wall3d_face(s, v, b, c[0], c[1], c[2]) +
                           cfd_wall3d_face(s, v, b, cm[0], cm[1], cm[2]));
            }
            sum += (wp * .5 * (v[q] + up) - wm * .5 * (v[q] + um)) / s->grid.h[b];
        }
        out[q] = sum;
    }
}
