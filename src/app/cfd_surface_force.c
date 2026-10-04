#include "app/cfd_surface_force.h"
#include <math.h>
#include <string.h>
bool cfd_surface_flux(double rho, double mu, double pressure, double area, const double normal[3],
                      const double velocity[3], const double surface_velocity[3],
                      const double grad[3][3], CfdSurfaceFlux *out) {
    if (!normal || !velocity || !surface_velocity || !grad || !out || !isfinite(rho) || rho <= 0 ||
        !isfinite(mu) || mu < 0 || !isfinite(pressure) || !isfinite(area) || area <= 0)
        return false;
    double norm = 0, relative = 0;
    for (int i = 0; i < 3; i++) {
        if (!isfinite(normal[i]) || !isfinite(velocity[i]) || !isfinite(surface_velocity[i]))
            return false;
        norm += normal[i] * normal[i];
        relative += (velocity[i] - surface_velocity[i]) * normal[i];
        for (int j = 0; j < 3; j++)
            if (!isfinite(grad[i][j]))
                return false;
    }
    if (fabs(norm - 1) > 1e-10)
        return false;
    CfdSurfaceFlux result = {0};
    result.outward_mass_kg_s = rho * area * relative;
    for (int i = 0; i < 3; i++) {
        result.pressure_on_fluid_n[i] = -pressure * normal[i] * area;
        for (int j = 0; j < 3; j++)
            result.viscous_on_fluid_n[i] += mu * (grad[i][j] + grad[j][i]) * normal[j] * area;
        result.outward_momentum_n[i] = result.outward_mass_kg_s * velocity[i];
        if (!isfinite(result.pressure_on_fluid_n[i]) || !isfinite(result.viscous_on_fluid_n[i]) ||
            !isfinite(result.outward_momentum_n[i]))
            return false;
    }
    if (!isfinite(result.outward_mass_kg_s))
        return false;
    *out = result;
    return true;
}
