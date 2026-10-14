#ifndef PHYSICS_SIM_CFD_MIXTURE_THERMO_H
#define PHYSICS_SIM_CFD_MIXTURE_THERMO_H
#include "app/cfd_hotgas_caloric.h"
#define CFD_MIXTURE_SPECIES 5
/* Order N2,O2,CO2,H2O,CH3OH gas. Formation h at 298.15 K included.
 * Ideal gas 200..1500 K; no kinetics/phase equilibrium. */
typedef struct { CfdHotGasCaloric pure[5]; double formation[5]; } CfdMixtureThermo;
extern const double cfd_mixture_mass[5];
bool cfd_mixture_thermo_init(CfdMixtureThermo *out);
bool cfd_mixture_model(const CfdMixtureThermo *s, const double y[5],
    CfdHotGasCaloric *caloric, double *formation);
bool cfd_mixture_properties(const CfdMixtureThermo *s, const double y[5], double t,
    double *energy, double *cp, double *gas_constant);
/* u: rho, momentum xyz, total energy incl formation, five partial densities. */
bool cfd_mixture_primitive(const CfdMixtureThermo *s, const double u[10],
    double *temperature, double *pressure, double *sound, double *cv);
bool cfd_mixture_enthalpies(const CfdMixtureThermo *s, double t, double h[5]);
#endif
