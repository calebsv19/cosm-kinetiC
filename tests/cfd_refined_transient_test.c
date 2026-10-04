#include "app/cfd_refined_diffusion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double run(int n, int steps, int uniform, int discrete_reference) {
    CfdRefinedMesh m;
    assert(cfd_refined_mesh_init(&m, n, n, 1, 1, uniform ? 0 : n / 4, uniform ? 0 : n / 4,
                                 uniform ? n : 3 * n / 4, uniform ? n : 3 * n / 4));
    CfdRefinedDiffusion *d = cfd_refined_diffusion_create(&m, NULL);
    assert(d);
    double *u = calloc(m.cell_count, sizeof(double)), *old = calloc(m.cell_count, sizeof(double));
    double *next = calloc(m.cell_count, sizeof(double)),
           *rhs = calloc(m.cell_count, sizeof(double));
    double *bc = calloc(m.face_count, sizeof(double)), *uf = calloc(m.face_count, sizeof(double));
    double *flux = calloc(m.face_count, sizeof(double)),
           *balance = calloc(m.cell_count, sizeof(double));
    assert(u && old && next && rhs && bc && uf && flux && balance);
    double pi = acos(-1), nu = .1, T = .5, dt = T / steps, energy = 0, maxbalance = 0;
    long iterations = 0;
    for (int c = 0; c < m.cell_count; c++) {
        u[c] = old[c] = sin(pi * m.cells[c].cx) * sin(pi * m.cells[c].cy);
        energy += u[c] * u[c] * m.cells[c].volume;
    }
    for (int step = 0; step < steps; step++) {
        double mass = step == 0 ? 1 / dt : 1.5 / dt;
        for (int c = 0; c < m.cell_count; c++)
            rhs[c] = step == 0 ? u[c] / dt : (2 * u[c] - .5 * old[c]) / dt;
        CfdRefinedSolve report;
        assert(cfd_refined_helmholtz_solve(d, mass, nu, rhs, bc, next, uf, flux, &report));
        iterations += report.iterations;
        cfd_refined_flux_balance(&m, flux, balance);
        double next_energy = 0;
        for (int c = 0; c < m.cell_count; c++) {
            maxbalance = fmax(maxbalance, fabs(mass * next[c] * m.cells[c].volume + balance[c] -
                                               rhs[c] * m.cells[c].volume));
            next_energy += next[c] * next[c] * m.cells[c].volume;
        }
        assert(next_energy <= energy + 1e-11);
        energy = next_energy;
        double *swap = old;
        old = u;
        u = next;
        next = swap;
    }
    double h = 1. / (2 * n),
           lambda = discrete_reference ? 8 / (h * h) * pow(sin(pi * h / 2), 2) : 2 * pi * pi;
    double error = 0;
    for (int c = 0; c < m.cell_count; c++) {
        double exact = exp(-nu * lambda * T) * sin(pi * m.cells[c].cx) * sin(pi * m.cells[c].cy);
        error += (u[c] - exact) * (u[c] - exact) * m.cells[c].volume;
    }
    error = sqrt(error);
    printf("n=%d uniform=%d steps=%d dt=%.9g diffusive_number=%.6g error=%.12g balance=%.3g "
           "iterations=%ld\n",
           n, uniform, steps, dt, nu * dt / (h * h), error, maxbalance, iterations);
    assert(maxbalance < 1e-9);
    free(u);
    free(old);
    free(next);
    free(rhs);
    free(bc);
    free(uf);
    free(flux);
    free(balance);
    cfd_refined_diffusion_destroy(d);
    cfd_refined_mesh_destroy(&m);
    return error;
}
int main(void) {
    /* Uniform-grid eigenvalue is analytically known, separating temporal error
     * from spatial error without using a finer numerical run as the answer. */
    double a = run(8, 10, 1, 1), b = run(8, 20, 1, 1), c = run(8, 40, 1, 1);
    printf("temporal ratios %.6g %.6g\n", a / b, b / c);
    assert(a / b > 3.5 && b / c > 3.5);
    /* Local patch spans the evolving sine field; dt is below spatial error. */
    a = run(8, 100, 0, 0);
    b = run(16, 100, 0, 0);
    c = run(32, 100, 0, 0);
    printf("refined spatial ratios %.6g %.6g\n", a / b, b / c);
    assert(a / b > 3 && b / c > 3);
    run(32, 10, 0, 0); /* Strongly exceeds the explicit diffusion stability bound. */
    puts("implicit refined diffusion: temporal/spatial convergence, decay and per-cell balance "
         "passed");
}
