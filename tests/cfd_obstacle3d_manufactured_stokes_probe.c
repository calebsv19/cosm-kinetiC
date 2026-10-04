/* Physical forcing from independently integrated smooth compact polynomials.
 * No native matrix action manufactures the right-hand side. */
#include "../src/app/cfd_obstacle3d_mixed.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static const double low[3] = {.25, .25, .25}, high[3] = {1.25, 1.75, 1.75};
static const double coeff[9] = {0, 0, 0, 0, 256, -1024, 1536, -1024, 256};
static double polynomial(double t, int derivative) {
    double answer = 0;
    for (int k = derivative; k <= 8; k++) {
        double factor = coeff[k];
        for (int j = 0; j < derivative; j++)
            factor *= k - j;
        answer += factor * pow(t, k - derivative);
    }
    return answer;
}
static double value(int axis, double x, int derivative) {
    double scale = high[axis] - low[axis];
    if (x <= low[axis] || x >= high[axis])
        return 0;
    return polynomial((x - low[axis]) / scale, derivative) / pow(scale, derivative);
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
        integral += coeff[k] * (pow(u, k + 1) - pow(l, k + 1)) / (k + 1);
    return scale * integral / (b - a);
}
static double started;
static double now(void) {
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
static bool bounded(void *context) {
    (void)context;
    return now() - started < 180;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    int n = atoi(argv[1]);
    assert(n == 8 || n == 16 || n == 32);
    started = now();
    int dims[3] = {2 * n, n, n}, lo[3] = {3 * n / 4, n / 4, n / 4};
    int hi[3] = {5 * n / 4, 3 * n / 4, 3 * n / 4};
    double lengths[3] = {4, 2, 2}, h = 2. / n, amplitude = .0001, pa = .001;
    CfdMemoryBudget budget = {.limit_bytes = 512 * 1024 * 1024};
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
    cfd_memory_free(rhs);cfd_memory_free(u);cfd_memory_free(p);
    cfd_obstacle_mixed3d_destroy(s);
    assert(budget.live_bytes == 0 && norm_u > 0 && norm_p > 0);
    cfd_memory_scope(old);
    printf("{\"n\":%d,\"velocity_relative_error\":%.17g,\"pressure_relative_error\":%.17g,\"maximum_divergence\":%.17g,\"momentum_residual\":%.17g,\"iterations\":%d,\"wall_s\":%.17g,\"peak_owned_bytes\":%zu}\n",n,sqrt(error_u/norm_u),sqrt(error_p/norm_p),maxdiv,residual,iterations,now()-started,budget.peak_bytes);
    return 0;
}
