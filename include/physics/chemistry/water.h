#ifndef PHYSICS_CHEMISTRY_WATER_H
#define PHYSICS_CHEMISTRY_WATER_H
#include <stdbool.h>

typedef struct {
    double pressure_pa, liquid_density, vapor_density, liquid_u, vapor_u;
} ChemWaterProperties;

typedef struct {
    double volume_m3, water_kg, thermal_capacity_j_k, relaxation_s;
    bool insulated;
} ChemWaterModel;

typedef struct {
    double temperature_k, vapor_kg, energy_j;
} ChemWaterState;

bool chem_water_properties(double temperature_k, ChemWaterProperties *out);
bool chem_water_initialize(const ChemWaterModel *model, double temperature_k,
                           double vapor_kg, ChemWaterState *out);
bool chem_water_step(const ChemWaterModel *model, const ChemWaterState *state,
                     double dt_s, ChemWaterState *out);
#endif
