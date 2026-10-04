/* Known physical cell averages versus the native pressure action. Including the
 * unchanged implementation exposes its static transpose only in this probe. */
#include "../src/app/cfd_obstacle3d_mixed.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static double mean_power(double a, double b, int k) {
    return (pow(b, k + 1) - pow(a, k + 1)) / ((k + 1) * (b - a));
}
int main(int argc, char **argv) {
    assert(argc == 2);
    int n = atoi(argv[1]);
    assert(n == 8 || n == 16 || n == 32);
    int dims[3] = {2 * n, n, n}, lo[3] = {3 * n / 4, n / 4, n / 4};
    int hi[3] = {5 * n / 4, 3 * n / 4, 3 * n / 4};
    double lengths[3] = {4, 2, 2}, h = 2. / n;
    CfdMemoryBudget budget = {.limit_bytes = 512 * 1024 * 1024};
    CfdMemoryBudget *previous = cfd_memory_scope(&budget);
    CfdCartesian3d grid;
    assert(cfd_cartesian3d_init(&grid, dims, lengths));
    CfdObstacleMixed3d *s = cfd_obstacle_mixed3d_create(&grid, .1, lo, hi);
    assert(s && cfd_obstacle_mixed3d_verify(s));
    double *p = s->pwork, *action = s->vwork;
    double quadratic_error = 0, cubic_error = 0, predicted_error = 0;
    int polynomials = 0, interior_faces = 0;
    for (int px = 0; px <= 3; px++)
        for (int py = 0; py <= 3 - px; py++)
            for (int pz = 0; pz <= 3 - px - py; pz++) {
                int powers[3] = {px, py, pz};
                polynomials++;
                for (int k = 0; k < n; k++)
                    for (int j = 0; j < n; j++)
                        for (int i = 0; i < 2 * n; i++) {
                            int cell = cfd_obstacle_mixed3d_cell(s, i, j, k);
                            if (cell < 0)
                                continue;
                            int c[3] = {i, j, k};
                            p[cell] = 1;
                            for (int a = 0; a < 3; a++)
                                p[cell] *= mean_power(c[a] * h, (c[a] + 1) * h, powers[a]);
                        }
                transpose(s, p, action);
                for (int q = 0; q < s->count; q++) {
                    int a, c[3], lower[3];
                    double x[3];
                    cfd_obstacle_mixed3d_position(s, q, &a, x, c);
                    memcpy(lower, c, sizeof(lower));
                    lower[a]--;
                    if (fluid(s, c) < 0 || fluid(s, lower) < 0)
                        continue;
                    interior_faces++;
                    double exact = -powers[a];
                    if (powers[a])
                        for (int b = 0; b < 3; b++)
                            exact *= mean_power(x[b] - .5 * h, x[b] + .5 * h,
                                                powers[b] - (a == b));
                    double error = action[q] / grid.volume - exact;
                    double predicted = powers[a] == 3 ? -h * h / 4 : 0;
                    predicted_error = fmax(predicted_error, fabs(error - predicted));
                    if (px + py + pz <= 2)
                        quadratic_error = fmax(quadratic_error, fabs(error));
                    cubic_error = fmax(cubic_error, fabs(error));
                }
            }
    assert(polynomials == 20 && interior_faces > 0);
    assert(quadratic_error < 1e-10 && predicted_error < 1e-10);
    assert(fabs(cubic_error - h * h / 4) < 1e-10);
    /* Constant pressure has zero interior gradient. On open ends it is the
     * pressure traction load; no null global pressure mode is assumed there. */
    for (int q = 0; q < s->cells; q++)
        p[q] = 3.2;
    transpose(s, p, action);
    double constant_error = 0;
    for (int q = 0; q < s->count; q++) {
        int a, c[3];
        double x[3], exact = 0;
        cfd_obstacle_mixed3d_position(s, q, &a, x, c);
        if (a == 0 && c[0] == 0)
            exact = -3.2 * grid.area[0];
        if (a == 0 && c[0] == dims[0])
            exact = 3.2 * grid.area[0];
        constant_error = fmax(constant_error, fabs(action[q] - exact));
    }
    assert(constant_error < 1e-12);
    cfd_obstacle_mixed3d_destroy(s);
    assert(budget.live_bytes == 0);
    cfd_memory_scope(previous);
    printf("{\"n\":%d,\"polynomials\":%d,\"quadratic_error\":%.17g,"
           "\"cubic_dual_volume_gradient_error\":%.17g,\"predicted_bias_error\":%.17g,"
           "\"constant_boundary_load_error\":%.17g,\"peak_owned_bytes\":%zu}\n",
           n, polynomials, quadratic_error, cubic_error, predicted_error,
           constant_error, budget.peak_bytes);
    return 0;
}
