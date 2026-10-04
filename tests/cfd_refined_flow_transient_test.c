#include "app/cfd_refined_channel.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
typedef struct {
    double u, v, fx, fy;
} Exact;
/* Divergence-free streamfunction perturbation with zero velocity on all
 * boundaries and zero normal velocity derivative at the natural outlet.
 * The forcing is derived from continuous Navier-Stokes, not the solver stencil. */
static Exact exact(double x, double y, double time) {
    double a = acos(-1) / 4, b = acos(-1) / 2, s = sin(a * x), q = cos(a * x), e = .01 * exp(-time);
    double X = s * s * s * s, X1 = 4 * a * s * s * s * q, X2 = 4 * a * a * (3 * s * s * q * q - X);
    double X3 = 4 * a * a * a * (6 * s * q * q * q - 10 * s * s * s * q);
    double Y = pow(sin(b * y), 2), Y1 = b * sin(2 * b * y), Y2 = 2 * b * b * cos(2 * b * y),
           Y3 = -4 * b * b * b * sin(2 * b * y);
    double up = e * X * Y1, vp = -e * X1 * Y, u = 6 * .02 * (y / 2) * (1 - y / 2) + up, v = vp;
    double ux = e * X1 * Y1, uy = 3 * .02 * (1 - y) + e * X * Y2, vx = -e * X2 * Y,
           vy = -e * X1 * Y1;
    double lapu = e * (X2 * Y1 + X * Y3), lapv = -e * (X3 * Y + X1 * Y2);
    return (Exact){u, v, -up + u * ux + v * uy - .1 * lapu, -vp + u * vx + v * vy - .1 * lapv};
}
static double *run(int n, double dt, double *error, int *count) {
    clock_t started = clock();
    CfdRefinedMesh m;
#if defined(CFD_REFINED_VERIFY_LOCAL)
    CfdRefinementRegion regions[] = {
        {1, .5, 3, 1.5, 1}, {1.5, .75, 2.5, 1.25, 2}, {1.75, .875, 2.25, 1.125, 3}};
    assert(cfd_refined_mesh_init_regions(&m, 2 * n, n, 4, 2, regions, 3, 100000));
#elif defined(CFD_REFINED_VERIFY_UNIFORM)
    assert(cfd_refined_mesh_init(&m, 2 * n, n, 4, 2, 0, 0, 2 * n, n));
#else
    assert(cfd_refined_mesh_init(&m, 2 * n, n, 4, 2, n / 2, n / 4, 3 * n / 2, 3 * n / 4));
#endif
    CfdRefinedChannel c;
    assert(cfd_refined_channel_init(&c, &m, 1, .1, .02));
    double *fx = calloc(m.cell_count, sizeof(double)), *fy = calloc(m.cell_count, sizeof(double));
    double *result = calloc((size_t)2 * m.cell_count, sizeof(double));
    assert(fx && fy && result);
    *count = 2 * m.cell_count;
    for (int i = 0; i < m.cell_count; i++) {
        Exact e = exact(m.cells[i].cx, m.cells[i].cy, 0);
        c.u[i] = c.old_u[i] = e.u;
        c.v[i] = c.old_v[i] = e.v;
    }
    for (int f = 0; f < m.face_count; f++) {
        Exact e = exact(m.faces[f].cx, m.faces[f].cy, 0);
        c.normal[f] = m.faces[f].axis == 0 ? e.u : e.v;
#ifdef CFD_REFINED_VERIFY_INITIAL_FLUX
        double x = m.faces[f].cx, y = m.faces[f].cy, h = .5 * m.faces[f].area;
        double a = acos(-1) / 4, b = acos(-1) / 2;
        if (m.faces[f].axis == 0) {
            double top = .01 * pow(sin(a * x), 4) * pow(sin(b * (y + h)), 2);
            double bottom = .01 * pow(sin(a * x), 4) * pow(sin(b * (y - h)), 2);
            c.normal[f] = 6 * .02 * ((y / 2) * (1 - y / 2) - h * h / 12) + (top - bottom) / (2 * h);
        } else {
            double right = .01 * pow(sin(a * (x + h)), 4) * pow(sin(b * y), 2);
            double left = .01 * pow(sin(a * (x - h)), 4) * pow(sin(b * y), 2);
            c.normal[f] = -(right - left) / (2 * h);
        }
#endif
    }
    clock_t setup_done = clock();
    int steps = (int)round(.4 / dt);
    double max_cfl = 0, min_h = INFINITY;
    long total_iterations = 0;
    for (int i = 0; i < m.cell_count; i++)
        min_h = fmin(min_h, fmin(m.dx, m.dy) * m.cells[i].span / m.lattice_scale);
    int max_correctors = 0;
    double maxdiv = 0;
    for (int k = 0; k < steps; k++) {
        for (int i = 0; i < m.cell_count; i++) {
            Exact e = exact(m.cells[i].cx, m.cells[i].cy, (k + 1) * dt);
            fx[i] = e.fx;
            fy[i] = e.fy;
        }
        if (!cfd_refined_channel_step_forced(&c, dt, fx, fy)) {
            fprintf(stderr, "coupling failed n=%d step=%d residual=%.9g correctors=%d\n", n, k,
                    c.coupling_residual, c.correctors);
            assert(0);
        }
        max_cfl = fmax(max_cfl, c.transport_cfl);
        total_iterations += c.mixed_iterations;
        maxdiv = fmax(maxdiv, c.divergence);
        if (c.correctors > max_correctors)
            max_correctors = c.correctors;
    }
    clock_t evolution_done = clock();
    double err = 0, p_error = 0;
    for (int i = 0; i < m.cell_count; i++) {
        Exact e = exact(m.cells[i].cx, m.cells[i].cy, .4);
        err +=
            ((c.u[i] - e.u) * (c.u[i] - e.u) + (c.v[i] - e.v) * (c.v[i] - e.v)) * m.cells[i].volume;
        double p = .006 * (4 - m.cells[i].cx);
        p_error += (c.p[i] - p) * (c.p[i] - p) * m.cells[i].volume;
        result[2 * i] = c.u[i] * sqrt(m.cells[i].volume / 8);
        result[2 * i + 1] = c.v[i] * sqrt(m.cells[i].volume / 8);
    }
    *error = sqrt(err / 8);
    printf(
        "n=%d dt=%.6g velocity_l2=%.12g pressure_l2=%.12g max_divergence=%.3g max_correctors=%d\n",
        n, dt, *error, sqrt(p_error / 8), maxdiv, max_correctors);
    printf("cost_cells=%d steps=%d min_h_m=%.9g max_transport_cfl=%.9g observed_cfl_dt_limit_s=%.9g nominal_explicit_diffusion_dt_s=%.9g setup_cpu_s=%.9g evolution_cpu_s=%.9g total_mixed_iterations=%ld\n",
           m.cell_count, steps, min_h, max_cfl, max_cfl > 0 ? .25 * dt / max_cfl : INFINITY,
           min_h * min_h / (4 * c.nu), (double)(setup_done-started)/CLOCKS_PER_SEC,
           (double)(evolution_done-setup_done)/CLOCKS_PER_SEC, total_iterations);
    fflush(stdout);
    assert(maxdiv < 1e-7 && isfinite(*error));
    free(fx);
    free(fy);
    cfd_refined_channel_destroy(&c);
    cfd_refined_mesh_destroy(&m);
    return result;
}
static double difference(const double *a, const double *b, int n) {
    double e = 0;
    for (int i = 0; i < n; i++)
        e += (a[i] - b[i]) * (a[i] - b[i]);
    return sqrt(e);
}
int main(int argc, char **argv) {
    if (argc == 3) {
        int n = atoi(argv[1]), count;
        double dt = atof(argv[2]), error;
        assert(n >= 8 && n <= 64 && n % 8 == 0);
        assert(isfinite(dt) && dt > 0 && dt <= .04 && fabs(.4/dt-round(.4/dt)) < 1e-8);
        free(run(n, dt, &error, &count));
        return 0;
    }
    assert(argc == 1);
    int count;
    double a, b, c;
#ifndef CFD_REFINED_VERIFY_TIME_ONLY
    free(run(8, .005, &a, &count));
    free(run(16, .005, &b, &count));
    free(run(32, .005, &c, &count));
    printf("coupled spatial error ratios %.6g %.6g\n", a / b, b / c);
    fflush(stdout);
    assert(a / b > 1.5 && b / c > 1.5);
#endif
    double *u = run(16, .04, &a, &count), *v = run(16, .02, &b, &count),
           *w = run(16, .01, &c, &count);
    double d1 = difference(u, v, count), d2 = difference(v, w, count);
    printf("coupled timestep differences %.12g %.12g ratio %.6g\n", d1, d2, d1 / d2);
    double *fine = run(16, .005, &a, &count), *finer = run(16, .0025, &b, &count);
    double d3 = difference(w, fine, count), d4 = difference(fine, finer, count);
    printf("fine timestep differences %.12g %.12g ratios %.6g %.6g\n", d3, d4, d2 / d3, d3 / d4);
    printf("coupled_temporal_order_qualified=%s\n",
           d1 / d2 > 3.5 && d2 / d3 > 3.5 && d3 / d4 > 3.5 ? "true" : "false");
#ifndef CFD_REFINED_VERIFY_SPLIT
    assert(d1 / d2 > 3.5 && d2 / d3 > 3.5 && d3 / d4 > 3.5);
#endif
    free(fine);
    free(finer);
    free(u);
    free(v);
    free(w);
    return 0;
}
