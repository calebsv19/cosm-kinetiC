#include "app/cfd_mac2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

/* A pulse next to a rectangular corner. Integrate diffusive flux over the
 * dual-cell boundary: half of the touching face has a half-cell wall distance,
 * the other half has a full-cell distance to a constrained normal velocity.
 * The remaining three faces have a full-cell distance. Check the predictor
 * before pressure correction, for both orientations and two grid spacings. */
static void check(int n, int component, int corner, int side) {
    CfdMac2D c;
    double dims[3] = {4, 2, .5}, q = .01, dt = 1e-5, mu = .1;
    assert(cfd_mac2d_init(&c, n, n, dims, 1, mu, 0, 0, 0, 0));
    assert(cfd_mac2d_set_obstacle(&c, n / 4, n / 4, n / 2, n / 2));
    double dx = dims[0] / n, dy = dims[1] / n;
    int along = corner == 1 ? n / 4 : corner == 2 ? n / 2 : 3 * n / 8;
    int across = side < 0 ? n / 4 - 1 : n / 2;
    int i = component ? across : along;
    int j = component ? along : across;
    int k = j * n + i;
    if (component)
        c.v[k] = q;
    else
        c.u[k] = q;
    double face_length = component ? dy : dx;
    double spacing = component ? dx : dy;
    double other_spacing = component ? dy : dx;
    double wall_length = corner ? face_length / 2 : face_length;
    double near_flux =
        -mu * q * (wall_length / (spacing / 2) + (face_length - wall_length) / spacing);
    double other_flux = -mu * q * (face_length / spacing + 2 * spacing / other_spacing);
    double acceleration = (near_flux + other_flux) / (dx * dy);
    double advection = .5 * q * q / (component ? dy : dx);
    double expected = q + dt * (acceleration - advection);
    assert(cfd_mac2d_step(&c, dt) && c.substeps == 1);
    double actual = component ? c.vt[k] : c.ut[k];
    assert(fabs(actual - expected) < 1e-13);
    assert(fabs(c.momentum_residual) < 1e-10);
    cfd_mac2d_destroy(&c);
}
int main(void) {
    for (int n = 16; n <= 32; n *= 2)
        for (int component = 0; component < 2; component++)
            for (int corner = 0; corner < 3; corner++)
                for (int side = -1; side <= 1; side += 2)
                    check(n, component, corner, side);
    puts("Corner and straight-wall predictor flux quadrature passed in both orientations");
}
