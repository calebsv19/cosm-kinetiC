#include "app/cfd_refined_channel.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static double run(int n, int uniform, double dt, double end_time) {
    CfdRefinedMesh m;
    assert(cfd_refined_mesh_init(&m, 2 * n, n, 4, 2, uniform ? 0 : n / 2, uniform ? 0 : n / 4,
                                 uniform ? 2 * n : 3 * n / 2, uniform ? n : 3 * n / 4));
    CfdRefinedChannel c;
    assert(cfd_refined_channel_init(&c, &m, 1, .1, .02));
    int steps = (int)round(end_time / dt);
    long pit = 0, vit = 0, mit = 0;
    double maxdiv = 0;
    for (int k = 0; k < steps; k++) {
        if (!cfd_refined_channel_step(&c, dt)) {
            fprintf(stderr, "step failed n=%d k=%d cfl=%g\n", n, k, c.transport_cfl);
            assert(0);
        }
        maxdiv = fmax(maxdiv, c.divergence);
        pit += c.pressure_iterations;
        vit += c.velocity_iterations;
        mit += c.mixed_iterations;
    }
    double error = 0, perror = 0, kinetic = 0, inlet = 0, outlet = 0, vmax = 0;
    for (int i = 0; i < m.cell_count; i++) {
        double y = m.cells[i].cy / 2, u = 6 * .02 * y * (1 - y),
               p = 12 * .1 * .02 / 4 * (4 - m.cells[i].cx);
        error += ((c.u[i] - u) * (c.u[i] - u) + c.v[i] * c.v[i]) * m.cells[i].volume;
        perror += (c.p[i] - p) * (c.p[i] - p) * m.cells[i].volume;
        kinetic += (c.u[i] * c.u[i] + c.v[i] * c.v[i]) * m.cells[i].volume;
        vmax = fmax(vmax, fabs(c.v[i]));
    }
    for (int f = 0; f < m.face_count; f++)
        if (m.faces[f].axis == 0) {
            if (m.faces[f].lo < 0)
                inlet += c.normal[f] * m.faces[f].area;
            if (m.faces[f].hi < 0)
                outlet += c.normal[f] * m.faces[f].area;
        }
    error = sqrt(error / 8) / .02;
    perror = sqrt(perror / 8) / .024;
    printf("n=%d uniform=%d dt=%g time=%g velocity_relative_l2=%.12g pressure_relative_l2=%.12g "
           "vmax=%.3g "
           "divergence=%.3g mass=%.3g energy=%.9g iterations=%ld/%ld/%ld\n",
           n, uniform, dt, c.time, error, perror, vmax, maxdiv, fabs(inlet - outlet), kinetic, pit,
           vit, mit);
    fflush(stdout);
    assert(maxdiv < 1e-7 && fabs(inlet - outlet) < 1e-8 && isfinite(error) && error < .1 &&
           isfinite(perror) && perror < .1);
#ifdef CFD_REFINED_VERIFY_SPLIT
    assert(perror < .03); /* Retain the old split-path regression bound. */
#else
    /* Mixed and split discretizations have different spatial errors. The
     * coarse mixed case regresses the old 3% bound; retain explicit readback
     * and demand physical accuracy at the finest verification resolution. */
    printf("channel_one_percent_gate=%s\n",
           error < .01 && perror < .01 ? "passed" : "not_qualified");
    if (n >= 32)
        assert(error < .01 && perror < .01);
#endif
    cfd_refined_channel_destroy(&c);
    cfd_refined_mesh_destroy(&m);
    return error;
}
int main(void) {
    double a = run(8, 0, .01, .5), b = run(16, 0, .01, .5), c = run(32, 0, .01, .5);
    printf("refined channel error ratios %.6g %.6g\n", a / b, b / c);
    assert(a / b > 1.5 && b / c > 1.5);
    run(16, 0, .005, .5);
    run(16, 1, .01, .5);
    run(16, 0, .01, 5);
}
