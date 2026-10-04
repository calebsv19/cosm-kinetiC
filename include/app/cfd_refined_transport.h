#ifndef PHYSICS_SIM_CFD_REFINED_TRANSPORT_H
#define PHYSICS_SIM_CFD_REFINED_TRANSPORT_H
#include "app/cfd_refined_mesh.h"
typedef struct CfdRefinedTransport CfdRefinedTransport;
/* Cached least-squares geometry and workspace; no per-evaluation allocation. */
CfdRefinedTransport *cfd_refined_transport_create(const CfdRefinedMesh *mesh);
void cfd_refined_transport_destroy(CfdRefinedTransport *t);
/* Conservative limited linear upwind transport of a cell-average component.
 * rhs = -div(velocity*q); boundary values and velocities are at face centers.
 * max_rate is the largest outgoing volume rate / cell volume (s^-1).
 * outward_flux is the integrated domain-boundary transported quantity rate.
 * This component operator does not itself implement incompressible momentum. */
bool cfd_refined_transport_rhs(CfdRefinedTransport *t, const double *q, const double *velocity,
                               const double *boundary, double *rhs, double *max_rate,
                               double *outward_flux);
#endif
