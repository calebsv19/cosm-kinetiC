#include "app/cfd_mac2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static const double pi = 3.14159265358979323846;
typedef struct {
    double amplitude, pressure;
    int normal_gradient;
} Case;
static void exact(Case *c, double x, double y, double t, double z[3]) {
    double a = c->amplitude * exp(-t), b = c->pressure * exp(-t);
    z[0] = a * pi * sin(pi * x) * sin(2 * pi * y);
    z[1] = -a * pi * cos(pi * x) * pow(sin(pi * y), 2);
    z[2] = b * cos(pi * x) * (c->normal_gradient ? sin(pi * y) : cos(2 * pi * y));
}
static double forcing(void *context, int component, double x, double y, double t) {
    Case *c = context;
    double z[3];
    exact(c, x, y, t, z);
    double a = c->amplitude * exp(-t), b = c->pressure * exp(-t), k = pi;
    double ux = a * k * k * cos(k * x) * sin(2 * k * y),
           uy = 2 * a * k * k * sin(k * x) * cos(2 * k * y);
    double vx = a * k * k * sin(k * x) * pow(sin(k * y), 2), vy = -ux;
    double lapu = -5 * k * k * z[0],
           lapv = a * k * k * k * cos(k * x) * (pow(sin(k * y), 2) - 2 * cos(2 * k * y));
    double px = -b * k * sin(k * x) * (c->normal_gradient ? sin(k * y) : cos(2 * k * y));
    double py = b * cos(k * x) * (c->normal_gradient ? k * cos(k * y) : -2 * k * sin(2 * k * y));
    return component == 0 ? -z[0] + z[0] * ux + z[1] * uy + px - .01 * lapu
                          : -z[1] + z[0] * vx + z[1] * vy + py - .01 * lapv;
}
/* Independent finite-difference check of the analytic forcing expression. */
static void check_force(Case *c) {
    double h = 1e-4;
    for (int j = 1; j < 8; j++)
        for (int i = 1; i < 8; i++) {
            double x = i * .23, y = j * .11, t = .037, z[3], xp[3], xm[3], yp[3], ym[3], tp[3],
                   tm[3];
            exact(c, x, y, t, z);
            exact(c, x + h, y, t, xp);
            exact(c, x - h, y, t, xm);
            exact(c, x, y + h, t, yp);
            exact(c, x, y - h, t, ym);
            exact(c, x, y, t + h, tp);
            exact(c, x, y, t - h, tm);
            for (int d = 0; d < 2; d++) {
                double f = (tp[d] - tm[d]) / (2 * h) + z[0] * (xp[d] - xm[d]) / (2 * h) +
                           z[1] * (yp[d] - ym[d]) / (2 * h) +
                           (d == 0 ? xp[2] - xm[2] : yp[2] - ym[2]) / (2 * h) -
                           .01 * (xp[d] + xm[d] + yp[d] + ym[d] - 4 * z[d]) / (h * h);
                assert(fabs(f - forcing(c, d, x, y, t)) < 1e-7);
            }
        }
}
int main(int argc, char **argv) {
    assert(argc == 6);
    int n = atoi(argv[1]), steps = atoi(argv[2]);
    Case problem = {atof(argv[3]), atof(argv[4]), atoi(argv[5])};
    check_force(&problem);
    CfdMac2D c;
    double dims[3] = {2, 1, .5}, dt = .1 / steps;
    assert(cfd_mac2d_init(&c, n, n, dims, 1, .01, 0, 0, 0, 0));
    double z[3];
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            exact(&problem, 2. * i / n, (j + .5) / n, 0, z);
            c.u[j * n + i] = z[0];
        }
    for (int j = 1; j < n; j++)
        for (int i = 0; i < n; i++) {
            exact(&problem, 2. * (i + .5) / n, (double)j / n, 0, z);
            c.v[j * n + i] = z[1];
        }
    assert(cfd_mac2d_project(&c, dt));
    double div = 0, momentum = 0;
    int substeps = 0;
    for (int t = 0; t < steps; t++) {
        assert(cfd_mac2d_step_forced(&c, dt, forcing, &problem));
        div = fmax(div, cfd_mac2d_divergence(&c));
        momentum = fmax(momentum, fabs(c.momentum_residual));
        substeps += c.substeps;
    }
    double ev = 0, ep = 0, wall = 0, interior = 0, lag = 0;
    int nw = 0, ni = 0;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            exact(&problem, 2. * i / n, (j + .5) / n, .1, z);
            ev += pow(c.u[j * n + i] - z[0], 2);
            exact(&problem, 2. * (i + .5) / n, (double)j / n, .1, z);
            ev += pow(c.v[j * n + i] - z[1], 2);
            exact(&problem, 2. * (i + .5) / n, (j + .5) / n, .1, z);
            double e = pow(c.p[j * n + i] - z[2], 2);
            ep += e;
            exact(&problem, 2. * (i + .5) / n, (j + .5) / n, .1 - dt, z);
            lag += pow(c.p[j * n + i] - z[2], 2);
            if (j < n / 8 || j >= 7 * n / 8) {
                wall += e;
                nw++;
            } else {
                interior += e;
                ni++;
            }
        }
    assert(div < 1e-8 && momentum < 1e-9 && substeps == steps);
    printf("{\"n\":%d,\"dt\":%.17g,\"velocity_l2\":%.17g,\"pressure_l2\":%.17g,\"wall_pressure_"
           "l2\":%.17g,\"interior_pressure_l2\":%.17g,\"pressure_start_time_l2\":%.17g,"
           "\"divergence\":%.17g,\"momentum\":%.17g}\n",
           n, dt, sqrt(ev / (n * n)), sqrt(ep / (n * n)), sqrt(wall / nw), sqrt(interior / ni),
           sqrt(lag / (n * n)), div, momentum);
    cfd_mac2d_destroy(&c);
}
