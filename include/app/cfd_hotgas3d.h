#ifndef PHYSICS_SIM_CFD_HOTGAS3D_H
#define PHYSICS_SIM_CFD_HOTGAS3D_H
#include "app/cfd_cartesian3d.h"
#include "app/cfd_hotgas_caloric.h"
/* Conservative calorically-perfect ideal-gas Euler foundation. Periodic XYZ.
 * Cell-major conserved densities: rho, rho*u, rho*v, rho*w, total J/m3.
 * No gravity, viscosity, radiation, species or combustion. Not a pool-fire
 * solver. */
typedef struct {
    CfdCartesian3d grid;
    double gamma, gas_constant_j_kg_k;
    CfdHotGasCaloric caloric;
    double *state, *candidate;
} CfdHotGas3d;
bool cfd_hotgas3d_energy(const CfdHotGas3d *s, double temperature, double *energy, double *cv);
bool cfd_hotgas3d_init(CfdHotGas3d *s, const int n[3], const double length[3], double gamma,
                       double gas_constant);
void cfd_hotgas3d_destroy(CfdHotGas3d *s);
bool cfd_hotgas3d_primitive(const CfdHotGas3d *s, const double u[5], double *temperature,
                            double *pressure, double *sound_speed);
/* Source is an integrated conserved-density increment per step. Candidate
 * source is included in CFL admission; failure leaves accepted state unchanged.
 */
bool cfd_hotgas3d_step(CfdHotGas3d *s, const double *source, double dt);
#endif
