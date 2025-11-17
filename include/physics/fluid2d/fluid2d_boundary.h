#ifndef FLUID2D_BOUNDARY_H
#define FLUID2D_BOUNDARY_H

#include "app/app_config.h"
#include "app/scene_presets.h"
#include "physics/fluid2d/fluid2d.h"

// Applies preset-configured boundary flows (emitters/receivers that span a
// full edge of the grid) to the provided fluid grid. Call this before the
// solver step each frame so velocity/density fields incorporate the edge
// injections/removals.
void fluid2d_boundary_apply(const BoundaryFlow flows[BOUNDARY_EDGE_COUNT],
                            Fluid2D *grid,
                            double dt);

void fluid2d_boundary_enforce(const BoundaryFlow flows[BOUNDARY_EDGE_COUNT],
                              Fluid2D *grid);

// Wind tunnel helpers: treat emit/receive edges as true inlet/outlet
// conditions rather than additive emitters. The preset's boundary flows
// still describe which edge is inlet vs outlet, but the data is derived
// from AppConfig tunnel parameters.
void fluid2d_boundary_apply_wind(const AppConfig *cfg,
                                 const FluidScenePreset *preset,
                                 Fluid2D *grid,
                                 double dt,
                                 float ramp);

void fluid2d_boundary_enforce_wind(const AppConfig *cfg,
                                   const FluidScenePreset *preset,
                                   Fluid2D *grid);

#endif // FLUID2D_BOUNDARY_H
