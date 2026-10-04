#include "app/cfd_mac2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static double analytic(int n) {
    CfdMac2D c;
    double dims[3] = {2, 1, .5}, pi = acos(-1.), force, shifted;
    assert(cfd_mac2d_init(&c, n, n, dims, 1, .1, 0, 0, 0, 0));
    assert(cfd_mac2d_set_obstacle(&c, n / 4, n / 4, n / 2, 3 * n / 4));
    for (int k = 0; k < n * n; k++)
        c.p[k] = .2 * (k % n + .5) * 2 / n;
    assert(cfd_mac2d_surface_pressure(&c, &force));
    double expected = -.2 * .5 * .5 * .5;
    assert(fabs(force - expected) < 1e-12);
    for (int k = 0; k < n * n; k++)
        c.p[k] += 123;
    assert(cfd_mac2d_surface_pressure(&c, &shifted));
    assert(fabs(force - shifted) < 1e-12);
    for (int k = 0; k < n * n; k++)
        c.p[k] = cos(pi * (k % n + .5) * 2 / n);
    assert(cfd_mac2d_surface_pressure(&c, &force));
    expected = .25 * (cos(pi * .5) - cos(pi));
    double error = fabs(force - expected);
    printf("smooth n=%d surface_pressure_error_N=%.12g\n", n, error);
    cfd_mac2d_destroy(&c);
    return error;
}
static void baffle(int n, double drive, double rho, double dt) {
    CfdMac2D c;
    double dims[3] = {2, 1, .5};
    assert(cfd_mac2d_init(&c, n, n, dims, rho, .1, drive, 0, 0, 0));
    assert(cfd_mac2d_set_obstacle(&c, n / 4, 0, n / 2, n));
    for (int t = 0; t < 3; t++)
        assert(cfd_mac2d_step(&c, dt));
    double expected = drive * 1.5 * .5; // independent geometric fluid volume
    assert(c.surface_pressure_valid);
    assert(fabs(c.continuum_drive_force_x_n - expected) < 1e-12);
    assert(fabs(c.obstacle_mean_surface_pressure_force_x_n - expected) < 1e-9);
    assert(fabs(c.obstacle_mean_surface_pressure_force_x_n - c.obstacle_mean_pressure_force_x_n -
                c.unresolved_drive_force_x_n) < 1e-9);
    assert(fabs(c.momentum_residual) < 1e-9);
    double speed = 0;
    for (int k = 0; k < n * n; k++)
        speed = fmax(speed, fabs(c.u[k]));
    assert(speed < 1e-8);
    printf("baffle n=%d G=%g rho=%g dt=%g discrete_N=%.12g surface_N=%.12g expected_N=%.12g "
           "unresolved_drive_N=%.12g\n",
           n, drive, rho, dt, c.obstacle_mean_pressure_force_x_n,
           c.obstacle_mean_surface_pressure_force_x_n, expected, c.unresolved_drive_force_x_n);
    cfd_mac2d_destroy(&c);
}
int main(void) {
    double previous = 0;
    for (int n = 8; n <= 64; n *= 2) {
        double error = analytic(n);
        if (previous)
            assert(previous / error > 3.0 && previous / error < 5.0);
        previous = error;
        baffle(n, .1, 1, .005);
        baffle(n, -.1, 2, .0025);
    }
    puts("Boundary surface-pressure reconstruction passed; general drag remains unqualified.");
}
