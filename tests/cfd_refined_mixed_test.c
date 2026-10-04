#include "app/cfd_refined_mixed.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 8;
    assert(n >= 4 && n <= 64 && n % 4 == 0);
    CfdRefinedMesh m;
    assert(cfd_refined_mesh_init(&m, 2 * n, n, 4, 2, n / 2, n / 4, 3 * n / 2, 3 * n / 4));
    CfdRefinedMixed *s = cfd_refined_mixed_create(&m, .1);
    assert(s);
    double *cells = calloc((size_t)5 * m.cell_count, sizeof(double)),
           *faces = calloc((size_t)4 * m.face_count, sizeof(double));
    assert(cells && faces);
    double *u = cells, *v = u + m.cell_count, *p = v + m.cell_count, *ru = p + m.cell_count,
           *rv = ru + m.cell_count;
    double *fu = faces, *fv = fu + m.face_count, *bu = fv + m.face_count, *bv = bu + m.face_count;
    for (int f = 0; f < m.face_count; f++) {
        double y = m.faces[f].cy / 2;
        bu[f] = 6 * .02 * y * (1 - y);
    }
    CfdRefinedMixedReport report;
    bool okay = cfd_refined_mixed_solve(s, 0, ru, rv, bu, bv, u, v, fu, fv, p, &report);
    if (!okay)
        fprintf(stderr, "solve failed iterations=%d residual=%.12g\n", report.iterations,
                report.relative_residual);
    assert(okay);
    double error = 0, pe = 0;
    for (int i = 0; i < m.cell_count; i++) {
        double y = m.cells[i].cy / 2, e = u[i] - 6 * .02 * y * (1 - y),
               ep = p[i] - .006 * (4 - m.cells[i].cx);
        error += (e * e + v[i] * v[i]) * m.cells[i].volume;
        pe += ep * ep * m.cells[i].volume;
    }
    printf("n=%d velocity_l2=%.15g pressure_l2=%.15g divergence=%.3g iterations=%d residual=%.3g "
           "bytes=%zu\n",
           n, sqrt(error / 8), sqrt(pe / 8), report.divergence, report.iterations,
           report.relative_residual, report.storage_bytes);
    assert(report.divergence < 1e-8);
    if (argc > 2) {
        FILE *fp = fopen(argv[2], "w");
        assert(fp);
        fprintf(fp, "{\"u\":[");
        for (int i = 0; i < m.cell_count; i++)
            fprintf(fp, "%s%.17g", i ? "," : "", u[i]);
        fprintf(fp, "],\"v\":[");
        for (int i = 0; i < m.cell_count; i++)
            fprintf(fp, "%s%.17g", i ? "," : "", v[i]);
        fprintf(fp, "],\"p\":[");
        for (int i = 0; i < m.cell_count; i++)
            fprintf(fp, "%s%.17g", i ? "," : "", p[i]);
        fprintf(fp, "]}\n");
        fclose(fp);
    }
    /* Same matrix and RHS should reuse both the factor and converged state. */
    assert(cfd_refined_mixed_solve(s, 0, ru, rv, bu, bv, u, v, fu, fv, p, &report));
    assert(report.iterations == 0);
    free(cells);
    free(faces);
    cfd_refined_mixed_destroy(s);
    cfd_refined_mesh_destroy(&m);
    return 0;
}
