#ifndef PHYSICS_SIM_CFD_HOTGAS_RECONSTRUCTION3D_H
#define PHYSICS_SIM_CFD_HOTGAS_RECONSTRUCTION3D_H
#include "app/cfd_hotgas_domain3d.h"
/* Internal composition helpers. Primitive MC slopes after removing the local
 * isothermal equilibrium gradient. Missing physical neighbors flatten slopes,
 * except L-HLLC slip walls reflect normal velocity for the boundary slope.
 * Invalid reconstructed states refuse; no silent first-order fallback. */
double cfd_hotgas_compression3d(const CfdHotGas3d *s, const CfdHotGasDomain *d, int q);
bool cfd_hotgas_muscl_face(const CfdHotGas3d *s, const CfdHotGasDomain *d, int q,
                           int axis, int sign, double out[5]);
bool cfd_hotgas_domain3d_euler(CfdHotGas3d *s, const CfdHotGasDomain *d,
                               const double *source, double dt, CfdHotGasDomainBudget *budget);
bool cfd_hotgas_domain3d_rk2(CfdHotGas3d *s, const CfdHotGasDomain *d,
                             const double *source, double dt, CfdHotGasDomainBudget *budget);
bool cfd_hotgas_domain3d_rk3(CfdHotGas3d *s, const CfdHotGasDomain *d,
                             const double *source, double dt, CfdHotGasDomainBudget *budget);
#endif
