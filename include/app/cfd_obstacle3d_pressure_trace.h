#ifndef PHYSICS_SIM_CFD_OBSTACLE3D_PRESSURE_TRACE_H
#define PHYSICS_SIM_CFD_OBSTACLE3D_PRESSURE_TRACE_H
#include "app/cfd_obstacle3d.h"
#include <stddef.h>

typedef struct {
    double force_n[3], side_force_n[6][3];
    size_t interval_depth_patches[3]; /* Counts for two, three and four intervals. */
} CfdObstacle3dPressureTrace;

/* Optional body-pressure diagnostic from actual fluid-cell pressure means.
 * Four intervals reproduce cubic traces; three/two retain the existing fallback.
 * Leaves the solver, its published forces and output unchanged on failure.
 * The caller supplies a matching, initialized stationary-box geometry and its
 * complete pressure array. This diagnostic is not physical certification. */
bool cfd_obstacle3d_pressure_trace_diagnostic(const CfdObstacle3d *, const double *, size_t,
                                             CfdObstacle3dPressureTrace *);
#endif
