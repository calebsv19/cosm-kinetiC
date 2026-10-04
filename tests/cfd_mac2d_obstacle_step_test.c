#include "app/cfd_mac2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static int idx(int n, int i, int j) { return j * n + (i + n) % n; }
static int fluid(CfdMac2D *c, int i, int j) {
    return j >= 0 && j < c->ny && !c->solid[idx(c->nx, i, j)];
}
static double momentum(CfdMac2D *c) {
    double sum = 0;
    /* Independent cell-centred integral, not the solver's face sum. */
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++)
            if (fluid(c, i, j))
                sum += .5 * (c->u[idx(c->nx, i, j)] + c->u[idx(c->nx, i + 1, j)]);
    return sum * c->parameters.rho * c->parameters.length * c->parameters.height *
           c->parameters.width / (c->nx * c->ny);
}
static double energy(CfdMac2D *c) {
    double e = 0;
    for (int k = 0; k < c->nx * c->ny; k++)
        e += c->u[k] * c->u[k];
    for (int k = 0; k < c->nx * (c->ny + 1); k++)
        e += c->v[k] * c->v[k];
    return e;
}
static void run(int n, double drive, int baffle, double dt) {
    CfdMac2D c;
    double dims[3] = {2, 1, .5};
    assert(cfd_mac2d_init(&c, n, n, dims, 1, .1, drive, 0, 0, 0));
    assert(cfd_mac2d_set_obstacle(&c, n / 4, baffle ? 0 : n / 4, n / 2, baffle ? n : 3 * n / 4));
    double residual = 0, div = 0, leak = 0, speed = 0;
    for (int tick = 0; tick < (int)lround(2 / dt); tick++) {
        double before = momentum(&c);
        assert(cfd_mac2d_step(&c, dt));
        double force = c.mean_drive_force_x_n + c.outer_wall_force_on_fluid_x_n -
                       c.obstacle_viscous_force_x_n - c.obstacle_mean_pressure_force_x_n;
        residual = fmax(residual, fabs((momentum(&c) - before) / dt - force));
        div = fmax(div, cfd_mac2d_divergence(&c));
        assert(fabs(c.momentum_residual) < 1e-9);
        for (int j = 0; j < n; j++)
            for (int i = 0; i < n; i++) {
                if (!fluid(&c, i, j) || !fluid(&c, i - 1, j))
                    leak = fmax(leak, fabs(c.u[idx(n, i, j)]));
                if (!fluid(&c, i, j) || !fluid(&c, i, j - 1))
                    leak = fmax(leak, fabs(c.v[idx(n, i, j)]));
                speed = fmax(speed, hypot(c.u[idx(n, i, j)], c.v[idx(n, i, j)]));
            }
        assert(isfinite(speed) && speed < 1);
    }
    assert(residual < 1e-9 && div < 1e-8 && leak == 0);
    if (drive == 0)
        assert(speed == 0);
    if (drive != 0 && !baffle) {
        assert(speed > 1e-4);
        assert(drive * c.obstacle_viscous_force_x_n > 0);
    }
    if (baffle)
        assert(speed < 1e-8);
    double sample[11];
    cfd_mac2d_values(&c, .75, .5, sample);
    assert(sample[2] == 1 && sample[0] == 0);
    printf("n=%d drive=%g baffle=%d dt=%g residual_N=%.12g divergence=%.12g leak=%g "
           "peak_speed=%.12g pressure_N=%.12g viscous_N=%.12g\n",
           n, drive, baffle, dt, residual, div, leak, speed, c.obstacle_mean_pressure_force_x_n,
           c.obstacle_viscous_force_x_n);
    if (drive != 0 && !baffle) {
        c.parameters.gradient = 0;
        double initial = energy(&c), previous = initial;
        for (int tick = 0; tick < 100; tick++) {
            assert(cfd_mac2d_step(&c, dt));
            double next = energy(&c);
            assert(next <= previous + 1e-10);
            previous = next;
        }
        assert(previous < initial);
    }
    cfd_mac2d_destroy(&c);
}
int main(int argc, char **argv) {
    int max_n = argc > 1 ? atoi(argv[1]) : 64;
    assert(max_n == 16 || max_n == 32 || max_n == 64);
    run(16, 0, 0, .005);
    for (int n = 16; n <= max_n; n *= 2) {
        run(n, .1, 0, .005);
        run(n, -.1, 0, .005);
        run(n, .1, 1, .005);
    }
    run(32, .1, 0, .0025);
    puts("Masked full-step momentum gate passed; physical drag accuracy not qualified.");
}
