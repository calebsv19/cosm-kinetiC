#ifndef PHYSICS_CHEMISTRY_THERMAL_H
#define PHYSICS_CHEMISTRY_THERMAL_H
#include "flow.h"
/* Synthetic incompressible single-liquid caloric model; SI.
 * E = V*carrier_cv*(T-Tref) + sum n_i*(formation_i+cv_i*(T-Tref)).
 * Solute partial energy/enthalpy coincide by zero-excess-volume model assumption.
 * No independent reaction-heat source; only irreversible elementary kinetics. */
typedef struct { double reference_t,min_t,max_t,carrier_cv;
 double formation[CHEM_S],cv[CHEM_S],activation[CHEM_R];
 double conductivity,bath_t,bath_conductance,power,expansion;
} ChemThermalModel;
bool chem_thermal_valid(const ChemMechanism *,const ChemThermalModel *);
bool chem_thermal_energy(const ChemMechanism *,const ChemThermalModel *,const ChemState *,double volume,double temperature,double *energy);
bool chem_thermal_temperature(const ChemMechanism *,const ChemThermalModel *,const ChemState *,double volume,double energy,double *temperature);
bool chem_thermal_react(const ChemMechanism *,const ChemThermalModel *,ChemState *,double *energy,double volume,double dt,double *external_j);
bool chem_thermal_transport(const ChemMechanism *,const ChemGrid *,const ChemThermalModel *,const ChemState *old_cells,const double *old_energy,const double *velocity,double dt,const ChemState *expected,double *energy);
bool chem_thermal_flow_step(const ChemMechanism *,const ChemGrid *,const ChemFlowModel *,const ChemThermalModel *,ChemState *,double *velocity,double *pressure,double *energy,double dt,bool reacting,ChemFlowMetrics *,double *external_j,double *viscous_j);
#endif
