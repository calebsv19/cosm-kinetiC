#include "app/cfd_open2d_budget.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void run(int n) {
    CfdOpen2D c;
    CfdOpen2DBudget a, b;
    assert(cfd_open2d_init(&c, n, n, 2, 1, .5, 1, .1, .05));
    double gradient = 12 * .1 * .05;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            c.p[j * n + i] = gradient * (2 - 2. * (i + .5) / n);
    assert(cfd_open2d_budget(&c, &a));
    double exact_force = .12 * .5, exact_power = .12 * .05 * .5;
    assert(fabs(a.pressure_force_x_n - exact_force) < 1e-12);
    assert(fabs(a.wall_force_x_n + exact_force) < 1e-12);
    double reference_error = fabs(a.dissipation_w - exact_power) / exact_power;
    assert(fabs(reference_error - 1. / (n * n)) < 1e-12);
    for (int t = 0; t < 2000; t++)
        assert(cfd_open2d_step(&c, .0005));
    assert(cfd_open2d_budget(&c, &a));
    assert(cfd_open2d_step(&c, .0005));
    assert(cfd_open2d_budget(&c, &b));
    double mr = (b.momentum_x_kg_m_s - a.momentum_x_kg_m_s) / .0005;
    double kr = (b.kinetic_energy_j - a.kinetic_energy_j) / .0005;
    double momentum = b.pressure_force_x_n + b.normal_viscous_force_x_n + b.wall_force_x_n -
                      b.outward_momentum_flux_n - mr;
    double energy =
        b.pressure_work_w + b.viscous_work_w - b.outward_kinetic_flux_w - b.dissipation_w - kr;
    printf("n=%d exact_dissipation_error=%.12g pressure_work=%.12g dissipation=%.12g "
           "momentum_residual=%.12g energy_residual=%.12g relative_momentum=%.12g "
           "relative_energy=%.12g\n",
           n, reference_error, b.pressure_work_w, b.dissipation_w, momentum, energy,
           fabs(momentum) / exact_force, fabs(energy) / exact_power);
    fflush(stdout);
    assert(fabs(momentum) / exact_force < 1e-7);
    /* Midpoint inlet parabola overestimates flow by 1/(2*n^2), while
     * midpoint squared shear underestimates dissipation by 1/n^2. */
    assert(fabs(energy / exact_power - 1.5 / (n * n)) < 1e-7);
    if (n == 32)
        assert(fabs(energy) / exact_power < .002);
    assert(cfd_open2d_set_obstacle(&c, 2, 2, 4, 4) == false);
    cfd_open2d_destroy(&c);
}
int main(void) {
    run(8);
    run(16);
    run(32);
    CfdOpen2D masked;
    CfdOpen2DBudget budget;
    assert(cfd_open2d_init(&masked, 16, 16, 2, 1, .5, 1, .1, .05));
    assert(cfd_open2d_set_obstacle(&masked, 4, 4, 8, 8));
    assert(!cfd_open2d_budget(&masked, &budget));
    cfd_open2d_destroy(&masked);
    puts("Empty-channel momentum/work/dissipation audit passed; wake/outlet energy not qualified");
}
