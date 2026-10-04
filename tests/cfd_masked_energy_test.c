#include "app/cfd_open2d_budget.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    for (int n = 16; n <= 64; n *= 2) {
        CfdOpen2D c;
        assert(cfd_open2d_init(&c, n, n, 4, 2, .5, 1, .1, .002));
        /* Manufactured two parabolic slots separated by a stationary solid
         * band. This is a quadrature calibration, not a supported scene preset. */
        c.solid = calloc(n * n, 1);
        assert(c.solid);
        double gap = .75, mean = .002;
        for (int j = 0; j < n; j++) {
            double y = 2. * (j + .5) / n;
            bool solid = y > .75 && y < 1.25;
            double r = (y > 1.25 ? y - 1.25 : y) / gap;
            for (int i = 0; i <= n; i++)
                c.u[j * (n + 1) + i] = solid ? 0 : 6 * mean * r * (1 - r);
            for (int i = 0; i < n; i++) {
                c.solid[j * n + i] = solid;
                c.p[j * n + i] = 12 * c.mu * mean / (gap * gap) * (4 - 4. * (i + .5) / n);
            }
        }
        CfdOpen2DEnergy e;
        assert(cfd_open2d_energy(&c, &e));
        double exact = 2 * 12 * c.mu * mean * mean * c.length * c.width / gap;
        double cells = 3 * n / 8., expected = 1 / (cells * cells);
        double error = fabs(e.dissipation_w - exact) / exact;
        printf("n=%d masked_dissipation_relative_error=%.12g expected=%.12g\n", n, error, expected);
        assert(fabs(error - expected) < 1e-12);
        assert(fabs(e.viscous_work_w) < 1e-15 && fabs(e.outward_kinetic_flux_w) < 1e-15);
        cfd_open2d_destroy(&c);
    }
    puts("Masked physical dissipation recovers known slot quadrature refinement");
}
