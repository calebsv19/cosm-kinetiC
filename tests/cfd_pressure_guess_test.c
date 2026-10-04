#include "app/cfd_open2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
bool cold_init(CfdOpen2D *, int, int, double, double, double, double, double, double);
bool cold_obstacle(CfdOpen2D *, int, int, int, int);
bool cold_step(CfdOpen2D *, double);
void cold_destroy(CfdOpen2D *);
int main(void) {
    CfdOpen2D warm, cold;
    assert(cfd_open2d_init(&warm, 16, 16, 4, 2, .5, .7, .12, .01));
    assert(cold_init(&cold, 16, 16, 4, 2, .5, .7, .12, .01));
    assert(cfd_open2d_set_obstacle(&warm, 6, 6, 10, 10));
    assert(cold_obstacle(&cold, 6, 6, 10, 10));
    int wi = 0, ci = 0;
    double error = 0;
    for (int t = 0; t < 1000; t++) {
        double dt = t < 500 ? .001 : .0005;
        assert(cfd_open2d_step(&warm, dt));
        assert(cold_step(&cold, dt));
        wi += warm.iterations;
        ci += cold.iterations;
        for (int k = 0; k < 16 * 17; k++) {
            error = fmax(error, fabs(warm.u[k] - cold.u[k]));
            error = fmax(error, fabs(warm.v[k] - cold.v[k]));
        }
        for (int k = 0; k < 16 * 16; k++)
            assert(fabs(warm.p[k] - cold.p[k]) < 1e-7);
    }
    assert(error < 1e-9);
    assert(wi < ci);
    printf("warm_iterations=%d cold_iterations=%d max_velocity_difference=%.12g\n", wi, ci, error);
    warm.p[0] = NAN;
    assert(!cfd_open2d_step(&warm, .001));
    cfd_open2d_destroy(&warm);
    cold_destroy(&cold);
}
