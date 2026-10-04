#include "app/cfd_refined_channel.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static double momentum(const CfdRefinedChannel *c) {
    double result = 0;
    for (int i = 0; i < c->mesh->cell_count; i++)
        result += .5 * c->u[i] * c->mesh->cells[i].volume;
    return result;
}
int main(void) {
    CfdRefinedMesh m;
#ifdef CFD_REFINED_VERIFY_LOCAL
    CfdRefinementRegion regions[] = {{1, .5, 3, 1.5, 1},
                                     {1.375, .625, 1.625, .875, 4},
                                     {2.375, .625, 2.625, .875, 4},
                                     {1.375, 1.125, 1.625, 1.375, 4},
                                     {2.375, 1.125, 2.625, 1.375, 4}};
    assert(cfd_refined_mesh_init_regions(&m, 16, 8, 4, 2, regions, 5, 10000));
#else
    assert(cfd_refined_mesh_init(&m, 16, 8, 4, 2, 4, 2, 12, 6));
#endif
    assert(cfd_refined_mesh_remove_rectangle(&m, 1.5, .75, 2.5, 1.25));
    CfdRefinedChannel c;
    assert(cfd_refined_channel_init(&c, &m, 1, .1, .002));
    double previous = momentum(&c), older = previous, previous_adv = 0, max_balance = 0,
           max_div = 0, dt = .005;
    for (int k = 0; k < 20; k++) {
        assert(cfd_refined_channel_step(&c, dt));
        CfdRefinedForce all, body;
        assert(cfd_refined_mixed_boundary_force(c.mixed_operator, -1, 1, .5, &all));
        assert(cfd_refined_mixed_boundary_force(c.mixed_operator, CFD_REFINED_SOLID, 1, .5, &body));
        double now = momentum(&c), adv = 0;
        for (int i = 0; i < m.cell_count; i++)
            adv += .5 * m.cells[i].volume * c.adv_u[i];
        double rate = k ? (1.5 * now - 2 * previous + .5 * older) / dt : (now - previous) / dt;
        double residual = all.total[0] + rate - (k ? 2 * adv - previous_adv : adv);
        max_balance = fmax(max_balance, fabs(residual));
        max_div = fmax(max_div, c.divergence);
        for (int f = 0; f < m.face_count; f++)
            if (m.faces[f].boundary == CFD_REFINED_SOLID)
                assert(fabs(c.face_u[f]) + fabs(c.face_v[f]) < 1e-14);
        assert(isfinite(body.total[0]) && body.total[0] > 0);
        older = previous;
        previous = now;
        previous_adv = adv;
    }
    printf("evolving_body time=%.6g closed_momentum_balance=%.12g divergence=%.12g\n", c.time,
           max_balance, max_div);
    assert(max_balance < 1e-9 && max_div < 1e-8);
    cfd_refined_channel_destroy(&c);
    cfd_refined_mesh_destroy(&m);
    return 0;
}
