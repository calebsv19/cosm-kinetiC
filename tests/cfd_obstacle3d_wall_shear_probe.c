/* Physical forcing from independently integrated smooth compact polynomials.
 * Nonzero one-sided shear on the front cube face; no matrix-manufactured RHS. */
#include "../src/app/cfd_obstacle3d_mixed.c"
#include "../src/app/cfd_obstacle3d.c"
#include "../src/app/cfd_obstacle3d_reconstruction.c"
#include "cfd_obstacle3d_wall_shear_candidate.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static const double low[3] = {.25, .625, .625}, high[3] = {1.5, 1.375, 1.375};
static const double coeff[3][9] = {
    {0, 0, 0, 0, 45.5625, -91.125, 45.5625, 0, 0},
    {0, 0, 0, 0, 256, -1024, 1536, -1024, 256},
    {0, 0, 0, 0, 256, -1024, 1536, -1024, 256}
};
static double polynomial(int axis, double t, int derivative) {
    double answer = 0;
    for (int k = derivative; k <= 8; k++) {
        double factor = coeff[axis][k];
        for (int j = 0; j < derivative; j++)
            factor *= k - j;
        answer += factor * pow(t, k - derivative);
    }
    return answer;
}
static double value(int axis, double x, int derivative) {
    double scale = high[axis] - low[axis];
    if (x < low[axis] || x > high[axis])
        return 0;
    return polynomial(axis, (x - low[axis]) / scale, derivative) / pow(scale, derivative);
}
static double mean(int axis, double a, double b, int derivative) {
    assert(b > a);
    double left = fmax(a, low[axis]), right = fmin(b, high[axis]);
    if (right <= left)
        return 0;
    if (derivative)
        return (value(axis, right, derivative - 1) - value(axis, left, derivative - 1)) / (b - a);
    double scale = high[axis] - low[axis], l = (left - low[axis]) / scale;
    double u = (right - low[axis]) / scale, integral = 0;
    for (int k = 0; k <= 8; k++)
        integral += coeff[axis][k] * (pow(u, k + 1) - pow(l, k + 1)) / (k + 1);
    return scale * integral / (b - a);
}
static void body_loads(const CfdObstacle3d *v, const double *u, const double *p,
                       double fp[3], double fv[3], double candidate[3]) {
    memset(fp, 0, 3 * sizeof(double));
    memset(fv, 0, 3 * sizeof(double));
    memset(candidate, 0, 3 * sizeof(double));
    for (int a = 0; a < 3; a++)
        for (int side = 0; side < 2; side++)
            for (int k = v->lo[2]; k < v->hi[2]; k++)
                for (int j = v->lo[1]; j < v->hi[1]; j++)
                    for (int i = v->lo[0]; i < v->hi[0]; i++) {
                        int cell[3] = {i, j, k}, direction = side ? 1 : -1;
                        if (cell[a] != v->lo[a])
                            continue;
                        cell[a] = side ? v->hi[a] : v->lo[a] - 1;
                        patch_force(v, u, p, a, direction, cell, fp, fv, NULL);
                        candidate_trace(v, u, a, direction, cell, candidate);
                    }
}
static double started;
static double now(void) {
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
static bool bounded(void *context) {
    (void)context;
    return now() - started < 600;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    int n = atoi(argv[1]);
    assert(n == 8 || n == 16 || n == 32 || n == 64);
    started = now();
    int dims[3] = {2 * n, n, n}, lo[3] = {3 * n / 4, n / 4, n / 4};
    int hi[3] = {5 * n / 4, 3 * n / 4, 3 * n / 4};
    double lengths[3] = {4, 2, 2}, h = 2. / n, amplitude = .0001, pa = .001;
    CfdMemoryBudget budget = {.limit_bytes = 1024 * 1024 * 1024};
    CfdMemoryBudget *old = cfd_memory_scope(&budget);
    CfdCartesian3d grid;
    assert(cfd_cartesian3d_init(&grid, dims, lengths));
    CfdObstacleMixed3d *s = cfd_obstacle_mixed3d_create(&grid, .1, lo, hi);
    assert(s && cfd_obstacle_mixed3d_verify(s));
    cfd_obstacle_mixed3d_checkpoint(s, bounded, NULL);
    double *rhs = cfd_memory_calloc(s->count, sizeof(double));
    double *u = cfd_memory_calloc(s->count, sizeof(double));
    double *p = cfd_memory_calloc(s->cells, sizeof(double));
    assert(rhs && u && p);
    for (int q = 0; q < s->count; q++) {
        int a, c[3];
        double x[3], m[3][4];
        cfd_obstacle_mixed3d_position(s, q, &a, x, c);
        for (int b = 0; b < 3; b++)
            for (int d = 0; d < 4; d++)
                m[b][d] = mean(b, x[b] - h / 2, x[b] + h / 2, d);
        double gradient = pa;
        for (int b = 0; b < 3; b++)
            gradient *= m[b][a == b];
        double laplacian = 0;
        if (a == 0)
            laplacian = amplitude * (m[0][2] * m[1][1] * m[2][0] +
                                    m[0][0] * m[1][3] * m[2][0] +
                                    m[0][0] * m[1][1] * m[2][2]);
        if (a == 1)
            laplacian = -amplitude * (m[0][3] * m[1][0] * m[2][0] +
                                     m[0][1] * m[1][2] * m[2][0] +
                                     m[0][1] * m[1][0] * m[2][2]);
        rhs[q] = (-.1 * laplacian + gradient) * cfd_obstacle_mixed3d_volume(s, q);
    }
    assert(cfd_obstacle_mixed3d_solve(s, rhs, u, p));
    double error_u = 0, norm_u = 0, error_p = 0, norm_p = 0, maxdiv = 0;
    for (int q = 0; q < s->count; q++) {
        int a, c[3];
        double x[3], exact = 0;
        cfd_obstacle_mixed3d_position(s, q, &a, x, c);
        if (a == 0)
            exact = amplitude * value(0, x[0], 0) * mean(1, x[1] - h / 2, x[1] + h / 2, 1) * mean(2, x[2] - h / 2, x[2] + h / 2, 0);
        if (a == 1)
            exact = -amplitude * mean(0, x[0] - h / 2, x[0] + h / 2, 1) * value(1, x[1], 0) * mean(2, x[2] - h / 2, x[2] + h / 2, 0);
        double v = cfd_obstacle_mixed3d_volume(s, q);
        error_u += v * (u[q] - exact) * (u[q] - exact);
        norm_u += v * exact * exact;
    }
    for (int k = 0; k < n; k++)
        for (int j = 0; j < n; j++)
            for (int i = 0; i < 2 * n; i++) {
                int cell = cfd_obstacle_mixed3d_cell(s, i, j, k);
                if (cell < 0)
                    continue;
                double exact = pa * mean(0, i * h, (i + 1) * h, 0) * mean(1, j * h, (j + 1) * h, 0) * mean(2, k * h, (k + 1) * h, 0);
                error_p += grid.volume * (p[cell] - exact) * (p[cell] - exact);
                norm_p += grid.volume * exact * exact;
            }
    cfd_obstacle_mixed3d_divergence(s, u, s->pwork);
    for (int q = 0; q < s->cells; q++)
        maxdiv = fmax(maxdiv, fabs(s->pwork[q]) / grid.volume);
    double residual = s->residual;
    int iterations = s->iterations;
    CfdObstacle3d view = {.grid = grid, .mixed = s, .mu = .1};
    memcpy(view.lo, lo, sizeof(lo));
    memcpy(view.hi, hi, sizeof(hi));
    double solved_fp[3], solved_fv[3], solved_candidate[3];
    double exact_fp[3], exact_fv[3], exact_candidate[3];
    body_loads(&view, u, p, solved_fp, solved_fv, solved_candidate);
    for (int q = 0; q < s->count; q++) {
        int a; double x[3], exact = 0;
        cfd_obstacle_mixed3d_position(s, q, &a, x, NULL);
        if (a == 0)
            exact = amplitude * value(0, x[0], 0) * mean(1, x[1]-h/2, x[1]+h/2, 1) * mean(2, x[2]-h/2, x[2]+h/2, 0);
        if (a == 1)
            exact = -amplitude * mean(0, x[0]-h/2, x[0]+h/2, 1) * value(1, x[1], 0) * mean(2, x[2]-h/2, x[2]+h/2, 0);
        rhs[q] = exact;
    }
    for (int k = 0; k < n; k++)
        for (int j = 0; j < n; j++)
            for (int i = 0; i < 2*n; i++) {
                int cell = cfd_obstacle_mixed3d_cell(s, i, j, k);
                if (cell >= 0)
                    s->pwork[cell] = pa * mean(0, i*h, (i+1)*h, 0) * mean(1, j*h, (j+1)*h, 0) * mean(2, k*h, (k+1)*h, 0);
            }
    body_loads(&view, rhs, s->pwork, exact_fp, exact_fv, exact_candidate);
    double analytic_fy = .1 * amplitude * value(0, high[0], 2) * mean(1, .5, 1.5, 0) * mean(2, .5, 1.5, 0);
    assert(analytic_fy > 0);
    cfd_memory_free(rhs);cfd_memory_free(u);cfd_memory_free(p);
    cfd_obstacle_mixed3d_destroy(s);
    assert(budget.live_bytes == 0 && norm_u > 0 && norm_p > 0);
    cfd_memory_scope(old);
    printf("{\"n\":%d,\"velocity_relative_error\":%.17g,\"pressure_relative_error\":%.17g,\"maximum_divergence\":%.17g,\"momentum_residual\":%.17g,\"iterations\":%d,\"wall_s\":%.17g,\"peak_owned_bytes\":%zu,",n,sqrt(error_u/norm_u),sqrt(error_p/norm_p),maxdiv,residual,iterations,now()-started,budget.peak_bytes);
    printf("\"analytic_viscous_force_n\":[0,%.17g,0],", analytic_fy);
    printf("\"solved_pressure_force_n\":[%.17g,%.17g,%.17g],", solved_fp[0], solved_fp[1], solved_fp[2]);
    printf("\"solved_viscous_force_n\":[%.17g,%.17g,%.17g],", solved_fv[0], solved_fv[1], solved_fv[2]);
    printf("\"solved_candidate_viscous_force_n\":[%.17g,%.17g,%.17g],", solved_candidate[0], solved_candidate[1], solved_candidate[2]);
    printf("\"prescribed_pressure_force_n\":[%.17g,%.17g,%.17g],", exact_fp[0], exact_fp[1], exact_fp[2]);
    printf("\"prescribed_viscous_force_n\":[%.17g,%.17g,%.17g],", exact_fv[0], exact_fv[1], exact_fv[2]);
    printf("\"prescribed_candidate_viscous_force_n\":[%.17g,%.17g,%.17g]}\n", exact_candidate[0], exact_candidate[1], exact_candidate[2]);
    return 0;
}
