/* Local physical polynomial stencil check, not a manufactured matrix RHS. */
#include "../src/app/cfd_obstacle3d_mixed.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    int n = atoi(argv[1]);
    assert(n == 8 || n == 16 || n == 32);
    int dims[3] = {2*n, n, n}, lo[3] = {3*n/4, n/4, n/4}, hi[3] = {5*n/4, 3*n/4, 3*n/4};
    double lengths[3] = {4, 2, 2}, h = 2./n, mu = .1;
    CfdMemoryBudget budget = {.limit_bytes = 512*1024*1024};
    CfdMemoryBudget *previous = cfd_memory_scope(&budget);
    CfdCartesian3d grid;
    assert(cfd_cartesian3d_init(&grid, dims, lengths));
    CfdObstacleMixed3d *s = cfd_obstacle_mixed3d_create(&grid, mu, lo, hi);
    assert(s && cfd_obstacle_mixed3d_verify(s));
    double *u = cfd_memory_calloc(s->count, sizeof(double));
    double *action = cfd_memory_calloc(s->count, sizeof(double));
    assert(u && action);
    int near = cfd_obstacle_mixed3d_index(s, 1, lo[0]-1, n/2, n/2);
    int inside = cfd_obstacle_mixed3d_index(s, 1, lo[0]-3, n/2, n/2);
    assert(near >= 0 && inside >= 0);
    double maximum_prediction_error = 0, maximum_interior_error = 0, maximum_linear_error = 0;
    double mean_defect = 0, point_defect = 0;
    for (int averages = 0; averages <= 1; averages++)
        for (int trial = 0; trial < 3; trial++) {
            double a = trial == 0 ? 1.2 : trial == 1 ? -.3 : .8;
            double b = trial == 0 ? -.7 : trial == 1 ? 1. : 0.;
            memset(u, 0, (size_t)s->count*sizeof(double));
            for (int q = 0; q < s->h[1].n; q++) {
                double x[3];
                cfd_obstacle_mixed3d_position(s, s->offset[1]+q, NULL, x, NULL);
                double distance = lo[0]*h-x[0];
                u[s->offset[1]+q] = a*distance+b*(distance*distance+(averages ? h*h/12 : 0));
            }
            apply(&s->h[1], u+s->offset[1], action+s->offset[1]);
            /* Independent continuum density -mu*d2u/dx2 = -2*mu*b. */
            double defect = action[near]/grid.volume+2*mu*b;
            double predicted = (averages ? 2./3 : .5)*mu*b;
            maximum_prediction_error = fmax(maximum_prediction_error, fabs(defect-predicted));
            maximum_interior_error = fmax(maximum_interior_error, fabs(action[inside]/grid.volume+2*mu*b));
            if (trial == 2)
                maximum_linear_error = fmax(maximum_linear_error, fabs(defect));
            if (trial == 1) {
                if (averages) mean_defect = defect;
                else point_defect = defect;
            }
        }
    cfd_memory_free(u); cfd_memory_free(action);
    cfd_obstacle_mixed3d_destroy(s);
    assert(budget.live_bytes == 0);
    cfd_memory_scope(previous);
    assert(maximum_prediction_error < 1e-12 && maximum_interior_error < 1e-12 && maximum_linear_error < 1e-12);
    printf("{\"n\":%d,\"local_polynomial_cases\":6,\"interval_mean_wall_density_defect\":%.17g,"
           "\"point_wall_density_defect\":%.17g,\"maximum_prediction_error\":%.17g,"
           "\"maximum_interior_error\":%.17g,\"maximum_linear_error\":%.17g,\"peak_owned_bytes\":%zu}\n",
           n,mean_defect,point_defect,maximum_prediction_error,maximum_interior_error,maximum_linear_error,budget.peak_bytes);
    return 0;
}
