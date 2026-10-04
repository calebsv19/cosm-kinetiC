#include "app/cfd_refined_channel.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    CfdRefinedMesh m;
    assert(cfd_refined_mesh_init(&m, 8, 4, 4, 2, 2, 1, 6, 3));
    CfdRefinedChannel a, b;
    assert(cfd_refined_channel_init(&a, &m, 1, .1, .02));
    assert(cfd_refined_channel_init(&b, &m, 2, .2, .02));
    assert(!cfd_refined_channel_step(&a, 10));
    assert(!a.failed && a.tick == 0);
    double *force = calloc(m.cell_count, sizeof(double));
    assert(force);
    force[0] = NAN;
    assert(!cfd_refined_channel_step_forced(&a, .01, force, NULL));
    assert(!a.failed && a.tick == 0);
    free(force);
    for (int k = 0; k < 4; k++) {
        assert(cfd_refined_channel_step(&a, .01));
        assert(cfd_refined_channel_step(&b, .01));
    }
    for (int i = 0; i < m.cell_count; i++) {
        assert(fabs(a.u[i] - b.u[i]) < 1e-13);
        assert(fabs(a.v[i] - b.v[i]) < 1e-13);
        assert(fabs(2 * a.p[i] - b.p[i]) < 1e-12);
    }
    assert(!cfd_refined_channel_step(&a, .02));
    assert(a.tick == 4 && !a.failed);
    for (int f = 0; f < m.face_count; f++) {
        assert(isnan(a.face_p[f]));
        double component = m.faces[f].axis == 0 ? a.face_u[f] : a.face_v[f];
        assert(component == a.normal[f]);
    }
    cfd_refined_channel_destroy(&a);
    cfd_refined_channel_destroy(&b);
    cfd_refined_mesh_destroy(&m);
    puts("mixed channel units, final face velocities and rejected-input state preservation passed");
}
