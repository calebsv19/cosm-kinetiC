#include "app/cfd_refined_diffusion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double check(int n) {
    CfdRefinedMesh m;
    assert(cfd_refined_mesh_init(&m, n, n, 1, 1, n / 4, n / 4, 3 * n / 4, 3 * n / 4));
    unsigned char *fixed = calloc(m.face_count, 1);
    double *velocity = calloc(m.face_count, sizeof(double)),
           *bc = calloc(m.face_count, sizeof(double));
    double *flux = calloc(m.face_count, sizeof(double)), *pf = calloc(m.face_count, sizeof(double));
    double *p = calloc(m.cell_count, sizeof(double)),
           *source = calloc(m.cell_count, sizeof(double));
    double *balance = calloc(m.cell_count, sizeof(double));
    assert(fixed && velocity && bc && flux && pf && p && source && balance);
    double k = 2 * acos(-1), amplitude = .01;
    for (int f = 0; f < m.face_count; f++) {
        CfdRefinedFace *face = &m.faces[f];
        double x = face->cx, y = face->cy;
        fixed[f] = face->axis == 0 && face->hi < 0;
        velocity[f] = face->axis == 0 ? 6 * y * (1 - y) + amplitude * k * sin(k * x) * cos(k * y)
                                      : -amplitude * k * (1 - cos(k * x)) * sin(k * y);
    }
    CfdRefinedDiffusion *d = cfd_refined_diffusion_create(&m, fixed);
    assert(d);
    cfd_refined_flux_balance(&m, velocity, balance);
    double initial = 0;
    for (int c = 0; c < m.cell_count; c++) {
        source[c] = -balance[c] / m.cells[c].volume;
        initial = fmax(initial, fabs(source[c]));
    }
    CfdRefinedSolve report;
    assert(cfd_refined_diffusion_solve(d, source, bc, p, pf, flux, &report));
    double error = 0;
    for (int f = 0; f < m.face_count; f++) {
        CfdRefinedFace *face = &m.faces[f];
        velocity[f] += flux[f];
        double exact = face->axis == 0 ? 6 * face->cy * (1 - face->cy) : 0;
        double distance = 0, spacing = face->axis == 0 ? m.dx : m.dy;
        if (face->lo >= 0)
            distance += .25 * m.cells[face->lo].span * spacing;
        if (face->hi >= 0)
            distance += .25 * m.cells[face->hi].span * spacing;
        error += (velocity[f] - exact) * (velocity[f] - exact) * face->area * distance;
    }
    cfd_refined_flux_balance(&m, velocity, balance);
    double divergence = 0;
    for (int c = 0; c < m.cell_count; c++)
        divergence = fmax(divergence, fabs(balance[c]) / m.cells[c].volume);
    error = sqrt(error / 2);
    printf(
        "n=%d initial_divergence=%.9g projected_divergence=%.9g velocity_l2=%.12g iterations=%d\n",
        n, initial, divergence, error, report.iterations);
    assert(initial > .1 && divergence < 1e-7);
    free(fixed);
    free(velocity);
    free(bc);
    free(flux);
    free(pf);
    free(p);
    free(source);
    free(balance);
    cfd_refined_diffusion_destroy(d);
    cfd_refined_mesh_destroy(&m);
    return error;
}
int main(void) {
    double a = check(8), b = check(16), c = check(32);
    printf("velocity error ratios: %.6g %.6g\n", a / b, b / c);
    assert(a / b > 1.5 && b / c > 1.5);
}
