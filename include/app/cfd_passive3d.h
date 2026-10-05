#ifndef PHYSICS_SIM_CFD_PASSIVE3D_H
#define PHYSICS_SIM_CFD_PASSIVE3D_H
#include "app/cfd_cartesian3d.h"
/* Periodic, body-free constant-property passive sensible energy and tracer.
 * Extensive cell quantities: J above reference temperature and kg tracer.
 * Thermal diffusivity is k/(rho*cp); tracer never changes bulk rho or momentum.
 * Live owner must not be copied. Initial fields are zero. */
typedef struct {
    CfdCartesian3d grid;
    double rho, cp, reference_k, alpha, tracer_diffusivity, time;
    double *storage, *energy, *smoke, *candidate, *work;
    double input_j, input_kg, last_balance_j, last_balance_kg;
    unsigned steps, substeps;
    unsigned long long work_cells;
} CfdPassive3d;
bool cfd_passive3d_init(CfdPassive3d *s, const int n[3], const double length[3],
    double rho, double cp, double reference_k, double conductivity, double tracer_diffusivity);
void cfd_passive3d_destroy(CfdPassive3d *s);
/* Integrated nonnegative source arrays, 3*N lower-face velocity values in m/s.
 * Inputs are borrowed read-only, must not alias owned scalar memory.
 * Fixed velocity during this scalar step; adaptive scalar subcycles only.
 * Failure preserves accepted fields, time, counters and budgets. */
bool cfd_passive3d_step(CfdPassive3d *s, const double *velocity,
    const double *source_j, const double *source_kg, double dt);
double cfd_passive3d_total(const double *values, int count);
#endif
