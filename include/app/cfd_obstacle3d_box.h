#ifndef PHYSICS_SIM_CFD_OBSTACLE3D_BOX_H
#define PHYSICS_SIM_CFD_OBSTACLE3D_BOX_H
#include "app/cfd_obstacle3d.h"
/* Optional stationary-box initialization for the existing steady Stokes solver.
 * Tunnel Y/Z dimensions stay 2 m to preserve the current end-load observation.
 * Body lower/upper planes are physical metres and must coincide with grid planes;
 * no snapping. Require >=2 body cells and >=2 fluid cells to each outer plane.
 * Bulk Reynolds rho*Q*max(body extent)/(4*mu) must be <=0.1. This admission bound
 * is not an absolute force-accuracy claim or an inertial/wake capability.
 * Initialize a fresh/destroyed state; failure leaves it safe to destroy. */
bool cfd_obstacle3d_box_init(CfdObstacle3d *s, const int n[3], const double length[3],
                             double rho, double mu, double flow,
                             const double lower_m[3], const double upper_m[3]);
#endif
