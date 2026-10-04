#include "app/cfd_mac2d_force_check.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void flat_wall_reference(void) {
    CfdMac2D c;
    double dims[3] = {4, 2, .5};
    int n = 32;
    assert(cfd_mac2d_init(&c, n, n, dims, 1, .1, 0, 0, 0, 0));
    assert(cfd_mac2d_set_obstacle(&c, 12, 12, 20, 20));
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            double y = (j + .5) * 2 / n;
            c.u[j * n + i] = y > 1.25 ? 3 * (y - 1.25) : y < .75 ? y - .75 : 0;
        }
    CfdMac2DForceCheck a;
    assert(cfd_mac2d_force_check(&c, 4, 4, 28, 28, &a));
    /* Top and bottom planar shears: mu*(3-1)*length*width. */
    assert(fabs(a.surface_viscous_x_n - .1 * (3 - 1) * 1 * .5) < 1e-12);
    assert(a.surface_pressure_x_n == 0);
    assert(a.higher_order_available && fabs(a.cubic_viscous_x_n - .1) < 1e-12);
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            double x = (i + .5) * 4 / n;
            c.p[j * n + i] = x < 1.5 ? x * x : 0;
        }
    assert(cfd_mac2d_force_check(&c, 4, 4, 28, 28, &a));
    assert(fabs(a.quadratic_pressure_x_n - 2.25 * .5 * .5) < 1e-12);
    cfd_mac2d_destroy(&c);
    puts("Independent planar-wall shear reconstruction passed");
}
int main(void) {
    flat_wall_reference();
    for (int n = 16; n <= 64; n *= 2) {
        CfdMac2D c;
        double dims[3] = {4, 2, .5};
        assert(cfd_mac2d_init(&c, n, n, dims, 1, .1, .01, 0, 0, 0));
        assert(cfd_mac2d_set_obstacle(&c, 3 * n / 8, 3 * n / 8, 5 * n / 8, 5 * n / 8));
        double dt = .01;
        CfdMac2DForceCheck a, b;
        /* Fixed physical control surfaces, independently varied in X and Y.
         * Keep the momentum derivative: neither endpoint is assumed steady. */
        CfdMac2DForceCheck before[4], after[4];
        for (int t = 0; t < 1000; t++) {
            bool observe = t == 499 || t == 999;
            if (observe)
                for (int k = 0; k < 4; k++) {
                    int x = (k & 1) ? n / 4 : n / 8;
                    int y = (k & 2) ? n / 4 : n / 8;
                    assert(cfd_mac2d_force_check(&c, x, y, n - x, n - y, &before[k]));
                }
            assert(cfd_mac2d_step(&c, dt));
            if (observe)
                for (int k = 0; k < 4; k++) {
                    int x = (k & 1) ? n / 4 : n / 8;
                    int y = (k & 2) ? n / 4 : n / 8;
                    assert(cfd_mac2d_force_check(&c, x, y, n - x, n - y, &after[k]));
                    double rate =
                        (after[k].cv_momentum_x_kg_m_s - before[k].cv_momentum_x_kg_m_s) / dt;
                    double force = after[k].cv_pressure_x_n + after[k].cv_viscous_x_n +
                                   after[k].cv_drive_x_n - after[k].cv_advective_x_n - rate;
                    double traction = after[k].quadratic_pressure_x_n + after[k].cubic_viscous_x_n;
                    assert(after[k].higher_order_available && isfinite(force) && force > 0);
                    printf("cvcheck n=%d time=%.12g placement=%d force=%.12g momentum_rate=%.12g "
                           "traction=%.12g mismatch=%.12g\n",
                           n, (t + 1) * dt, k, force, rate, traction,
                           fabs(traction - force) / fabs(force));
                    fflush(stdout);
                }
        }
        assert(cfd_mac2d_force_check(&c, n / 8, n / 8, 7 * n / 8, 7 * n / 8, &a));
        assert(cfd_mac2d_step(&c, dt));
        assert(cfd_mac2d_force_check(&c, n / 8, n / 8, 7 * n / 8, 7 * n / 8, &b));
        double cv = b.cv_pressure_x_n + b.cv_viscous_x_n + b.cv_drive_x_n - b.cv_advective_x_n -
                    (b.cv_momentum_x_kg_m_s - a.cv_momentum_x_kg_m_s) / dt;
        double surface = b.surface_pressure_x_n + b.surface_viscous_x_n;
        printf("n=%d pressure=%.12g viscous=%.12g surface=%.12g control_volume=%.12g "
               "normal_strain_artifact=%.12g relative_mismatch=%.12g higher_order_surface=%.12g "
               "higher_order_mismatch=%.12g\n",
               n, b.surface_pressure_x_n, b.surface_viscous_x_n, surface, cv,
               b.reconstructed_normal_viscous_x_n, fabs(surface - cv) / fabs(cv),
               b.quadratic_pressure_x_n + b.cubic_viscous_x_n,
               fabs(b.quadratic_pressure_x_n + b.cubic_viscous_x_n - cv) / fabs(cv));
        fflush(stdout);
        assert(isfinite(cv) && cv > 0 && surface > 0);
        cfd_mac2d_destroy(&c);
    }
}
