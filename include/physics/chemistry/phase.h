#ifndef PHYSICS_CHEMISTRY_PHASE_H
#define PHYSICS_CHEMISTRY_PHASE_H
#include "physics/chemistry/transport.h"
/* SI: K, mol/m3/Pa, J/mol, m3, m3, m2, mol/m2/s/Pa. */
typedef struct {
    double temperature,henry,delta_u,gas_volume,liquid_volume,area,permeance;
} ChemPhaseModel;
bool chem_phase_valid(const ChemPhaseModel *model);
bool chem_phase_admit(const ChemPhaseModel *model,const ChemState *state,double energy);
/* State amounts [gas,aqueous]; extents [dissolution,degassing]. Q enters material.
 * Output arguments publish only on success. No independent reaction heat source. */
bool chem_phase_step(const ChemPhaseModel *model,ChemState *state,double *energy,
    double dt,double *heat);
bool chem_phase_spatial_step(const ChemPhaseModel *model,const ChemGrid *gas,
    const ChemGrid *liquid,ChemState *cells,double *energy,double dt,double *heat);
#endif
