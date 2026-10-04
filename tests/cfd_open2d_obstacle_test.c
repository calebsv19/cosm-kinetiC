#include "app/cfd_open2d.h"
#include "app/cfd_open2d_force_check.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static int solid(const CfdOpen2D *c, int i, int j) {
    return i >= 0 && i < c->nx && j >= 0 && j < c->ny && c->solid[j * c->nx + i];
}
static CfdMac2DForceCheck measure(CfdOpen2D *c) {
    CfdMac2DForceCheck result;
    assert(cfd_open2d_force_check(c, c->nx / 8, c->ny / 8, 7 * c->nx / 8, 7 * c->ny / 8, &result));
    return result;
}
static void run(int n) {
    CfdOpen2D c, empty;
    assert(cfd_open2d_init(&c, n, n, 4, 2, .5, 1, .1, .02));
    assert(cfd_open2d_init(&empty, n, n, 4, 2, .5, 1, .1, .02));
    assert(!cfd_open2d_set_obstacle(&c, 0, n / 4, n / 2, n / 2));
    assert(!c.solid);
    assert(cfd_open2d_set_obstacle(&c, 3 * n / 8, 3 * n / 8, 5 * n / 8, 5 * n / 8));
    assert(!cfd_open2d_set_obstacle(&c, n / 4, n / 4, n / 2, n / 2));
    double max_div = 0, max_mass = 0, leak = 0, symmetry = 0, peak = 0;
    for (int tick = 0; tick < 1000; tick++) {
        assert(cfd_open2d_step(&c, .001));
        assert(cfd_open2d_step(&empty, .001));
        max_div = fmax(max_div, c.divergence);
        max_mass = fmax(max_mass, fabs(c.inlet_flux - c.outlet_flux));
        for (int j = 0; j < n; j++)
            for (int i = 0; i <= n; i++) {
                double u = c.u[j * (n + 1) + i];
                if (solid(&c, i - 1, j) || solid(&c, i, j))
                    leak = fmax(leak, fabs(u));
                symmetry = fmax(symmetry, fabs(u - c.u[(n - 1 - j) * (n + 1) + i]));
                peak = fmax(peak, fabs(u));
            }
        for (int j = 0; j <= n; j++)
            for (int i = 0; i < n; i++) {
                double v = c.v[j * n + i];
                if (solid(&c, i, j - 1) || solid(&c, i, j))
                    leak = fmax(leak, fabs(v));
                symmetry = fmax(symmetry, fabs(v + c.v[(n - j) * n + i]));
            }
    }
    assert(leak == 0 && max_div < 1e-8 && max_mass < 1e-8 && symmetry < 1e-8);
    assert(peak > .03 && peak < .2 && c.inlet_pressure > empty.inlet_pressure);
    assert(!cfd_open2d_set_obstacle(&c, n / 4, n / 4, n / 2, n / 2));
    CfdMac2DForceCheck before = measure(&c);
    assert(cfd_open2d_step(&c, .001));
    CfdMac2DForceCheck after = measure(&c);
    double cv = after.cv_pressure_x_n + after.cv_viscous_x_n - after.cv_advective_x_n -
                (after.cv_momentum_x_kg_m_s - before.cv_momentum_x_kg_m_s) / .001;
    double surface = after.surface_pressure_x_n + after.surface_viscous_x_n;
    double candidate = after.quadratic_pressure_x_n + after.cubic_viscous_x_n;
    assert(isfinite(cv) && cv > 0 && surface > 0);
    if (n == 64)
        assert(fabs(surface - cv) / cv < .02);
    printf("force n=%d time=%.9g pressure=%.12g viscous=%.12g surface=%.12g cv=%.12g "
           "mismatch=%.12g candidate_mismatch=%.12g\n",
           n, c.time, after.surface_pressure_x_n, after.surface_viscous_x_n, surface, cv,
           fabs(surface - cv) / cv, fabs(candidate - cv) / cv);
    printf("n=%d time=%.9g pressure=%.12g empty_pressure=%.12g mass_error=%.12g divergence=%.12g "
           "leakage=%.12g symmetry=%.12g peak=%.12g\n",
           n, c.time, c.inlet_pressure, empty.inlet_pressure, max_mass, max_div, leak, symmetry,
           peak);
    fflush(stdout);
    cfd_open2d_destroy(&c);
    cfd_open2d_destroy(&empty);
}
int main(int argc, char **argv) {
    int max_n = argc > 1 ? atoi(argv[1]) : 64;
    assert(max_n == 16 || max_n == 32 || max_n == 64);
    for (int n = 16; n <= max_n; n *= 2)
        run(n);
    puts("Open-boundary obstacle coupling passed; no reference drag qualification inferred");
}
