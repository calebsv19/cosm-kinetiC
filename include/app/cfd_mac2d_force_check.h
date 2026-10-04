#ifndef PHYSICS_SIM_CFD_MAC2D_FORCE_CHECK_H
#define PHYSICS_SIM_CFD_MAC2D_FORCE_CHECK_H
#include "app/cfd_mac2d.h"
typedef struct CfdMac2DForceCheck {
    double surface_pressure_x_n, surface_viscous_x_n;
    double reconstructed_normal_viscous_x_n;
    double quadratic_pressure_x_n, cubic_viscous_x_n;
    bool higher_order_available;
    double cv_pressure_x_n, cv_viscous_x_n, cv_advective_x_n;
    double cv_drive_x_n, cv_momentum_x_kg_m_s;
} CfdMac2DForceCheck;
/* Independent physical-space quadrature. Rectangle bounds are cell boundaries,
 * must enclose all solids with two fluid-cell padding and avoid periodic seam. */
bool cfd_mac2d_force_check(const CfdMac2D *c, int x0, int y0, int x1, int y1,
                           CfdMac2DForceCheck *out);
/* Read-only open/periodic face-layout adapter; no field allocation or copy. */
bool cfd_mac2d_force_check_strided(const CfdMac2D *c, int u_stride, int x0, int y0, int x1, int y1,
                                   CfdMac2DForceCheck *out);
#endif
