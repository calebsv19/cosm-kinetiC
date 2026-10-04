#include "app/cfd_mac2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static int ix(int n, int x, int y) { return y * n + (x + n) % n; }
static int fluid(CfdMac2D *c, int x, int y) {
    return y >= 0 && y < c->ny && !c->solid[ix(c->nx, x, y)];
}
static double momentum(CfdMac2D *c) {
    double s = 0;
    for (int k = 0; k < c->nx * c->ny; k++)
        s += c->u[k];
    return s * c->parameters.rho * c->parameters.length * c->parameters.height *
           c->parameters.width / (c->nx * c->ny);
}
static void run(int n, int full_height) {
    CfdMac2D c;
    double dims[3] = {2, 1, .5};
    double dt = .01, pi = acos(-1.);
    assert(cfd_mac2d_init(&c, n, n, dims, 2, .1, 0, 0, 0, 0));
    assert(cfd_mac2d_set_obstacle(&c, n / 4, full_height ? 0 : n / 4, n / 2,
                                  full_height ? n : 3 * n / 4));

    double exact[4096], mean = 0;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            int k = ix(n, i, j);
            exact[k] = fluid(&c, i, j)
                           ? cos(2 * pi * (i + .5) / n) * (1 + .3 * cos(2 * pi * (j + .5) / n))
                           : 0;
            mean += exact[k];
        }
    mean /= c.fluid_cells;
    for (int k = 0; k < n * n; k++)
        if (!c.solid[k])
            exact[k] -= mean;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            if (fluid(&c, i, j) && fluid(&c, i - 1, j))
                c.u[ix(n, i, j)] =
                    dt / 2 * (exact[ix(n, i, j)] - exact[ix(n, i - 1, j)]) / (2. / n);
    for (int j = 1; j < n; j++)
        for (int i = 0; i < n; i++)
            if (fluid(&c, i, j) && fluid(&c, i, j - 1))
                c.v[ix(n, i, j)] =
                    dt / 2 * (exact[ix(n, i, j)] - exact[ix(n, i, j - 1)]) / (1. / n);
    double before = momentum(&c);
    assert(cfd_mac2d_project(&c, dt));
    double error = 0, leak = 0;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            int k = ix(n, i, j);
            error = fmax(error, fabs(c.p[k] - exact[k]));
            if (!fluid(&c, i, j) || !fluid(&c, i - 1, j))
                leak = fmax(leak, fabs(c.u[k]));
            if (!fluid(&c, i, j) || !fluid(&c, i, j - 1))
                leak = fmax(leak, fabs(c.v[k]));
        }
    double expected = 0, shifted = 0;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            if (fluid(&c, i, j)) {
                double area = .5 / n;
                if (!fluid(&c, i + 1, j)) {
                    expected += exact[ix(n, i, j)] * area;
                    shifted += (exact[ix(n, i, j)] + 17) * area;
                }
                if (!fluid(&c, i - 1, j)) {
                    expected -= exact[ix(n, i, j)] * area;
                    shifted -= (exact[ix(n, i, j)] + 17) * area;
                }
            }
    assert(fabs(expected - shifted) < 1e-12);
    assert(fabs(expected - c.obstacle_pressure_force_x_n) < 1e-9);
    double balance = (momentum(&c) - before) / dt + c.obstacle_pressure_force_x_n;
    assert(error < 1e-7 && leak == 0 && fabs(balance) < 1e-9);
    assert(fabs(c.obstacle_pressure_force_x_n) > .01);
    assert(c.divergence_after < 1e-8 && c.projection_energy_change <= 1e-10);
    printf("n=%d full_height=%d pressure_error=%.12g leak=%.12g body_pressure_N=%.12g "
           "momentum_balance_N=%.12g\n",
           n, full_height, error, leak, c.obstacle_pressure_force_x_n, balance);
    assert(!cfd_mac2d_set_obstacle(&c, 1, 1, 2, 2));
    c.u[ix(n, n / 4, full_height ? 0 : n / 4)] = 1;
    assert(!cfd_mac2d_project(&c, dt));
    cfd_mac2d_destroy(&c);
}
int main(void) {
    for (int n = 8; n <= 32; n *= 2) {
        run(n, 0);
        run(n, 1);
    }
    puts("Masked obstacle projection passed; open outlets remain unsupported.");
}
