#include "app/cfd_channel.h"
#include "app/cfd_surface_force.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void close_to(double a, double b) { assert(fabs(a - b) < 1e-9); }
static void calibration(void) {
    double zero[3] = {0}, g[3][3] = {{0}}, u[3] = {2, 3, 0}, n[3] = {1, 0, 0};
    CfdSurfaceFlux f;
    g[0][1] = 4;
    g[1][0] = 2;
    assert(cfd_surface_flux(2, .5, 7, 3, n, u, zero, (const double (*)[3])g, &f));
    close_to(f.pressure_on_fluid_n[0], -21);
    close_to(f.viscous_on_fluid_n[1], 9);
    close_to(f.outward_mass_kg_s, 12);
    close_to(f.outward_momentum_n[0], 24);
    close_to(f.outward_momentum_n[1], 36);
    assert(cfd_surface_flux(2, .5, 7, 3, n, u, u, (const double (*)[3])g, &f));
    close_to(f.outward_mass_kg_s, 0);
    u[0] = -2;
    assert(cfd_surface_flux(2, .5, 7, 3, n, u, zero, (const double (*)[3])g, &f));
    close_to(f.outward_mass_kg_s, -12);
    close_to(f.outward_momentum_n[0], 24); // backflow is not clipped
    n[0] = 2;
    assert(!cfd_surface_flux(2, .5, 7, 3, n, u, zero, (const double (*)[3])g, &f));
    n[0] = 1;
    assert(!cfd_surface_flux(2, .5, NAN, 3, n, u, zero, (const double (*)[3])g, &f));
    /* Closed rectangular body, n_fluid=-n_body. p=p0+gradient*x.
     * Fluid-on-body force is -volume*gradient, invariant to pressure gauge. */
    for (int gauge = 0; gauge < 2; gauge++) {
        double total[3] = {0}, size[3] = {2, 3, 4}, gradient[3] = {.2, -.3, .4};
        for (int axis = 0; axis < 3; axis++)
            for (int side = -1; side <= 1; side += 2) {
                double normal[3] = {0}, grad[3][3] = {{0}};
                normal[axis] = -side;
                double area = 24 / size[axis],
                       p = 1000 * gauge + gradient[axis] * side * size[axis] / 2;
                assert(cfd_surface_flux(1, .1, p, area, normal, zero, zero,
                                        (const double (*)[3])grad, &f));
                for (int k = 0; k < 3; k++)
                    total[k] -= f.pressure_on_fluid_n[k];
            }
        for (int k = 0; k < 3; k++)
            close_to(total[k], -24 * gradient[k]);
    }
}
static void channel(double gradient) {
    CfdChannel c;
    double dims[3] = {2, 1, .5};
    assert(cfd_channel_init(&c, 64, dims, 1, .1, gradient, 0, 0));
    double max_residual = 0;
    for (int t = 0; t < 800; t++) {
        double old = 0;
        for (int j = 0; j < c.n; j++)
            old += c.rho * c.u[j] * c.height / c.n * c.length * c.width;
        assert(cfd_channel_step(&c, .05));
        double force = 0, adv = 0, mass = 0, current = 0;
        for (int j = 0; j < c.n; j++) {
            current += c.rho * c.u[j] * c.height / c.n * c.length * c.width;
            for (int side = -1; side <= 1; side += 2) {
                double n[3] = {side, 0, 0}, u[3] = {c.u[j], 0, 0}, z[3] = {0}, g[3][3] = {{0}};
                CfdSurfaceFlux f;
                assert(cfd_surface_flux(c.rho, c.mu, side < 0 ? gradient * c.length : 0,
                                        c.height / c.n * c.width, n, u, z, (const double (*)[3])g,
                                        &f));
                force += f.pressure_on_fluid_n[0];
                adv += f.outward_momentum_n[0];
                mass += f.outward_mass_kg_s;
            }
        }
        for (int side = -1; side <= 1; side += 2) {
            double n[3] = {0, side, 0}, z[3] = {0}, g[3][3] = {{0}};
            CfdSurfaceFlux f;
            g[0][1] = c.tau[side < 0 ? 0 : c.n] / c.mu;
            assert(cfd_surface_flux(c.rho, c.mu, 0, c.length * c.width, n, z, z,
                                    (const double (*)[3])g, &f));
            force += f.viscous_on_fluid_n[0];
        }
        close_to(mass, 0);
        close_to(adv, 0);
        max_residual = fmax(max_residual, fabs((current - old) / .05 + adv - force));
    }
    assert(max_residual < 1e-9);
    close_to(c.tau[0] * c.length * c.width, gradient * c.height * c.length * c.width / 2);
    printf("channel gradient=%g max_transient_surface_balance_N=%.12g wall_load_N=%.12g\n",
           gradient, max_residual, c.tau[0] * c.length * c.width);
}
int main(void) {
    calibration();
    channel(.1);
    channel(-.1);
    puts("Surface traction calibration and reduced-channel balance passed; no obstacle or outlet "
         "solve qualified.");
}
