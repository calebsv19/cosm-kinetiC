#include "physics/chemistry/water.h"

#include <math.h>

static const double minimum_temperature = 283.15;
static const double maximum_temperature = 323.15;
static const double reference_temperature = 298.15;

bool chem_water_properties(double temperature_k, ChemWaterProperties *out)
{
    if (!out || !isfinite(temperature_k) || temperature_k < minimum_temperature ||
        temperature_k > maximum_temperature)
        return false;

    /* IAPWS SR1-86(1992), equations 1–4, 6 and 7. SI throughout. */
    const double critical_temperature = 647.096;
    const double theta = temperature_k / critical_temperature;
    const double tau = 1 - theta;
    const double a[] = {-7.85951783, 1.84408259, -11.7866497,
                        22.6807411, -15.9618719, 1.80122502};
    const double powers[] = {1, 1.5, 3, 3.5, 4, 7.5};
    double pressure_series = 0;
    double pressure_derivative = 0;
    for (int i = 0; i < 6; ++i) {
        pressure_series += a[i] * pow(tau, powers[i]);
        pressure_derivative += a[i] * powers[i] * pow(tau, powers[i] - 1);
    }
    const double pressure = 22.064e6 * exp(pressure_series / theta);
    const double dp_dt = -pressure * (pressure_series / theta + pressure_derivative) /
                         temperature_k;
    const double liquid_density = 322 * (1 + 1.99274064 * pow(tau, 1./3) +
        1.09965342 * pow(tau, 2./3) - .510839303 * pow(tau, 5./3) -
        1.75493479 * pow(tau, 16./3) - 45.5170352 * pow(tau, 43./3) -
        674694.450 * pow(tau, 110./3));
    const double vapor_density = 322 * exp(-2.03150240 * pow(tau, 2./6) -
        2.68302940 * pow(tau, 4./6) - 5.38626492 * pow(tau, 8./6) -
        17.2991605 * pow(tau, 18./6) - 44.7586581 * pow(tau, 37./6) -
        63.9201063 * pow(tau, 71./6));
    const double alpha = 1000 * (-1135.905627715 - 5.65134998e-8 * pow(theta, -19) +
        2690.66631 * theta + 127.287297 * pow(theta, 4.5) -
        135.003439 * pow(theta, 5) + .981825814 * pow(theta, 54.5));
    const double energy_term = temperature_k * dp_dt - pressure;
    *out = (ChemWaterProperties){pressure, liquid_density, vapor_density,
        alpha + energy_term / liquid_density, alpha + energy_term / vapor_density};
    return true;
}

static bool model_valid(const ChemWaterModel *model)
{
    return model && isfinite(model->volume_m3) && model->volume_m3 >= 1e-9 &&
        model->volume_m3 <= 1e3 && isfinite(model->water_kg) && model->water_kg >= 0 &&
        model->water_kg <= 240 * model->volume_m3 &&
        isfinite(model->thermal_capacity_j_k) && model->thermal_capacity_j_k >= 0 &&
        model->thermal_capacity_j_k <= 1e9 &&
        (model->water_kg > 0 || model->thermal_capacity_j_k > 0) &&
        isfinite(model->relaxation_s) && model->relaxation_s >= 1e-6 &&
        model->relaxation_s <= 1e9;
}

static double energy(const ChemWaterModel *model, const ChemWaterProperties *properties,
                     double temperature_k, double vapor_kg)
{
    return (model->water_kg - vapor_kg) * properties->liquid_u +
           vapor_kg * properties->vapor_u +
           model->thermal_capacity_j_k * (temperature_k - reference_temperature);
}

static double equilibrium_vapor(const ChemWaterModel *model,
                                const ChemWaterProperties *properties)
{
    /* Solve m_v = rho_v (V - (M - m_v)/rho_l), limited by finite water. */
    return fmin(model->water_kg,
                properties->vapor_density *
                (model->volume_m3 - model->water_kg / properties->liquid_density) /
                (1 - properties->vapor_density / properties->liquid_density));
}

bool chem_water_initialize(const ChemWaterModel *model, double temperature_k,
                           double vapor_kg, ChemWaterState *out)
{
    ChemWaterProperties properties;
    if (!out || !model_valid(model) || !isfinite(vapor_kg) || vapor_kg < 0 ||
        vapor_kg > model->water_kg || !chem_water_properties(temperature_k, &properties))
        return false;
    const double headspace = model->volume_m3 -
        (model->water_kg - vapor_kg) / properties.liquid_density;
    if (vapor_kg / headspace > 2 * properties.vapor_density)
        return false;
    *out = (ChemWaterState){temperature_k, vapor_kg,
                           energy(model, &properties, temperature_k, vapor_kg)};
    return true;
}

static double energy_at(const ChemWaterModel *model, double temperature,
                        double vapor, bool equilibrium)
{
    ChemWaterProperties properties;
    if (!chem_water_properties(temperature, &properties))
        return NAN;
    if (equilibrium)
        vapor = equilibrium_vapor(model, &properties);
    return energy(model, &properties, temperature, vapor);
}

static bool temperature_for_energy(const ChemWaterModel *model, double target_energy,
                                   double vapor, bool equilibrium, double *out)
{
    double low = minimum_temperature;
    double high = maximum_temperature;
    if (target_energy < energy_at(model, low, vapor, equilibrium) ||
        target_energy > energy_at(model, high, vapor, equilibrium))
        return false;
    for (int i = 0; i < 55; ++i) {
        const double midpoint = low + (high - low) / 2;
        if (energy_at(model, midpoint, vapor, equilibrium) < target_energy)
            low = midpoint;
        else
            high = midpoint;
    }
    *out = low + (high - low) / 2;
    return true;
}

bool chem_water_step(const ChemWaterModel *model, const ChemWaterState *state,
                     double dt_s, ChemWaterState *out)
{
    ChemWaterState checked;
    if (!state || !out || !isfinite(dt_s) || dt_s < 0 || dt_s > 1e9 ||
        !chem_water_initialize(model, state->temperature_k, state->vapor_kg, &checked) ||
        !isfinite(state->energy_j) ||
        fabs(checked.energy_j - state->energy_j) > 1e-10 * fmax(1, fabs(state->energy_j)))
        return false;
    if (dt_s == 0) {
        *out = *state;
        return true;
    }

    double equilibrium_temperature = state->temperature_k;
    if (model->insulated &&
        !temperature_for_energy(model, state->energy_j, 0, true, &equilibrium_temperature))
        return false;

    ChemWaterProperties equilibrium_properties;
    if (!chem_water_properties(equilibrium_temperature, &equilibrium_properties))
        return false;
    const double target_vapor = equilibrium_vapor(model, &equilibrium_properties);
    const double fraction = -expm1(-dt_s / model->relaxation_s);
    const double vapor = fraction == 1 ? target_vapor :
        state->vapor_kg + fraction * (target_vapor - state->vapor_kg);
    double temperature = state->temperature_k;
    if (model->insulated &&
        !temperature_for_energy(model, state->energy_j, vapor, false, &temperature))
        return false;

    ChemWaterState candidate;
    if (!chem_water_initialize(model, temperature, vapor, &candidate))
        return false;
    if (model->insulated) {
        if (fabs(candidate.energy_j - state->energy_j) >
            1e-10 * fmax(1, fabs(state->energy_j)))
            return false;
        candidate.energy_j = state->energy_j;
    }
    *out = candidate;
    return true;
}
