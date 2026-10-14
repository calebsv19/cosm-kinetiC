#ifndef PHYSICS_SIM_HOTGAS_CALORIC_H
#define PHYSICS_SIM_HOTGAS_CALORIC_H
#include <stdbool.h>
/* Frozen ideal mixture N2/O2/CO2/H2O. Published Cp bands, 200..1500 K.
 * Continuous sensible h(T), h(298.15 K)=0, e=h-R*T. No formation energy,
 * reactions, condensation or species diffusion. Composition is immutable. */
typedef struct {
    bool active;
    double gas_constant, coefficients[3][6]; /* T^0..T^4, T^-2, mass basis */
} CfdHotGasCaloric;
bool cfd_hotgas_caloric_init(CfdHotGasCaloric *out, const double moles[4]);
bool cfd_hotgas_caloric_properties(const CfdHotGasCaloric *s, double temperature,
                                    double *energy, double *cp);
bool cfd_hotgas_caloric_temperature(const CfdHotGasCaloric *s, double energy, double *temperature);
#endif
