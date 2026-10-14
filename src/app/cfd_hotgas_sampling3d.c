#include "app/cfd_hotgas_domain3d.h"
#include <math.h>

static bool cell_values(const CfdHotGas3d *s, int q, double *temperature, double *advected) {
    double p, c;
    if (!cfd_hotgas3d_primitive(s, s->state + 5 * q, temperature, &p, &c))
        return false;
    *advected = (s->state[5 * q + 4] + p) * s->state[5 * q + 3] / s->state[5 * q];
    return isfinite(*advected);
}
bool cfd_hotgas_domain3d_sample(const CfdHotGas3d *s, const CfdHotGasDomain *d, double height,
                                double *temperature, double *conducted, double *advected) {
    if (!temperature || !conducted || !advected || !cfd_hotgas_domain3d_validate(s, d) ||
        !isfinite(height) || height < 0 || height > s->grid.length[2] - .5 * s->grid.h[2] ||
        d->face[4].kind != CFD_HOTGAS_SLIP_WALL || d->face[4].wall_temperature_k <= 0 ||
        d->face[5].kind == CFD_HOTGAS_PERIODIC)
        return false;
    int plane = s->grid.n[0] * s->grid.n[1], nz = s->grid.n[2];
    double h = s->grid.h[2];
    for (int q = 0; q < plane; ++q) {
        double location = height / h;
        int lower = (int)floor(location);
        if (lower >= nz)
            lower = nz - 1;
        double f0, f1;
        if (!cfd_hotgas_domain3d_conduction(s, d, q + (lower == 0 ? 0 : lower - 1) * plane, 2,
                                            lower == 0 ? 4 : -1, &f0) ||
            !cfd_hotgas_domain3d_conduction(s, d, q + lower * plane, 2, lower == nz - 1 ? 5 : -1,
                                            &f1))
            return false;
        conducted[q] = -((1 - (location - lower)) * f0 + (location - lower) * f1);
        double position = location - .5, t0, t1, a0, a1, fraction;
        if (position < 0) {
            t0 = d->face[4].wall_temperature_k;
            a0 = 0;
            fraction = 2 * location;
            if (!cell_values(s, q, &t1, &a1))
                return false;
        } else if (position >= nz - 1) {
            if (!cell_values(s, q + (nz - 1) * plane, &t0, &a0))
                return false;
            t1 = t0;
            a1 = a0;
            fraction = 0;
        } else {
            int j = (int)floor(position);
            fraction = position - j;
            if (!cell_values(s, q + j * plane, &t0, &a0) ||
                !cell_values(s, q + (j + 1) * plane, &t1, &a1))
                return false;
        }
        temperature[q] = (1 - fraction) * t0 + fraction * t1;
        advected[q] = (1 - fraction) * a0 + fraction * a1;
    }
    return true;
}
