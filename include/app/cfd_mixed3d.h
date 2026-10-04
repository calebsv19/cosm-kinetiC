#ifndef PHYSICS_SIM_CFD_MIXED3D_H
#define PHYSICS_SIM_CFD_MIXED3D_H
#include "app/cfd_cartesian3d.h"
typedef struct CfdMixed3d CfdMixed3d;
typedef bool (*CfdMixed3dCheckpoint)(void *context);
/* Stationary no-slip Y/Z. X is periodic or natural at BOTH ends. Integrated
 * MAC B/-B^T and rho*alpha/dt mass + vector-Laplacian viscosity, pressure Pa.
 * Owned candidate engine; output arrays are never a published state. */
CfdMixed3d *cfd_mixed3d_create(const CfdCartesian3d *grid, double mu, double mass, bool periodic_x);
void cfd_mixed3d_destroy(CfdMixed3d *s);
bool cfd_mixed3d_set_mass(CfdMixed3d *s, double mass);
int cfd_mixed3d_count(const CfdMixed3d *s);
int cfd_mixed3d_index(const CfdMixed3d *s, int axis, int i, int j, int k);
void cfd_mixed3d_position(const CfdMixed3d *s, int q, int *axis, double xyz[3], int ijk[3]);
double cfd_mixed3d_volume(const CfdMixed3d *s, int q);
void cfd_mixed3d_divergence(const CfdMixed3d *s, const double *u, double *out);
bool cfd_mixed3d_solve(CfdMixed3d *s, const double *rhs, double *u, double *pressure);
void cfd_mixed3d_checkpoint(CfdMixed3d *s, CfdMixed3dCheckpoint function, void *context);
int cfd_mixed3d_iterations(const CfdMixed3d *s);
int cfd_mixed3d_inner_iterations(const CfdMixed3d *s);
double cfd_mixed3d_residual(const CfdMixed3d *s);
const char *cfd_mixed3d_error(const CfdMixed3d *s);
#ifdef CFD_MIXED3D_VERIFY
bool cfd_mixed3d_verify(CfdMixed3d *s);
#endif
#endif
