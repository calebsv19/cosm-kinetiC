#include "app/cfd_hotgas_flux3d.h"
#include <math.h>
#include <string.h>

bool cfd_hotgas_hllc3d(const CfdHotGas3d *s, const double l[5], const double r[5],
                       int axis, double output[5], double *speed) {
    if (!s || !l || !r || !output || !speed || axis < 0 || axis > 2)
        return false;
    double t, pl, pr, cl, cr;
    if (!cfd_hotgas3d_primitive(s, l, &t, &pl, &cl) ||
        !cfd_hotgas3d_primitive(s, r, &t, &pr, &cr))
        return false;
    double vl = l[axis + 1] / l[0], vr = r[axis + 1] / r[0];
    double sl = fmin(vl - cl, vr - cr), sr = fmax(vl + cl, vr + cr);
    double fl[5] = {l[axis + 1], l[1] * vl, l[2] * vl, l[3] * vl, (l[4] + pl) * vl};
    double fr[5] = {r[axis + 1], r[1] * vr, r[2] * vr, r[3] * vr, (r[4] + pr) * vr};
    fl[axis + 1] += pl;
    fr[axis + 1] += pr;
    double flux[5];
    if (sl >= 0) {
        memcpy(flux, fl, sizeof(flux));
    } else if (sr <= 0) {
        memcpy(flux, fr, sizeof(flux));
    } else {
        double al = l[0] * (sl - vl), ar = r[0] * (sr - vr);
        double middle = (pr - pl + al * vl - ar * vr) / (al - ar);
        if (!isfinite(middle) || middle <= sl || middle >= sr)
            return false;
        double stars[2][5];
        for (int side = 0; side < 2; ++side) {
            const double *u = side ? r : l;
            double wave = side ? sr : sl, v = side ? vr : vl, p = side ? pr : pl;
            double rho = u[0] * (wave - v) / (wave - middle);
            double star_pressure = p + u[0] * (wave - v) * (middle - v);
            if (!isfinite(star_pressure) || star_pressure <= 0)
                return false;
            double *star = stars[side];
            star[0] = rho;
            for (int a = 0; a < 3; ++a)
                star[a + 1] = rho * (a == axis ? middle : u[a + 1] / u[0]);
            star[4] = rho * (u[4] / u[0] + (middle - v) *
                               (middle + p / (u[0] * (wave - v))));
            double ignored;
            if (!cfd_hotgas3d_primitive(s, star, &t, &ignored, &ignored))
                return false;
        }
        int side = middle < 0;
        const double *u = side ? r : l, *f = side ? fr : fl;
        double wave = side ? sr : sl;
        for (int i = 0; i < 5; ++i)
            flux[i] = f[i] + wave * (stars[side][i] - u[i]);
    }
    for (int i = 0; i < 5; ++i)
        if (!isfinite(flux[i]))
            return false;
    double maximum = fmax(fabs(sl), fabs(sr));
    if (!isfinite(maximum))
        return false;
    memcpy(output, flux, sizeof(flux));
    *speed = maximum;
    return true;
}
