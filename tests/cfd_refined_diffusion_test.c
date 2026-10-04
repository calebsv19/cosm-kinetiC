#include "app/cfd_refined_diffusion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double check(int n, int affine, int uniform, int mixed) {
    CfdRefinedMesh m;
    assert(cfd_refined_mesh_init(&m, n, n, 1, 1, uniform ? 0 : n / 4, uniform ? 0 : n / 4,
                                 uniform ? n : 3 * n / 4, uniform ? n : 3 * n / 4));
    unsigned char *mask = calloc(m.face_count, 1);
    assert(mask);
    for (int f = 0; f < m.face_count; f++)
        mask[f] = m.faces[f].axis == 0 && m.faces[f].hi < 0;
    CfdRefinedDiffusion *d = cfd_refined_diffusion_create(&m, mixed ? mask : NULL);
    assert(d);
    double *s = calloc(m.cell_count, sizeof(double)), *bc = calloc(m.face_count, sizeof(double));
    double *p = calloc(m.cell_count, sizeof(double)), *pf = calloc(m.face_count, sizeof(double));
    double *flux = calloc(m.face_count, sizeof(double)),
           *bal = calloc(m.cell_count, sizeof(double));
    assert(s && bc && p && pf && flux && bal);
    double pi = acos(-1), error = 0;
    for (int c = 0; c < m.cell_count; c++) {
        double x = m.cells[c].cx, y = m.cells[c].cy;
        s[c] = affine ? 0 : 2 * pi * pi * sin(pi * x) * sin(pi * y);
    }
    for (int f = 0; f < m.face_count; f++)
        bc[f] = affine ? 1 + 2 * m.faces[f].cx - 3 * m.faces[f].cy : 0;
    if (mixed)
        for (int f = 0; f < m.face_count; f++) {
            const CfdRefinedFace *face = &m.faces[f];
            if (!mask[f] && (face->lo < 0 || face->hi < 0))
                bc[f] = (face->lo < 0 ? -1 : 1) * (face->axis == 0 ? -2 : 3);
        }
    CfdRefinedSolve report;
    assert(cfd_refined_diffusion_solve(d, s, bc, p, pf, flux, &report));
    double maxflux = 0;
    for (int c = 0; c < m.cell_count; c++) {
        double x = m.cells[c].cx, y = m.cells[c].cy;
        double exact = affine ? 1 + 2 * x - 3 * y : sin(pi * x) * sin(pi * y);
        error += (p[c] - exact) * (p[c] - exact) * m.cells[c].volume;
    }
    if (affine)
        for (int f = 0; f < m.face_count; f++)
            maxflux = fmax(maxflux, fabs(flux[f] - (m.faces[f].axis == 0 ? -2 : 3)));
    cfd_refined_flux_balance(&m, flux, bal);
    double maxbalance = 0;
    for (int c = 0; c < m.cell_count; c++)
        maxbalance = fmax(maxbalance, fabs(bal[c] - s[c] * m.cells[c].volume));
    error = sqrt(error);
    printf("n=%d uniform=%d affine=%d mixed=%d cells=%d iterations=%d pressure_l2=%.12g "
           "flux_error=%.3g "
           "continuity=%.3g balance=%.3g\n",
           n, uniform, affine, mixed, m.cell_count, report.iterations, error, maxflux,
           report.flux_mismatch, maxbalance);
    assert(report.flux_mismatch < 1e-9 && maxbalance < 1e-9);
    if (affine)
        assert(error < 1e-9 && maxflux < 1e-8);
    free(mask);
    free(s);
    free(bc);
    free(p);
    free(pf);
    free(flux);
    free(bal);
    cfd_refined_diffusion_destroy(d);
    cfd_refined_mesh_destroy(&m);
    return error;
}
int main(void) {
    check(8, 1, 0, 0);
    check(16, 1, 0, 1);
    check(16, 1, 0, 0);
    check(16, 1, 1, 0);
    double a = check(8, 0, 0, 0), b = check(16, 0, 0, 0), c = check(32, 0, 0, 0);
    assert(a / b > 3 && b / c > 3);
    double u = check(16, 0, 1, 0), v = check(32, 0, 1, 0);
    assert(u / v > 3.5);
    puts("coarse/fine hybrid diffusion: affine exactness, conservation and manufactured "
         "convergence passed");
}
