#include "app/cfd_hotgas_reconstruction3d.h"
#include <math.h>

static double limited(double left, double right) {
    if (left * right <= 0)
        return 0;
    double magnitude = fmin(2 * fabs(left), fmin(.5 * fabs(left + right), 2 * fabs(right)));
    return copysign(magnitude, left);
}

static bool primitive(const CfdHotGas3d *s, int q, double v[5], double *t) {
    const double *u = s->state + 5 * q;
    double c;
    if (!cfd_hotgas3d_primitive(s, u, t, v + 4, &c))
        return false;
    v[0] = u[0];
    for (int a = 0; a < 3; ++a)
        v[a + 1] = u[a + 1] / u[0];
    return true;
}

bool cfd_hotgas_muscl_face(const CfdHotGas3d *s, const CfdHotGasDomain *d, int q,
                           int axis, int sign, double out[5]) {
    if (!s || !d || !out || !s->state || q < 0 || q >= s->grid.count ||
        axis < 0 || axis > 2 || (sign != -1 && sign != 1))
        return false;
    double own[5], low[5], high[5], t, ignored;
    if (!primitive(s, q, own, &t))
        return false;
    int stride = 1;
    for (int a = 0; a < axis; ++a)
        stride *= s->grid.n[a];
    int coordinate = q / stride % s->grid.n[axis];
    double half = d->gravity_m_s2[axis] * s->grid.h[axis] /
                  (2 * s->gas_constant_j_kg_k * t);
    if (!isfinite(half) || fabs(half) > 50)
        return false;
    for (int side = -1; side <= 1; side += 2) {
        bool missing = (side < 0 && coordinate == 0 && d->face[2 * axis].kind != CFD_HOTGAS_PERIODIC) ||
                       (side > 0 && coordinate == s->grid.n[axis] - 1 &&
                        d->face[2 * axis + 1].kind != CFD_HOTGAS_PERIODIC);
        int neighbor = missing ? q : cfd_cartesian3d_neighbor(&s->grid, q, axis, side);
        double *v = side < 0 ? low : high;
        if (!primitive(s, neighbor, v, &ignored))
            return false;
        if (missing && d->contact_flux == 3 &&
            d->face[2 * axis + (side > 0)].kind == CFD_HOTGAS_SLIP_WALL)
            v[axis + 1] = -v[axis + 1];
        /* Compare neighbors at the same local equilibrium coordinate. */
        double shift = missing ? 1 : exp(-side * 2 * half);
        v[0] *= shift;
        v[4] *= shift;
    }
    double v[5];
    for (int i = 0; i < 5; ++i)
        v[i] = own[i] + sign * .5 * limited(own[i] - low[i], high[i] - own[i]);
    double average = fabs(half) < 1e-6 ? 1 + half * half / 6 : sinh(half) / half;
    double factor = exp(sign * half) / average;
    v[0] *= factor;
    v[4] *= factor;
    out[0] = v[0];
    out[4] = v[4] / (s->gamma - 1);
    if (s->caloric.active) {
        double e, cv;
        if (!cfd_hotgas3d_energy(s,v[4]/(v[0]*s->gas_constant_j_kg_k),&e,&cv)) return false;
        out[4]=v[0]*e;
    }
    for (int a = 0; a < 3; ++a) {
        out[a + 1] = v[0] * v[a + 1];
        out[4] += .5 * v[0] * v[a + 1] * v[a + 1];
    }
    double p, c;
    return cfd_hotgas3d_primitive(s, out, &t, &p, &c);
}
