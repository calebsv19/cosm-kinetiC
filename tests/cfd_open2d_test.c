#include "app/cfd_open2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void backflow_probe(void) {
    CfdOpen2D c;
    int n = 24;
    assert(cfd_open2d_init(&c, n, n, 2, 1, .5, 1, .1, .05));
    double pi = acos(-1.), initial_reverse = 0, max_reverse = 0;
    /* Streamfunction pulse touches the outlet, retains impermeable Y walls.
     * This is a stability/flux test, not an exact backflow solution. */
    for (int j = 0; j < n; j++)
        for (int i = 1; i <= n; i++) {
            double x = 2. * i / n, y = (j + .5) / n;
            c.u[j * (n + 1) + i] += .12 * exp(-pow((x - 2) / .25, 2)) * pi * sin(2 * pi * y);
        }
    for (int j = 1; j < n; j++)
        for (int i = 0; i < n; i++) {
            double x = 2. * (i + .5) / n, y = (double)j / n;
            c.v[j * n + i] =
                .12 * 2 * (x - 2) / (.25 * .25) * exp(-pow((x - 2) / .25, 2)) * pow(sin(pi * y), 2);
        }
    for (int j = 0; j < n; j++)
        initial_reverse += fmax(0, -c.u[j * (n + 1) + n]) / n * .5;
    assert(initial_reverse > 1e-3);
    for (int t = 0; t < 1000; t++) {
        assert(cfd_open2d_step(&c, .0005));
        max_reverse = fmax(max_reverse, c.outlet_backflow);
        assert(fabs(c.inlet_flux - c.outlet_flux) < 1e-8);
    }
    assert(max_reverse > 1e-4);
    printf("backflow initial=%.12g max_after_projection=%.12g final=%.12g divergence=%.12g\n",
           initial_reverse, max_reverse, c.outlet_backflow, c.divergence);
    cfd_open2d_destroy(&c);
}
int main(void) {
    backflow_probe();
    for (int n = 8; n <= 32; n *= 2)
        for (int length = 2; length <= 4; length *= 2) {
            CfdOpen2D c;
            assert(cfd_open2d_init(&c, n, n, length, 1, .5, 1, .1, .05));
            double dt = .0005;
            for (int t = 0; t < 2000; t++)
                assert(cfd_open2d_step(&c, dt));
            double error = 0;
            for (int j = 0; j < n; j++)
                for (int i = 0; i <= n; i++) {
                    double y = (j + .5) / n;
                    error = fmax(error, fabs(c.u[j * (n + 1) + i] - 6 * .05 * y * (1 - y)));
                }
            printf("n=%d L=%d pressure=%.12g reference=%.12g velocity_error=%.12g "
                   "mass_mismatch=%.12g div=%.12g\n",
                   n, length, c.inlet_pressure, 12 * .1 * .05 * length, error,
                   c.inlet_flux - c.outlet_flux, c.divergence);
            fflush(stdout);
            assert(fabs(c.inlet_flux - c.outlet_flux) < 1e-8 && c.divergence < 1e-8);
            /* Quadratic wall diffusion must retain the exact supplied steady
             * parabola and physical pressure drop, not just approach it slowly. */
            assert(error < 1e-9);
            assert(fabs(c.inlet_pressure - 12 * .1 * .05 * length) < 1e-8);
            if (n == 32) {
                assert(fabs(c.inlet_pressure - 12 * .1 * .05 * length) / (12 * .1 * .05 * length) <
                       .01);
                assert(error / (1.5 * .05) < .005);
            }
            cfd_open2d_destroy(&c);
        }
}
