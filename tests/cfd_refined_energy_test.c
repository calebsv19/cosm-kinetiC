#include "app/cfd_refined_mixed.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    CfdRefinedMesh m;
    assert(cfd_refined_mesh_init(&m, 16, 8, 4, 2, 4, 2, 12, 6));
    CfdRefinedMixed *s = cfd_refined_mixed_create(&m, .03);
    assert(s);
    double *cells = calloc((size_t)5 * m.cell_count, sizeof(double)),
           *faces = calloc((size_t)4 * m.face_count, sizeof(double));
    assert(cells && faces);
    double *u = cells, *v = u + m.cell_count, *p = v + m.cell_count, *ru = p + m.cell_count,
           *rv = ru + m.cell_count;
    double *fu = faces, *fv = fu + m.face_count, *bu = fv + m.face_count, *bv = bu + m.face_count;
    for (int f = 0; f < m.face_count; f++)
        bu[f] = m.faces[f].cy;
    CfdRefinedMixedReport report;
    assert(cfd_refined_mixed_solve(s, 0, ru, rv, bu, bv, u, v, fu, fv, p, &report));
    for (int i = 0; i < m.cell_count; i++) {
        assert(fabs(u[i] - m.cells[i].cy) < 1e-9);
        assert(fabs(v[i]) + fabs(p[i]) < 1e-9);
    }
    CfdRefinedEnergy e;
    assert(cfd_refined_mixed_energy(s, 1.7, .4, NULL, NULL, &e));
    double expected = .03 * 1.7 * .4 * 8, kinetic = .5 * 1.7 * .4 * 4 * 8 / 3;
    printf("couette dissipation=%.15g boundary_power=%.15g expected=%.15g stabilization=%.3g "
           "kinetic_error=%.9g\n",
           e.strain_dissipation_w, e.stress_boundary_power_w, expected, e.stabilization_w,
           fabs(e.kinetic_j / kinetic - 1));
    assert(fabs(e.strain_dissipation_w - expected) < 1e-9);
    assert(fabs(e.stress_boundary_power_w - expected) < 1e-9);
    assert(fabs(e.discrete_diffusion_w - expected) < 1e-9 && fabs(e.stabilization_w) < 1e-9);
    assert(fabs(e.kinetic_j / kinetic - 1) < .005);
    assert(fabs(e.outward_kinetic_flux_w) < 1e-9 && e.body_force_power_w == 0);
    /* Observation scratch must not change the cached solution or next solve. */
    assert(cfd_refined_mixed_solve(s, 0, ru, rv, bu, bv, u, v, fu, fv, p, &report));
    assert(report.iterations == 0);
    free(cells);
    free(faces);
    cfd_refined_mixed_destroy(s);
    cfd_refined_mesh_destroy(&m);
    return 0;
}
