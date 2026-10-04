#include "app/cfd_mac2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static const double pi = 3.14159265358979323846;
static void pressure_recovery(double rho, double dt) {
    CfdMac2D c;
    double dims[3] = {2, 1, .5};
    int nx = 16, ny = 12;
    double exact[192];
    assert(cfd_mac2d_init(&c, nx, ny, dims, rho, rho * .1, 0, .3, .3, 0));
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++)
            exact[j * nx + i] = .4 * cos(2 * pi * (i + .5) / nx) * cos(pi * (j + .5) / ny);
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++)
            c.u[j * nx + i] = .3 + dt / rho *
                                       (exact[j * nx + i] - exact[j * nx + (i + nx - 1) % nx]) /
                                       (2.0 / nx);
    for (int j = 1; j < ny; j++)
        for (int i = 0; i < nx; i++)
            c.v[j * nx + i] = dt / rho * (exact[j * nx + i] - exact[(j - 1) * nx + i]) / (1.0 / ny);
    assert(cfd_mac2d_divergence(&c) > 1e-3);
    assert(cfd_mac2d_project(&c, dt));
    double error = 0;
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++) {
            int k = j * nx + i;
            error = fmax(error, fabs(c.p[k] - exact[k]));
            assert(fabs(c.u[k] - .3) < 1e-10);
            double div = (c.u[j * nx + (i + 1) % nx] - c.u[k]) / (2.0 / nx) +
                         (c.v[(j + 1) * nx + i] - c.v[k]) / (1.0 / ny);
            assert(fabs(div) < 1e-9);
        }
    for (int i = 0; i < nx * (ny + 1); i++)
        assert(fabs(c.v[i]) < 1e-10);
    assert(error < 1e-9);
    assert(c.projection_energy_change <= 1e-12);
    printf("pressure rho=%g dt=%g Pa_error=%g divergence=%g iterations=%d "
           "projection_energy_change=%g\n",
           rho, dt, error, c.divergence_after, c.pressure_iterations, c.projection_energy_change);
    cfd_mac2d_destroy(&c);
}
static double channel(int n, double g, double bottom, double top) {
    CfdMac2D c;
    double dims[3] = {2, 1, .5};
    assert(cfd_mac2d_init(&c, 4, n, dims, 1, .1, g, bottom, top, 0));
    for (int t = 0; t < 800; t++) {
        assert(cfd_mac2d_step(&c, .05));
        assert(fabs(c.momentum_residual) < 1e-10);
        assert(c.divergence_after < 1e-10);
    }
    double error = 0;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < 4; i++) {
            double y = (j + .5) / n, exact = bottom + (top - bottom) * y + g * y * (1 - y) / .2;
            error = fmax(error, fabs(c.u[j * 4 + i] - exact));
        }
    double shear_bottom = 2 * .1 * (c.u[0] - bottom) * n,
           shear_top = 2 * .1 * (top - c.u[(n - 1) * 4]) * n;
    assert(fabs(shear_bottom - (.1 * (top - bottom) + g / 2)) < 1e-10);
    assert(fabs(shear_top - (.1 * (top - bottom) - g / 2)) < 1e-10);
    printf("MAC channel n=%d G=%g walls=%g,%g error=%g shear=%g,%g\n", n, g, bottom, top, error,
           shear_bottom, shear_top);
    cfd_mac2d_destroy(&c);
    return error;
}
static double continuum_pressure(int n) {
    CfdMac2D c;
    double dims[3] = {2, 1, .5}, dt = .02;
    assert(cfd_mac2d_init(&c, n, n, dims, 1, .1, 0, 0, 0, 0));
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            c.u[j * n + i] = dt * (-.4 * pi * sin(2 * pi * i / n) * cos(pi * (j + .5) / n));
    for (int j = 1; j < n; j++)
        for (int i = 0; i < n; i++)
            c.v[j * n + i] = dt * (-.4 * pi * cos(2 * pi * (i + .5) / n) * sin(pi * j / n));
    assert(cfd_mac2d_project(&c, dt));
    double error = 0;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            error +=
                pow(c.p[j * n + i] - .4 * cos(2 * pi * (i + .5) / n) * cos(pi * (j + .5) / n), 2) /
                (n * n);
    error = sqrt(error);
    printf("continuum pressure n=%d L2_Pa_error=%g\n", n, error);
    cfd_mac2d_destroy(&c);
    return error;
}
static void solenoidal_preservation(void) {
    CfdMac2D c;
    double dims[3] = {2, 1, .5};
    int nx = 12, ny = 10;
    double psi[11][13], old_u[120], old_v[132];
    assert(cfd_mac2d_init(&c, nx, ny, dims, 1, .1, 0, 0, 0, 0));
    for (int j = 0; j <= ny; j++)
        for (int i = 0; i <= nx; i++)
            psi[j][i] = .01 * sin(2 * pi * i / nx) * pow(sin(pi * j / ny), 2);
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++)
            c.u[j * nx + i] = (psi[j + 1][i] - psi[j][i]) * ny;
    for (int j = 1; j < ny; j++)
        for (int i = 0; i < nx; i++)
            c.v[j * nx + i] = -(psi[j][i + 1] - psi[j][i]) * nx / 2;
    for (int i = 0; i < 120; i++)
        old_u[i] = c.u[i];
    for (int i = 0; i < 132; i++)
        old_v[i] = c.v[i];
    assert(cfd_mac2d_divergence(&c) < 1e-12);
    assert(cfd_mac2d_project(&c, .01));
    for (int i = 0; i < 120; i++)
        assert(fabs(old_u[i] - c.u[i]) < 1e-12);
    for (int i = 0; i < 132; i++)
        assert(fabs(old_v[i] - c.v[i]) < 1e-12);
    assert(c.pressure_iterations == 0);
    cfd_mac2d_destroy(&c);
    puts("nonuniform solenoidal velocity preserved by projection");
}
int main(void) {
    solenoidal_preservation();
    double p8 = continuum_pressure(8), p16 = continuum_pressure(16), p32 = continuum_pressure(32);
    assert(p8 / p16 > 3.8 && p16 / p32 > 3.8);
    pressure_recovery(1, .01);
    pressure_recovery(7, .025);
    double a = channel(8, .1, 0, 0), b = channel(16, .1, 0, 0), c = channel(32, .1, 0, 0);
    assert(a / b > 3.9 && b / c > 3.9);
    assert(channel(16, 0, -.2, .5) < 1e-10);
    channel(16, -.1, 0, 0);
    channel(16, .1, -.2, .5);
    CfdMac2D v;
    double dims[3] = {2, 1, .5};
    assert(cfd_mac2d_init(&v, 16, 16, dims, 1, .1, 0, 0, 0, .1));
    double initial = 0;
    for (int i = 0; i < 256; i++)
        initial += v.u[i] * v.u[i];
    int iterations = 0;
    for (int t = 0; t < 30; t++) {
        assert(cfd_mac2d_step(&v, .01));
        assert(v.divergence_after < 1e-8);
        assert(v.projection_energy_change < 1e-10);
        assert(fabs(v.momentum_residual) < 1e-10);
        iterations += v.pressure_iterations;
    }
    double remaining = 0;
    for (int i = 0; i < 256; i++)
        remaining += v.u[i] * v.u[i];
    for (int i = 0; i < 272; i++)
        remaining += v.v[i] * v.v[i];
    assert(iterations > 0 && remaining < initial);
    printf("2D perturbation energy ratio=%g iterations=%d\n", remaining / initial, iterations);
    cfd_mac2d_destroy(&v);
    assert(!cfd_mac2d_init(&v, 128, 16, dims, 1, .1, 0, 0, 0, 0));
    puts("MAC 2D verification passed");
}
