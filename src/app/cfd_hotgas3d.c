#include "app/cfd_hotgas3d.h"
#include <math.h>
#include <string.h>
bool cfd_hotgas3d_energy(const CfdHotGas3d *s, double t, double *energy, double *cv) {
    if (!s || !energy || !cv || !isfinite(t) || t<=0) return false;
    if (s->caloric.active) {
        if (fabs(s->gas_constant_j_kg_k-s->caloric.gas_constant)>1e-12*s->caloric.gas_constant) return false;
        double cp;
        if (!cfd_hotgas_caloric_properties(&s->caloric,t,energy,&cp)) return false;
        *cv=cp-s->gas_constant_j_kg_k;
    } else { *cv=s->gas_constant_j_kg_k/(s->gamma-1); *energy=(*cv)*t; }
    return isfinite(*energy) && isfinite(*cv) && *cv>0;
}
bool cfd_hotgas3d_init(CfdHotGas3d *s, const int n[3], const double length[3], double gamma,
                       double gas_constant) {
    if (!s || !isfinite(gamma) || gamma <= 1 || gamma > 2 || !isfinite(gas_constant) ||
        gas_constant <= 0)
        return false;
    memset(s, 0, sizeof(*s));
    if (!cfd_cartesian3d_init(&s->grid, n, length) || s->grid.count > 65536)
        return false;
    s->gamma = gamma;
    s->gas_constant_j_kg_k = gas_constant;
    s->state = cfd_memory_calloc((size_t)10 * s->grid.count, sizeof(double));
    if (!s->state)
        return false;
    s->candidate = s->state + 5 * s->grid.count;
    return true;
}
void cfd_hotgas3d_destroy(CfdHotGas3d *s) {
    if (s) {
        cfd_memory_free(s->state);
        s->state = NULL;
        s->candidate = NULL;
    }
}
bool cfd_hotgas3d_primitive(const CfdHotGas3d *s, const double u[5], double *temperature,
                            double *pressure, double *sound) {
    if (!s || !u || !temperature || !pressure || !sound)
        return false;
    for (int i = 0; i < 5; ++i)
        if (!isfinite(u[i]))
            return false;
    if (u[0] <= 0)
        return false;
    double kinetic = (u[1] * u[1] + u[2] * u[2] + u[3] * u[3]) / (2 * u[0]);
    if (s->caloric.active) {
        if (fabs(s->gas_constant_j_kg_k-s->caloric.gas_constant)>1e-12*s->caloric.gas_constant) return false;
        double t, e, cp;
        if (!cfd_hotgas_caloric_temperature(&s->caloric,(u[4]-kinetic)/u[0],&t) ||
            !cfd_hotgas_caloric_properties(&s->caloric,t,&e,&cp)) return false;
        double p=u[0]*s->gas_constant_j_kg_k*t;
        double c=sqrt(cp/(cp-s->gas_constant_j_kg_k)*s->gas_constant_j_kg_k*t);
        if (!isfinite(p) || !isfinite(c) || p<=0) return false;
        *temperature=t; *pressure=p; *sound=c; return true;
    }
    double p = (s->gamma - 1) * (u[4] - kinetic);
    double t = p / (u[0] * s->gas_constant_j_kg_k), c = sqrt(s->gamma * p / u[0]);
    if (!isfinite(t) || !isfinite(p) || !isfinite(c) || t <= 0 || p <= 0)
        return false;
    *temperature = t;
    *pressure = p;
    *sound = c;
    return true;
}
static void flux(const double u[5], double pressure, int axis, double out[5]) {
    double velocity = u[axis + 1] / u[0];
    out[0] = u[axis + 1];
    for (int i = 1; i < 4; ++i)
        out[i] = u[i] * velocity + (i == axis + 1 ? pressure : 0);
    out[4] = (u[4] + pressure) * velocity;
}
bool cfd_hotgas3d_step(CfdHotGas3d *s, const double *source, double dt) {
    if (!s || !s->state || !source || !isfinite(dt) || dt <= 0)
        return false;
    int n = s->grid.count;
    double maximum[3] = {0};
    for (int q = 0; q < n; ++q) {
        double temperature, pressure, sound;
        if (!cfd_hotgas3d_primitive(s, s->state + 5 * q, &temperature, &pressure, &sound))
            return false;
        for (int a = 0; a < 3; ++a)
            maximum[a] = fmax(maximum[a], fabs(s->state[5 * q + a + 1] / s->state[5 * q]) + sound);
        double trial[5];
        for (int i = 0; i < 5; ++i) {
            if (!isfinite(source[5 * q + i]))
                return false;
            trial[i] = s->state[5 * q + i] + source[5 * q + i];
            s->candidate[5 * q + i] = trial[i];
        }
        if (!cfd_hotgas3d_primitive(s, trial, &temperature, &pressure, &sound))
            return false;
        for (int a = 0; a < 3; ++a)
            maximum[a] = fmax(maximum[a], fabs(trial[a + 1] / trial[0]) + sound);
    }
    double courant = 0;
    for (int a = 0; a < 3; ++a)
        courant += dt * maximum[a] / s->grid.h[a];
    if (!isfinite(courant) || courant > .4)
        return false;
    /* Each periodic face is visited once. Equal/opposite conservative fluxes. */
    for (int q = 0; q < n; ++q)
        for (int a = 0; a < 3; ++a) {
            int right = cfd_cartesian3d_neighbor(&s->grid, q, a, 1);
            const double *left_u = s->state + 5 * q, *right_u = s->state + 5 * right;
            double t, pl, pr, cl, cr, fl[5], fr[5];
            if (!cfd_hotgas3d_primitive(s, left_u, &t, &pl, &cl) ||
                !cfd_hotgas3d_primitive(s, right_u, &t, &pr, &cr))
                return false;
            double speed =
                fmax(fabs(left_u[a + 1] / left_u[0]) + cl, fabs(right_u[a + 1] / right_u[0]) + cr);
            flux(left_u, pl, a, fl);
            flux(right_u, pr, a, fr);
            for (int i = 0; i < 5; ++i) {
                double transfer = dt / s->grid.h[a] *
                                  (.5 * (fl[i] + fr[i]) - .5 * speed * (right_u[i] - left_u[i]));
                s->candidate[5 * q + i] -= transfer;
                s->candidate[5 * right + i] += transfer;
            }
        }
    for (int q = 0; q < n; ++q) {
        double t, p, c;
        if (!cfd_hotgas3d_primitive(s, s->candidate + 5 * q, &t, &p, &c))
            return false;
    }
    memcpy(s->state, s->candidate, (size_t)5 * n * sizeof(double));
    return true;
}
