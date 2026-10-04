#ifndef PHYSICS_SIM_CFD_OPEN2D_FORCE_CHECK_H
#define PHYSICS_SIM_CFD_OPEN2D_FORCE_CHECK_H
#include "app/cfd_mac2d_force_check.h"
#include "app/cfd_open2d.h"
/* Physical quadrature on an interior rectangle enclosing all solids. Reuses
 * the existing estimator via a read-only layout view; no periodic evolution.
 * Surface traction uses quadratic pressure and cubic no-slip shear reconstruction.
 * Requires the third fluid sample at every body face; no silent order fallback. */
bool cfd_open2d_force_check(const CfdOpen2D *c, int x0, int y0, int x1, int y1,
                            CfdMac2DForceCheck *out);
#endif
