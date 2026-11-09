#ifndef PHYSICS_CHEMISTRY_FLOW_H
#define PHYSICS_CHEMISTRY_FLOW_H
#include "transport.h"
/* Periodic unsteady Stokes; lower faces component-major [3*cells].
 * rho=rho0*(1+sum beta_i*(c_i-reference_i)); only buoyancy uses contrast.
 * Domain mean force removed (periodic accelerating-frame convention).
 * Limits |contrast|<=.01 and maxspeed*maxlength/nu<=.1, <=4096cells.
 * No momentum advection, viscosity feedback, compressibility or energy feedback. */
typedef struct { double rho,nu,gravity[3],beta[CHEM_S],reference[CHEM_S]; int feedback; } ChemFlowModel;
typedef struct { double divergence,reynolds,contrast,kinetic,work,dissipation,time_defect,energy_residual,momentum_residual; } ChemFlowMetrics;
bool chem_flow_step(const ChemMechanism *,const ChemGrid *,const ChemFlowModel *,ChemState *,double *velocity,double *pressure,double dt,bool reacting,ChemFlowMetrics *);
/* Additive external face acceleration, m/s2. NULL preserves original law.
 * Caller owns any extra constitutive/admission contract. */
bool chem_flow_step_extra(const ChemMechanism *,const ChemGrid *,const ChemFlowModel *,ChemState *,double *,double *,double,bool,ChemFlowMetrics *,const double *);
#endif
