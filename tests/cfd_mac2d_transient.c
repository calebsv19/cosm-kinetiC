/* Nonuniform no-slip transient, continuous initial streamfunction.
 * Output supports nested-grid and timestep self-convergence; no analytical
 * solution is asserted for the nonlinear evolution. */
#include "app/cfd_mac2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double amplitude = .01;
static double psi(double x, double y) {
    const double pi = 3.14159265358979323846;
    return amplitude * sin(pi * x) * pow(sin(pi * y), 2);
}
int main(int argc, char **argv) {
    assert(argc == 4);
    amplitude = atof(argv[3]);
    assert(amplitude > 0 && amplitude <= .03);
    int n = atoi(argv[1]), steps = atoi(argv[2]);
    assert(n >= 4 && n <= 64 && steps > 0);
    double dims[3] = {2, 1, .5}, dt = .1 / steps, dx = 2.0 / n, dy = 1.0 / n;
    CfdMac2D c;
    assert(cfd_mac2d_init(&c, n, n, dims, 1, .01, 0, 0, 0, 0));
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            c.u[j * n + i] = (psi(i * dx, (j + 1) * dy) - psi(i * dx, j * dy)) / dy;
    for (int j = 1; j < n; j++)
        for (int i = 0; i < n; i++)
            c.v[j * n + i] = -(psi((i + 1) * dx, j * dy) - psi(i * dx, j * dy)) / dx;
    double div = 0, momentum = 0;
    int substeps = 0;
    for (int t = 0; t < steps; t++) {
        assert(cfd_mac2d_step(&c, dt));
        div = fmax(div, cfd_mac2d_divergence(&c));
        momentum = fmax(momentum, fabs(c.momentum_residual));
        substeps += c.substeps;
    }
    assert(div < 1e-8 && momentum < 1e-9);
    printf("%d %.17g %.17g %.17g %d\n", n, dt, div, momentum, substeps);
    for (int k = 0; k < n * n; k++)
        printf("%.17g\n", c.u[k]);
    for (int k = 0; k < n * (n + 1); k++)
        printf("%.17g\n", c.v[k]);
    for (int k = 0; k < n * n; k++)
        printf("%.17g\n", c.p[k]);
    cfd_mac2d_destroy(&c);
}
