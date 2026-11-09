#ifndef PHYSICS_CHEMISTRY_TRANSPORT_H
#define PHYSICS_CHEMISTRY_TRANSPORT_H
#include "reactor.h"
/* Uniform fully occupied cells; inactive axes n=1. Boundary: 0 periodic,
 * 1 sealed (velocity must be zero on that axis). SI lengths, velocity, D. */
typedef struct { int n[3],boundary[3]; double spacing[3],velocity[3],diffusion; } ChemGrid;
/* In-place publication only after both reaction halves and transport succeed.
 * States hold mol/cell; extents are local cumulative reaction history, not tracer.
 * Capacity4096 cells. Caller supplies exactly product(n) cells. */
bool chem_grid_valid(const ChemGrid *g);
bool chem_spatial_step(const ChemMechanism *m,const ChemGrid *g,ChemState *cells,double dt,bool reacting);
#endif
