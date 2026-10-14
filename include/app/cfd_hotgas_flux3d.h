#ifndef PHYSICS_SIM_CFD_HOTGAS_FLUX3D_H
#define PHYSICS_SIM_CFD_HOTGAS_FLUX3D_H
#include "app/cfd_hotgas3d.h"
/* Toro HLLC with bounding Davis signal estimates. Acoustic CFL unchanged.
 * Invalid/nonpositive star states refuse; output is assigned only on success.
 * Contact resolution is not an all-speed or reacting-gas qualification. */
bool cfd_hotgas_hllc3d(const CfdHotGas3d *s, const double left[5], const double right[5],
                       int axis, double output[5], double *speed);
/* Pressure-corrected low-dissipation variant. Physical acoustic CFL retained;
 * no multidimensional shock qualification. Transactional output on refusal. */
bool cfd_hotgas_lhllc3d(const CfdHotGas3d *s, const double left[5], const double right[5],
                        int axis, double compression, double output[5], double *speed);
#endif
