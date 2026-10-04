#include "app/cfd_refined_transport.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double average(double lo, double hi, double center, double sigma) {
    double scale = sqrt(2) * sigma;
    return sqrt(acos(-1) / 2) * sigma * (erf((hi - center) / scale) - erf((lo - center) / scale)) /
           (hi - lo);
}
static double exact_cell(const CfdRefinedMesh *m, int c, double time) {
    const CfdRefinedCell *cell = &m->cells[c];
    double hx = .25 * cell->span * m->dx, hy = .25 * cell->span * m->dy;
    return average(cell->cx - hx, cell->cx + hx, .6 + time, .22) *
           average(cell->cy - hy, cell->cy + hy, 1, .25);
}
static void boundary(const CfdRefinedMesh *m, double time, double *bc) {
    for (int fi = 0; fi < m->face_count; fi++) {
        const CfdRefinedFace *f = &m->faces[fi];
        if (f->axis == 0)
            bc[fi] = exp(-.5 * pow((f->cx - .6 - time) / .22, 2)) *
                     average(f->cy - .5 * f->area, f->cy + .5 * f->area, 1, .25);
        else
            bc[fi] = average(f->cx - .5 * f->area, f->cx + .5 * f->area, .6 + time, .22) *
                     exp(-.5 * pow((f->cy - 1) / .25, 2));
    }
}
static double run(int n, int uniform) {
    CfdRefinedMesh m;
    assert(cfd_refined_mesh_init(&m, 2 * n, n, 4, 2, uniform ? 0 : n / 2, uniform ? 0 : n / 4,
                                 uniform ? 2 * n : 3 * n / 2, uniform ? n : 3 * n / 4));
    CfdRefinedTransport *t = cfd_refined_transport_create(&m);
    assert(t);
    double *q = calloc(m.cell_count, sizeof(double)), *stage = calloc(m.cell_count, sizeof(double));
    double *a = calloc(m.cell_count, sizeof(double)), *b = calloc(m.cell_count, sizeof(double));
    double *bc = calloc(m.face_count, sizeof(double)), *vel = calloc(m.face_count, sizeof(double));
    assert(q && stage && a && b && bc && vel);
    for (int c = 0; c < m.cell_count; c++)
        q[c] = exact_cell(&m, c, 0);
    for (int f = 0; f < m.face_count; f++)
        vel[f] = m.faces[f].axis == 0 ? 1 : 0;
    double T = 2.7, dt = .2 / n, time = 0, mass0 = 0, outflow = 0, maxbalance = 0;
    int steps = (int)ceil(T / dt);
    dt = T / steps;
    for (int c = 0; c < m.cell_count; c++)
        mass0 += q[c] * m.cells[c].volume;
    for (int k = 0; k < steps; k++) {
        double rate, flux0, flux1, sum = 0;
        boundary(&m, time, bc);
        assert(cfd_refined_transport_rhs(t, q, vel, bc, a, &rate, &flux0));
        assert(dt * rate <= .201);
        for (int c = 0; c < m.cell_count; c++) {
            stage[c] = q[c] + dt * a[c];
            sum += a[c] * m.cells[c].volume;
        }
        maxbalance = fmax(maxbalance, fabs(sum + flux0));
        boundary(&m, time + dt, bc);
        assert(cfd_refined_transport_rhs(t, stage, vel, bc, b, &rate, &flux1));
        for (int c = 0; c < m.cell_count; c++)
            q[c] = .5 * (q[c] + stage[c] + dt * b[c]);
        outflow += .5 * dt * (flux0 + flux1);
        time += dt;
    }
    double error = 0, mass = 0, minq = 1, maxq = 0;
    for (int c = 0; c < m.cell_count; c++) {
        double e = q[c] - exact_cell(&m, c, T);
        error += e * e * m.cells[c].volume;
        mass += q[c] * m.cells[c].volume;
        minq = fmin(minq, q[c]);
        maxq = fmax(maxq, q[c]);
    }
    error = sqrt(error);
    printf("n=%d uniform=%d cells=%d steps=%d l2=%.12g conservation=%.3g face_balance=%.3g "
           "range=[%.3g,%.6g]\n",
           n, uniform, m.cell_count, steps, error, fabs(mass + outflow - mass0), maxbalance, minq,
           maxq);
    assert(fabs(mass + outflow - mass0) < 1e-11 && maxbalance < 1e-11);
    assert(minq >= -1e-12 && maxq <= 1 + 1e-12);
    free(q);
    free(stage);
    free(a);
    free(b);
    free(bc);
    free(vel);
    cfd_refined_transport_destroy(t);
    cfd_refined_mesh_destroy(&m);
    return error;
}
int main(void) {
    double a = run(8, 0), b = run(16, 0), c = run(32, 0);
    printf("refined crossing error ratios: %.6g %.6g\n", a / b, b / c);
    assert(a / b > 1.5 && b / c > 1.5);
    a = run(8, 1);
    b = run(16, 1);
    c = run(32, 1);
    printf("uniform crossing error ratios: %.6g %.6g\n", a / b, b / c);
    assert(a / b > 1.5 && b / c > 1.5);
    puts("conservative transport across both refinement boundaries passed");
}
