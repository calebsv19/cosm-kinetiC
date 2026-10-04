#include "app/cfd_open2d_budget.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static double input(CfdOpen2DEnergy e) {
    return e.pressure_work_w + e.viscous_work_w - e.outward_kinetic_flux_w;
}
static double run(int n, double dt) {
    CfdOpen2D c;
    assert(cfd_open2d_init(&c, n, n, 4, 2, .5, 1, .1, .002));
    assert(cfd_open2d_set_obstacle(&c, 3 * n / 8, 3 * n / 8, 5 * n / 8, 5 * n / 8));
    /* Exclude the initial discontinuous clipping/projection impulse explicitly.
     * This gate measures the evolving obstacle wake over physical time .1..1.1. */
    for (int t = 0; t < (int)lround(.1 / dt); t++)
        assert(cfd_open2d_step(&c, dt));
    CfdOpen2DEnergy a, b;
    assert(cfd_open2d_energy(&c, &a));
    double initial = a.kinetic_energy_j, work = 0, dissipation = 0;
    for (int t = 0; t < (int)lround(1 / dt); t++) {
        assert(cfd_open2d_step(&c, dt));
        assert(cfd_open2d_energy(&c, &b));
        work += .5 * dt * (input(a) + input(b));
        dissipation += .5 * dt * (a.dissipation_w + b.dissipation_w);
        a = b;
    }
    double residual = (work - dissipation - (a.kinetic_energy_j - initial)) / fabs(work);
    printf("n=%d dt=%.9g window_start=.1 window_end=1.1 input_work_j=%.12g dissipation_j=%.12g "
           "kinetic_change_j=%.12g relative_residual=%.12g\n",
           n, dt, work, dissipation, a.kinetic_energy_j - initial, residual);
    fflush(stdout);
    cfd_open2d_destroy(&c);
    return fabs(residual);
}
int main(void) {
    double coarse = run(16, .001), medium = run(32, .001), fine = run(64, .001),
           temporal = run(64, .0005);
    assert(medium < coarse && fine < medium);
    assert(fine < .02 && temporal < .02);
    assert(fabs(temporal - fine) < .002);
    puts("Bounded masked transient energy: spatial refinement, 2 percent fine gate and timestep "
         "refinement passed");
}
