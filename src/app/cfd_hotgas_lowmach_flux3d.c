#include "app/cfd_hotgas_flux3d.h"
#include <math.h>
#include <string.h>

/* Hydrodynamic pressure correction of Minoshima & Miyoshi (2021),
 * equations 15/16, with unchanged physical signal speeds/contact velocity.
 * Evaluate the corrected star flux directly: F+S(U*-U) would restore the
 * uncorrected momentum dissipation. An additional characteristic guard restores
 * damping for pressure/normal-velocity jumps of acoustic strength; this is an
 * app-specific extension, not the unmodified published L-HLLC scheme. */
bool cfd_hotgas_lhllc3d(const CfdHotGas3d *s, const double l[5], const double r[5],
                        int axis, double compression, double output[5], double *speed) {
    if (!s || !l || !r || !output || !speed || axis < 0 || axis > 2 || !isfinite(compression) || compression < 0 || compression > 1)
        return false;
    double t, pl, pr, cl, cr;
    if (!cfd_hotgas3d_primitive(s, l, &t, &pl, &cl) ||
        !cfd_hotgas3d_primitive(s, r, &t, &pr, &cr))
        return false;
    double vl = l[axis + 1] / l[0], vr = r[axis + 1] / r[0], sound = fmax(cl, cr);
    double sl = fmin(vl, vr) - sound, sr = fmax(vl, vr) + sound;
    double maximum = fmax(fabs(sl), fabs(sr)), flux[5];
    if (sl >= 0 || sr <= 0) {
        const double *u = sl >= 0 ? l : r;
        double v = sl >= 0 ? vl : vr, p = sl >= 0 ? pl : pr;
        flux[0] = u[axis + 1];
        for (int a = 0; a < 3; ++a)
            flux[a + 1] = u[a + 1] * v + (a == axis ? p : 0);
        flux[4] = (u[4] + p) * v;
    } else {
        double al = l[0] * (vl - sl), ar = r[0] * (sr - vr), sum = al + ar;
        double middle = (al * vl + ar * vr + pl - pr) / sum;
        double norml = 0, normr = 0;
        for (int a = 0; a < 3; ++a) {
            norml += (l[a + 1] / l[0]) * (l[a + 1] / l[0]);
            normr += (r[a + 1] / r[0]) * (r[a + 1] / r[0]);
        }
        double chi = fmin(1, sqrt(fmax(norml, normr)) / sound), phi = chi * (2 - chi);
        /* A one-way linear acoustic jump has |dp|=rho*c*|dv|. Rotational
         * low-Mach pressure jumps scale as rho*v*dv instead. No tuned Mach
         * cutoff: compare against the physical acoustic impedance. */
        double impedance = 2 * l[0] * r[0] / (l[0] + r[0]) * sound;
        double acoustic_jump = impedance * fabs(vr - vl), pressure_jump = fabs(pr - pl);
        double guard = acoustic_jump > 0 ? fmin(1, pressure_jump / acoustic_jump) :
                       pressure_jump > 0 ? 1 : 0;
        phi = fmax(phi, compression * guard);
        double pstar = (ar * pl + al * pr + phi * al * ar * (vl - vr)) / sum;
        if (!isfinite(middle) || middle <= sl || middle >= sr || !isfinite(pstar) || pstar <= 0)
            return false;
        double stars[2][5];
        for (int side = 0; side < 2; ++side) {
            const double *u = side ? r : l;
            double wave = side ? sr : sl, v = side ? vr : vl, p = side ? pr : pl;
            double *star = stars[side];
            star[0] = u[0] * (wave - v) / (wave - middle);
            for (int a = 0; a < 3; ++a)
                star[a + 1] = star[0] * (a == axis ? middle : u[a + 1] / u[0]);
            star[4] = ((wave - v) * u[4] - p * v + pstar * middle) / (wave - middle);
            double ignored;
            if (!cfd_hotgas3d_primitive(s, star, &t, &ignored, &ignored))
                return false;
        }
        const double *star = stars[middle < 0];
        flux[0] = star[0] * middle;
        for (int a = 0; a < 3; ++a)
            flux[a + 1] = star[a + 1] * middle + (a == axis ? pstar : 0);
        flux[4] = (star[4] + pstar) * middle;
    }
    for (int i = 0; i < 5; ++i)
        if (!isfinite(flux[i]))
            return false;
    if (!isfinite(maximum))
        return false;
    memcpy(output, flux, sizeof(flux));
    *speed = maximum;
    return true;
}
