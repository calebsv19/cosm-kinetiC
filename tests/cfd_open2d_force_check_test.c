#include "app/cfd_open2d_force_check.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
    CfdOpen2D c;
    int n = 32;
    assert(cfd_open2d_init(&c, n, n, 4, 2, .5, 1, .1, .002));
    assert(cfd_open2d_set_obstacle(&c, 12, 12, 20, 20));
    for (int j = 0; j < n; j++) {
        double y = (j + .5) * 2 / n, d = y > 1.25 ? y - 1.25 : y < .75 ? y - .75 : 0;
        double velocity = (y > 1.25 ? 3 : 1) * d + .7 * d * d + .2 * d * d * d;
        for (int i = 0; i <= n; i++)
            c.u[j * (n + 1) + i] = velocity;
        for (int i = 0; i < n; i++) {
            double x = (i + .5) * 4 / n;
            c.p[j * n + i] = x < 1.5 ? x * x : 0;
        }
    }
    CfdMac2DForceCheck result;
    assert(cfd_open2d_force_check(&c, 4, 4, 28, 28, &result));
    assert(fabs(result.surface_pressure_x_n - 2.25 * .5 * .5) < 1e-12);
    assert(fabs(result.surface_viscous_x_n - .1 * (3 - 1) * 1 * .5) < 1e-12);
    cfd_open2d_destroy(&c);
    puts("Open force reconstruction matches exact quadratic pressure and cubic wall shear");
}
