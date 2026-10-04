#include "app/cfd_obstacle3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* A full quadratic pressure with cross terms has known volume averages and
 * closed body force. No velocity solve or reference drag enters this test. */
static double polynomial(const double x[3], double gauge) {
    return gauge + .03 + .004 * x[0] + .003 * x[0] * x[0] + .002 * x[1] + .001 * x[1] * x[1] -
           .001 * x[2] + .0007 * x[2] * x[2] + .0011 * x[0] * x[1] + .0009 * x[0] * x[2] +
           .0005 * x[1] * x[2];
}
int main(int argc, char **argv) {
    assert(argc == 3 || argc == 4);
    bool cusp = argc == 4;
    int n = atoi(argv[1]);
    double length = atof(argv[2]), cx = length / 2;
    int grid[3] = {(int)(length * n / 2), n, n};
    double lengths[3] = {length, 2, 2};
    CfdMemoryBudget budget = {.limit_bytes = 512 * 1024 * 1024};
    CfdMemoryBudget *previous = cfd_memory_scope(&budget);
    CfdObstacle3d state;
    assert(cfd_obstacle3d_init(&state, grid, lengths, 1, .1, .008, cx));
    double exact[3] = {-.004 - .006 * cx - .0011 - .0009, -.002 - .002 - .0011 * cx - .0005,
                       .001 - .0014 - .0009 * cx - .0005};
    if (cusp) {
        exact[0] = .015;
        exact[1] = exact[2] = 0;
    }
    double maximum = 0, gauge_error = 0, original[3] = {0};
    for (int trial = 0; trial < 2; trial++) {
        for (int k = 0; k < n; k++)
            for (int j = 0; j < n; j++)
                for (int i = 0; i < grid[0]; i++) {
                    int q = cfd_obstacle_mixed3d_cell(state.mixed, i, j, k);
                    if (q < 0)
                        continue;
                    double h = 2. / n, x[3] = {(i + .5) * h, (j + .5) * h, (k + .5) * h};
                    state.p[q] =
                        polynomial(x, trial ? 3.2 : 0) + (.003 + .001 + .0007) * h * h / 12;
                    if (cusp) {
                        double near = cx - .5, far = cx + .5;
                        if (x[0] < near) {
                            double a = near - (i + 1) * h, b = near - i * h;
                            state.p[q] = .02 - .002 *
                                                   (pow(fmax(0, b), 1.5) - pow(fmax(0, a), 1.5)) /
                                                   (1.5 * h);
                        } else if (x[0] > far) {
                            double a = i * h - far, b = (i + 1) * h - far;
                            state.p[q] = .005 + .002 *
                                                    (pow(fmax(0, b), 1.5) - pow(fmax(0, a), 1.5)) /
                                                    (1.5 * h);
                        } else {
                            state.p[q] = .02 - .015 * (x[0] - near);
                        }
                        state.p[q] += trial ? 3.2 : 0;
                    }
                }
        cfd_obstacle3d_measure(&state, state.u, state.p);
        for (int a = 0; a < 3; a++) {
            maximum = fmax(maximum, fabs(state.pressure_force[a] - exact[a]));
            if (!trial)
                original[a] = state.pressure_force[a];
            else
                gauge_error = fmax(gauge_error, fabs(state.pressure_force[a] - original[a]));
        }
    }
    printf("{\"n\":%d,\"length\":%.17g,\"force_n\":[%.17g,%.17g,%.17g],"
           "\"exact_force_n\":[%.17g,%.17g,%.17g],\"maximum_abs_error_n\":%.17g,"
           "\"gauge_error_n\":%.17g,\"numerical_peak_bytes\":%zu}\n",
           n, length, original[0], original[1], original[2], exact[0], exact[1], exact[2], maximum,
           gauge_error, budget.peak_bytes);
    assert((cusp || maximum < 1e-11) && gauge_error < 1e-11);
    cfd_obstacle3d_destroy(&state);
    assert(budget.live_bytes == 0);
    cfd_memory_scope(previous);
}
