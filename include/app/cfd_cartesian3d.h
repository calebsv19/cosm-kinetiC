#ifndef PHYSICS_SIM_CFD_CARTESIAN3D_H
#define PHYSICS_SIM_CFD_CARTESIAN3D_H
#include "app/cfd_memory.h"
#include <stdbool.h>
typedef struct {
    int n[3], count;
    double length[3], h[3], volume, area[3];
} CfdCartesian3d;
/* Periodic face a at index (i,j,k) is the LOWER face of that cell.
 * One stored face serves both neighboring cells. Pressure is cell-centered. */
bool cfd_cartesian3d_init(CfdCartesian3d *g, const int n[3], const double length[3]);
int cfd_cartesian3d_neighbor(const CfdCartesian3d *g, int q, int axis, int offset);
void cfd_cartesian3d_position(const CfdCartesian3d *g, int q, int face_axis, double xyz[3]);
void cfd_cartesian3d_divergence(const CfdCartesian3d *g, const double *velocity, double *out);
void cfd_cartesian3d_gradient(const CfdCartesian3d *g, const double *pressure, double *out);
/* Cached SPD mass - viscosity*laplacian; periodic axes or zero Dirichlet
 * at half-cell outer faces. A pinned periodic Poisson operator is available.
 * Solutions are accepted only on the independently recomputed true residual. */
typedef struct CfdCartesian3dLinear CfdCartesian3dLinear;
CfdCartesian3dLinear *cfd_cartesian3d_linear_create(const CfdCartesian3d *g, const bool periodic[3],
                                                    double mass, double viscosity, bool pin);
/* Mixed pressure boundaries: nonperiodic sides default to half-cell Dirichlet;
 * selected homogeneous Neumann sides contribute no boundary diagonal. */
CfdCartesian3dLinear *cfd_cartesian3d_linear_create_mixed(const CfdCartesian3d *g,
    const bool periodic[3], const bool neumann[6], double mass, double viscosity, bool pin);
void cfd_cartesian3d_linear_destroy(CfdCartesian3dLinear *s);
bool cfd_cartesian3d_linear_solve(CfdCartesian3dLinear *s, const double *b, double *x,
                                  int *iterations, double *relative_residual);
void cfd_cartesian3d_linear_apply(const CfdCartesian3dLinear *s, const double *x, double *b);
#endif
