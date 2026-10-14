#include "app/cfd_hotgas_reconstruction3d.h"
#include <math.h>

/* Ducros anti-vorticity ratio, evaluated on accepted cell velocities. No
 * threshold or dimensional epsilon: scale before squaring, return zero for
 * a constant field. This is a compression/acoustic guard, not a shock proof. */
double cfd_hotgas_compression3d(const CfdHotGas3d *s, const CfdHotGasDomain *d, int q) {
    double gradient[3][3];
    for (int axis = 0, stride = 1; axis < 3; stride *= s->grid.n[axis++]) {
        int coordinate = q / stride % s->grid.n[axis];
        double velocities[2][3];
        for (int side = 0; side < 2; ++side) {
            bool missing = (side == 0 ? coordinate == 0 : coordinate == s->grid.n[axis] - 1) &&
                           d->face[2 * axis + side].kind != CFD_HOTGAS_PERIODIC;
            int neighbor = missing ? q : cfd_cartesian3d_neighbor(&s->grid, q, axis, side ? 1 : -1);
            const double *u = s->state + 5 * neighbor;
            for (int component = 0; component < 3; ++component)
                velocities[side][component] = u[component + 1] / u[0] *
                    (missing && component == axis &&
                     d->face[2 * axis + side].kind == CFD_HOTGAS_SLIP_WALL ? -1 : 1);
        }
        for (int component = 0; component < 3; ++component)
            gradient[component][axis] = (velocities[1][component] - velocities[0][component]) /
                                       (2 * s->grid.h[axis]);
    }
    double divergence = gradient[0][0] + gradient[1][1] + gradient[2][2];
    double curl[3] = {gradient[2][1] - gradient[1][2], gradient[0][2] - gradient[2][0],
                      gradient[1][0] - gradient[0][1]};
    double scale = fmax(fabs(divergence), fmax(fabs(curl[0]), fmax(fabs(curl[1]), fabs(curl[2]))));
    if (scale == 0)
        return 0;
    if (!isfinite(scale))
        return 1;
    double compression = divergence / scale, denominator = compression * compression;
    for (int a = 0; a < 3; ++a)
        denominator += (curl[a] / scale) * (curl[a] / scale);
    return compression * compression / denominator;
}
