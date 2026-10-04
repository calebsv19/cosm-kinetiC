#ifndef PHYSICS_SIM_CFD_MOMENTUM_FLUX_H
#define PHYSICS_SIM_CFD_MOMENTUM_FLUX_H
#include <math.h>
/* Monotonized-central slope; reconstructed interface states stay between the
 * adjacent cell values. This is a spatial reconstruction, not a claim that
 * the full projected Navier-Stokes update obeys a scalar maximum principle. */
static inline double cfd_mc_slope(double left, double right) {
    if ((left > 0 && right > 0) || (left < 0 && right < 0))
        return copysign(fmin(.5 * fabs(left + right), 2 * fmin(fabs(left), fabs(right))), left);
    return 0;
}
static inline double cfd_limited_momentum_flux(double qm, double q0, double q1, double qp,
                                               double speed) {
    double left = q0 + .5 * cfd_mc_slope(q0 - qm, q1 - q0);
    double right = q1 - .5 * cfd_mc_slope(q1 - q0, qp - q1);
    return speed * (speed >= 0 ? left : right);
}
#endif
