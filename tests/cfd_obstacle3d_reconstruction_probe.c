#include "app/cfd_obstacle3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    int degree;
    double c[24];
} Poly;
static Poly factor(Poly p, double a, double b) {
    Poly r = {.degree = p.degree + 1};
    for (int i = 0; i <= p.degree; i++) {
        r.c[i] += a * p.c[i];
        r.c[i + 1] += b * p.c[i];
    }
    return r;
}
static Poly derivative(Poly p) {
    Poly r = {.degree = p.degree - 1};
    for (int i = 1; i <= p.degree; i++)
        r.c[i - 1] = i * p.c[i];
    return r;
}
static double value(Poly p, double x) {
    double v = 0;
    for (int i = p.degree; i >= 0; i--)
        v = v * x + p.c[i];
    return v;
}
static double integral(Poly p, double lo, double hi) {
    double v = 0;
    for (int i = 0; i <= p.degree; i++)
        v += p.c[i] * (pow(hi, i + 1) - pow(lo, i + 1)) / (i + 1);
    return v;
}
static double product_integral(Poly a, Poly b, double lo, double hi) {
    static const double x[6] = {.1252334085114689, .3678314989981802, .5873179542866175,
                                .7699026741943047, .9041172563704749, .9815606342467192};
    static const double w[6] = {.2491470458134028, .2334925365383548, .2031674267230659,
                                .1600783285433462, .1069393259953184, .0471753363865118};
    double v = 0, m = .5 * (hi + lo), h = .5 * (hi - lo);
    for (int i = 0; i < 6; i++)
        for (int d = -1; d <= 1; d += 2)
            v += w[i] * value(a, m + d * h * x[i]) * value(b, m + d * h * x[i]) * h;
    return v;
}
static double energy(Poly x, Poly y, Poly z, const double lo[3], const double hi[3]) {
    Poly dx = derivative(x), dy = derivative(y), dz = derivative(z), ddx = derivative(dx),
         ddy = derivative(dy);
    double xx = product_integral(x, x, lo[0], hi[0]), xd = product_integral(dx, dx, lo[0], hi[0]),
           xdd = product_integral(ddx, ddx, lo[0], hi[0]),
           xc = product_integral(x, ddx, lo[0], hi[0]);
    double yy = product_integral(y, y, lo[1], hi[1]), yd = product_integral(dy, dy, lo[1], hi[1]),
           ydd = product_integral(ddy, ddy, lo[1], hi[1]),
           yc = product_integral(y, ddy, lo[1], hi[1]);
    double zz = product_integral(z, z, lo[2], hi[2]), zd = product_integral(dz, dz, lo[2], hi[2]);
    return .1 * (4 * xd * yd * zz + xx * ydd * zz + xdd * yy * zz - 2 * xc * yc * zz +
                 xx * yd * zd + xd * yy * zd);
}
int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 8;
    bool averages = argc < 3 || strcmp(argv[2], "point");
    int grid[3] = {2 * n, n, n};
    double lengths[3] = {4, 2, 2};
    CfdObstacle3d s;
    CfdMemoryBudget budget = {.limit_bytes = 512 * 1024 * 1024};
    CfdMemoryBudget *old = cfd_memory_scope(&budget);
    assert(cfd_obstacle3d_init(&s, grid, lengths, 1, .1, .008, 2));
    Poly fx = {.c = {1}}, fy = {.c = {1}}, fz = {.c = {1}};
    for (int t = 0; t < 2; t++) {
        fx = factor(factor(fx, -1.5, 1), -2.5, 1);
        fy = factor(factor(factor(factor(fy, 0, 1), -2, 1), -.5, 1), -1.5, 1);
        fz = factor(factor(factor(factor(fz, 0, 1), -2, 1), -.5, 1), -1.5, 1);
    }
    fy = factor(fy, 1, .3);
    Poly dx = derivative(fx), dy = derivative(fy), ddy = derivative(dy);
    for (int q = 0; q < s.count; q++) {
        int a, c[3];
        double pos[3];
        cfd_obstacle_mixed3d_position(s.mixed, q, &a, pos, c);
        double h = 2. / n;
        if (a == 0)
            s.u[q] =
                value(fx, pos[0]) *
                (averages ? (value(fy, pos[1] + h / 2) - value(fy, pos[1] - h / 2)) / h
                          : value(dy, pos[1])) *
                (averages ? integral(fz, pos[2] - h / 2, pos[2] + h / 2) / h : value(fz, pos[2]));
        if (a == 1)
            s.u[q] =
                -(averages ? (value(fx, pos[0] + h / 2) - value(fx, pos[0] - h / 2)) / h
                           : value(dx, pos[0])) *
                value(fy, pos[1]) *
                (averages ? integral(fz, pos[2] - h / 2, pos[2] + h / 2) / h : value(fz, pos[2]));
    }
    Poly pressure = {.degree = 3, .c = {.02, .003, 0, .0004}};
    for (int k = 0; k < n; k++)
        for (int j = 0; j < n; j++)
            for (int i = 0; i < 2 * n; i++) {
                int q = cfd_obstacle_mixed3d_cell(s.mixed, i, j, k);
                if (q < 0)
                    continue;
                double h = 2. / n;
                s.p[q] = averages ? integral(pressure, i * h, (i + 1) * h) / h
                                  : value(pressure, (i + .5) * h);
            }
    s.inlet_pressure = .02;
    cfd_obstacle3d_measure(&s, s.u, s.p);
    double lo[3] = {0, 0, 0}, hi[3] = {4, 2, 2}, blo[3] = {1.5, .5, .5}, bhi[3] = {2.5, 1.5, 1.5};
    double exactD = energy(fx, fy, fz, lo, hi) - energy(fx, fy, fz, blo, bhi);
    double exactP = value(pressure, 1.5) - value(pressure, 2.5);
    double exactV =
        .1 * (value(ddy, 1.5) - value(ddy, .5)) * integral(fx, 1.5, 2.5) * integral(fz, .5, 1.5);
    printf("{\"n\":%d,\"sample_semantics\":\"%s\",\"pressure_force_n\":%.17g,\"exact_pressure_"
           "force_n\":%.17g,\"viscous_force_n\":%.17g,\"exact_viscous_force_n\":%.17g,"
           "\"dissipation_w\":%.17g,\"exact_dissipation_w\":%.17g,\"divergence\":%.17g}\n",
           n, averages ? "face_cell_averages" : "point", s.pressure_force[0], exactP,
           s.viscous_force[0], exactV, s.physical_dissipation, exactD, s.max_divergence);
    if (argc > 3) {
        FILE *f = fopen(argv[3], "wb");
        assert(f);
        assert(fwrite(grid, sizeof(int), 3, f) == 3);
        for (int k = 0; k < n; k++)
            for (int j = 0; j < n; j++)
                for (int i = 0; i < 2 * n; i++) {
                    double row[4];
                    for (int a = 0; a < 3; a++)
                        row[a] = cfd_obstacle3d_face(&s, a, i, j, k);
                    int q = cfd_obstacle_mixed3d_cell(s.mixed, i, j, k);
                    row[3] = q < 0 ? NAN : s.p[q];
                    assert(fwrite(row, sizeof(double), 4, f) == 4);
                }
        for (int k = 0; k < n; k++)
            for (int j = 0; j < n; j++) {
                double v = cfd_obstacle3d_face(&s, 0, 2 * n, j, k);
                assert(fwrite(&v, sizeof(double), 1, f) == 1);
            }
        assert(!fclose(f));
    }
    cfd_obstacle3d_destroy(&s);
    assert(!budget.live_bytes);
    cfd_memory_scope(old);
}
