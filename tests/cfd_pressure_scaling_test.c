#include "app/cfd_open2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
bool plain_init(CfdOpen2D *, int, int, double, double, double, double, double, double);
bool plain_obstacle(CfdOpen2D *, int, int, int, int);
bool plain_step(CfdOpen2D *, double);
void plain_destroy(CfdOpen2D *);
static double now(void) {
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return t.tv_sec + 1e-9 * t.tv_nsec;
}
static void run(int n) {
    CfdOpen2D mg, cg;
    assert(cfd_open2d_init(&mg, n, n, 4, 2, .5, 1, .1, .002));
    assert(plain_init(&cg, n, n, 4, 2, .5, 1, .1, .002));
    assert(cfd_open2d_set_obstacle(&mg, 3 * n / 8, 3 * n / 8, 5 * n / 8, 5 * n / 8));
    assert(plain_obstacle(&cg, 3 * n / 8, 3 * n / 8, 5 * n / 8, 5 * n / 8));
    double dt = .001 * 64 * 64 / (n * n), mt = 0, ct = 0, error = 0, perror = 0;
    int mi = 0, ci = 0;
    for (int t = 0; t < 80; t++) {
        double start = now();
        assert(cfd_open2d_step(&mg, dt));
        mt += now() - start;
        mi += mg.iterations;
        start = now();
        assert(plain_step(&cg, dt));
        ct += now() - start;
        ci += cg.iterations;
        for (int k = 0; k < n * (n + 1); k++) {
            error = fmax(error, fabs(mg.u[k] - cg.u[k]));
            error = fmax(error, fabs(mg.v[k] - cg.v[k]));
        }
        for (int k = 0; k < n * n; k++)
            perror = fmax(perror, fabs(mg.p[k] - cg.p[k]));
    }
    printf("n=%d mg_iterations=%d cg_iterations=%d mg_seconds=%.9g cg_seconds=%.9g speedup=%.6g "
           "max_velocity_difference=%.12g max_pressure_difference=%.12g\n",
           n, mi, ci, mt, ct, ct / mt, error, perror);
    fflush(stdout);
    assert(error < 1e-9 && perror < 1e-7);
    assert(mi < ci);
    cfd_open2d_destroy(&mg);
    plain_destroy(&cg);
}
int main(void) {
    run(16);
    run(32);
    run(64);
    run(128);
}
