#include "app/cfd_refined_diffusion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static void geometry(CfdRefinedMesh *m) {
    int nc = m->cell_count, nf = m->face_count, nb = m->nx * m->ny;
    double *store = calloc((size_t)4 * nb + 3 * nc + nf, sizeof(double));
    int *degree = calloc(nc, sizeof(int));
    assert(store && degree);
    double *base = store, *gx = base + nb, *gy = gx + nb, *back = gy + nb, *leaf = back + nb,
           *balance = leaf + nc, *volume = balance + nc, *vel = volume + nc;
    for (int i = 0; i < nb; i++) {
        base[i] = 3 + 2 * (i % m->nx + .5) * m->dx - 4 * (i / m->nx + .5) * m->dy;
        gx[i] = 2;
        gy[i] = -4;
    }
    cfd_refined_prolong(m, base, gx, gy, leaf);
    cfd_refined_restrict(m, leaf, back);
    double sum = 0;
    for (int c = 0; c < nc; c++) {
        assert(fabs(leaf[c] - (3 + 2 * m->cells[c].cx - 4 * m->cells[c].cy)) < 1e-12);
        sum += m->cells[c].volume;
    }
    assert(fabs(sum - m->nx * m->dx * m->ny * m->dy) < 1e-10);
    for (int i = 0; i < nb; i++)
        assert(fabs(base[i] - back[i]) < 1e-12);
    for (int f = 0; f < nf; f++) {
        CfdRefinedFace *p = &m->faces[f];
        assert(p->area > 0 && p->lo != p->hi);
        int ids[2] = {p->lo, p->hi};
        for (int k = 0; k < 2; k++)
            if (ids[k] >= 0) {
                degree[ids[k]]++;
                volume[ids[k]] += (k == 0 ? 1 : -1) * (p->axis == 0 ? p->cx : p->cy) * p->area;
            }
        if (p->lo >= 0 && p->hi >= 0) {
            int a = m->cells[p->lo].span, b = m->cells[p->hi].span;
            assert(a <= 2 * b && b <= 2 * a);
        }
        vel[f] = p->axis == 0 ? 2 * p->cx + 3 * p->cy + 1 : -4 * p->cx + 3 * p->cy - 2;
    }
    cfd_refined_flux_balance(m, vel, balance);
    for (int c = 0; c < nc; c++) {
        assert(degree[c] >= 4 && degree[c] <= 8);
        assert(fabs(volume[c] - 2 * m->cells[c].volume) < 1e-12);
        assert(fabs(balance[c] - 5 * m->cells[c].volume) < 1e-12);
    }
    double boundary = 0, total = 0;
    for (int f = 0; f < nf; f++) {
        vel[f] = sin(.37 * f);
        if (m->faces[f].lo < 0)
            boundary -= vel[f] * m->faces[f].area;
        if (m->faces[f].hi < 0)
            boundary += vel[f] * m->faces[f].area;
    }
    cfd_refined_flux_balance(m, vel, balance);
    for (int c = 0; c < nc; c++)
        total += balance[c];
    assert(fabs(total - boundary) < 1e-11);
    printf("leaves=%d faces=%d levels_scale=%d shared_flux_balance=%.3g\n", nc, nf,
           m->lattice_scale, fabs(total - boundary));
    free(degree);
    free(store);
}
static void affine(CfdRefinedMesh *m) {
    CfdRefinedDiffusion *d = cfd_refined_diffusion_create(m, NULL);
    assert(d);
    int nc = m->cell_count, nf = m->face_count;
    double *store = calloc((size_t)2 * nc + 3 * nf, sizeof(double));
    assert(store);
    double *rhs = store, *p = rhs + nc, *bc = p + nc, *pf = bc + nf, *flux = pf + nf;
    for (int f = 0; f < nf; f++)
        bc[f] = 1 + 2 * m->faces[f].cx - 3 * m->faces[f].cy;
    CfdRefinedSolve report;
    assert(cfd_refined_diffusion_solve(d, rhs, bc, p, pf, flux, &report));
    double error = 0, flux_error = 0;
    for (int c = 0; c < nc; c++)
        error = fmax(error, fabs(p[c] - (1 + 2 * m->cells[c].cx - 3 * m->cells[c].cy)));
    for (int f = 0; f < nf; f++)
        flux_error = fmax(flux_error, fabs(flux[f] - (m->faces[f].axis == 0 ? -2 : 3)));
    printf("affine_error=%.3g flux_error=%.3g\n", error, flux_error);
    assert(error < 1e-8 && flux_error < 1e-7);
    cfd_refined_diffusion_destroy(d);
    free(store);
}
int main(void) {
    CfdRefinedMesh m, old;
    CfdRefinementRegion one = {1, .5, 3, 1.5, 1};
    assert(cfd_refined_mesh_init_regions(&m, 16, 8, 4, 2, &one, 1, 10000));
    assert(cfd_refined_mesh_init(&old, 16, 8, 4, 2, 4, 2, 12, 6));
    assert(m.cell_count == old.cell_count && m.face_count == old.face_count);
    for (int c = 0; c < m.cell_count; c++)
        assert(m.cells[c].cx == old.cells[c].cx && m.cells[c].cy == old.cells[c].cy &&
               m.cells[c].volume == old.cells[c].volume);
    for (int f = 0; f < m.face_count; f++)
        assert(m.faces[f].lo == old.faces[f].lo && m.faces[f].hi == old.faces[f].hi &&
               m.faces[f].area == old.faces[f].area);
    geometry(&m);
    cfd_refined_mesh_destroy(&m);
    cfd_refined_mesh_destroy(&old);
    CfdRefinementRegion regions[] = {
        {1, .5, 3, 1.5, 1}, {1.4, .65, 1.6, .85, 4}, {2.4, 1.15, 2.6, 1.35, 3}};
    assert(!cfd_refined_mesh_init_regions(&m, 16, 8, 4, 2, regions, 3, 128));
    assert(!m.cells && !m.faces);
    assert(cfd_refined_mesh_init_regions(&m, 16, 8, 4, 2, regions, 3, 10000));
    geometry(&m);
    affine(&m);
    assert(cfd_refined_mesh_remove_rectangle(&m, 1.5, .75, 2.5, 1.25));
    double volume = 0, area = 0, normal[2] = {0};
    for (int c = 0; c < m.cell_count; c++)
        volume += m.cells[c].volume;
    for (int f = 0; f < m.face_count; f++)
        if (m.faces[f].boundary == CFD_REFINED_SOLID) {
            area += m.faces[f].area;
            normal[m.faces[f].axis] += (m.faces[f].lo >= 0 ? 1 : -1) * m.faces[f].area;
        }
    assert(fabs(volume - 7.5) < 1e-12 && fabs(area - 3) < 1e-12 &&
           fabs(normal[0]) + fabs(normal[1]) < 1e-12);
    cfd_refined_mesh_destroy(&m);
    CfdRefinementRegion odd = {1.13, .37, 2.61, 1.43, 3};
    assert(cfd_refined_mesh_init_regions(&m, 17, 13, 4, 2, &odd, 1, 10000));
    geometry(&m);
    affine(&m);
    cfd_refined_mesh_destroy(&m);
    CfdRefinementRegion deep = {1.4999, .7499, 1.5001, .7501, 10};
    assert(cfd_refined_mesh_init_regions(&m, 16, 8, 4, 2, &deep, 1, 10000));
    geometry(&m);
    assert(m.cell_count < 10000);
    cfd_refined_mesh_destroy(&m);
    puts("sparse local mesh parity, budget, deep balance, transfer, geometry and affine diffusion "
         "passed");
    return 0;
}
