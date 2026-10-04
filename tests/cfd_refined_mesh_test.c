#include "app/cfd_refined_mesh.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static void check(int nx, int ny, int x0, int y0, int x1, int y1) {
    CfdRefinedMesh m;
    assert(cfd_refined_mesh_init(&m, nx, ny, 4, 2, x0, y0, x1, y1));
    int nc = nx * ny;
    assert(m.cell_count == nc + 3 * (x1 - x0) * (y1 - y0));
    double *base = calloc(nc, sizeof(double)), *gx = calloc(nc, sizeof(double));
    double *gy = calloc(nc, sizeof(double)), *back = calloc(nc, sizeof(double));
    double *leaf = calloc(m.cell_count, sizeof(double));
    double *balance = calloc(m.cell_count, sizeof(double));
    double *vel = calloc(m.face_count, sizeof(double));
    assert(base && gx && gy && back && leaf && balance && vel);
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++) {
            int k = j * nx + i;
            base[k] = 3 + 2 * (i + .5) * m.dx - 4 * (j + .5) * m.dy;
            gx[k] = 2;
            gy[k] = -4;
        }
    cfd_refined_prolong(&m, base, gx, gy, leaf);
    double volume = 0, integral = 0;
    for (int i = 0; i < m.cell_count; i++) {
        CfdRefinedCell *c = &m.cells[i];
        assert(fabs(leaf[i] - (3 + 2 * c->cx - 4 * c->cy)) < 2e-14);
        volume += c->volume;
        integral += leaf[i] * c->volume;
    }
    assert(fabs(volume - 8) < 1e-12);
    assert(fabs(integral - 24) < 1e-11);
    cfd_refined_restrict(&m, leaf, back);
    for (int i = 0; i < nc; i++)
        assert(fabs(base[i] - back[i]) < 2e-14);
    int interfaces = 0;
    for (int i = 0; i < m.face_count; i++) {
        CfdRefinedFace *f = &m.faces[i];
        assert(f->lo != f->hi && f->area > 0);
        if (f->lo >= 0 && f->hi >= 0 && m.cells[f->lo].span != m.cells[f->hi].span)
            interfaces++;
        /* Analytic linear velocity has divergence 5, including interface cells. */
        vel[i] = f->axis == 0 ? 2 * f->cx + 3 * f->cy + 1 : -4 * f->cx + 3 * f->cy - 2;
    }
    cfd_refined_flux_balance(&m, vel, balance);
    for (int i = 0; i < m.cell_count; i++)
        assert(fabs(balance[i] - 5 * m.cells[i].volume) < 2e-13);
    double boundary = 0, total = 0;
    for (int i = 0; i < m.face_count; i++) {
        vel[i] = sin(i * .37);
        if (m.faces[i].lo < 0)
            boundary -= vel[i] * m.faces[i].area;
        if (m.faces[i].hi < 0)
            boundary += vel[i] * m.faces[i].area;
    }
    cfd_refined_flux_balance(&m, vel, balance);
    for (int i = 0; i < m.cell_count; i++)
        total += balance[i];
    assert(fabs(total - boundary) < 1e-12);
    if (x0 > 0 && x1 < nx && y0 > 0 && y1 < ny)
        assert(interfaces == 4 * ((x1 - x0) + (y1 - y0)));
    printf("mesh=%dx%d leaves=%d faces=%d coarse_fine_faces=%d conservation=%.3g\n", nx, ny,
           m.cell_count, m.face_count, interfaces, fabs(total - boundary));
    free(base);
    free(gx);
    free(gy);
    free(back);
    free(leaf);
    free(balance);
    free(vel);
    cfd_refined_mesh_destroy(&m);
    cfd_refined_mesh_destroy(&m);
}
int main(void) {
    check(8, 8, 2, 2, 6, 6);
    check(17, 13, 3, 4, 12, 10);
    check(8, 6, 0, 0, 8, 6);
    check(32, 16, 0, 3, 6, 16);
    CfdRefinedMesh m;
    assert(!cfd_refined_mesh_init(&m, 8, 8, NAN, 2, 2, 2, 6, 6));
    assert(!cfd_refined_mesh_init(&m, 8, 8, 4, 2, 6, 2, 2, 6));
    puts("fixed refinement topology/transfer checks passed; coupled flow is verified separately");
}
