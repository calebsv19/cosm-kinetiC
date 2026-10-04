#ifndef PHYSICS_SIM_CFD_CHANNEL_H
#define PHYSICS_SIM_CFD_CHANNEL_H
#include <stdbool.h>
#define CFD_CHANNEL_MAX_CELLS 256
/* Fully developed laminar u(y,t), uniform in x/z. G = -dp/dx in Pa/m.
 * Streamwise face velocity is shared across both faces of each control volume.
 * This reduction satisfies continuity identically, without a pressure solve. */
typedef struct CfdChannel {
    int n;
    double length, height, width, rho, mu, gradient, wall_bottom, wall_top;
    double time, u[CFD_CHANNEL_MAX_CELLS], tau[CFD_CHANNEL_MAX_CELLS+1];
    double volume_flux, max_acceleration, momentum_residual, local_residual;
    double kinetic_energy, energy_residual, pressure_power, wall_power, dissipation;
    double temporal_dissipation, recovered_pressure_drop;
} CfdChannel;
bool cfd_channel_init(CfdChannel *c, int cells, const double dimensions[3],
    double rho, double mu, double gradient, double bottom, double top);
bool cfd_channel_step(CfdChannel *c, double dt);
double cfd_channel_velocity(const CfdChannel *c, double y);
double cfd_channel_shear(const CfdChannel *c, double y);
#endif
