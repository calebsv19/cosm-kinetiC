#ifndef PHYSICS_SIM_CFD_HOTGAS_DOMAIN3D_H
#define PHYSICS_SIM_CFD_HOTGAS_DOMAIN3D_H
#include "app/cfd_hotgas3d.h"
typedef enum { CFD_HOTGAS_PERIODIC, CFD_HOTGAS_SLIP_WALL, CFD_HOTGAS_RESERVOIR } CfdHotGasFaceKind;
typedef struct {
    CfdHotGasFaceKind kind;
    double reservoir_pressure_origin_pa, reservoir_temperature_k, reservoir_velocity_m_s[3];
    double wall_temperature_k; /* zero: insulated; positive: prescribed thermal reservoir */
} CfdHotGasFace;
typedef struct {
    CfdHotGasFace face[6]; /* x-, x+, y-, y+, z-, z+ */
    double gravity_m_s2[3], conductivity_w_m_k;
    int contact_flux; /* 0: Rusanov; 1: HLLC; 2: MUSCL HLLC / SSPRK2; 3: MUSCL L-HLLC / SSPRK3 */
} CfdHotGasDomain;
typedef struct {
    double outward[6][5], potential_outward_j[6], conducted_outward_j[6];
    double gravity_impulse_kg_m_s[3], gravity_work_j;
} CfdHotGasDomainBudget;
bool cfd_hotgas_domain3d_validate(const CfdHotGas3d *s, const CfdHotGasDomain *domain);
/* Locally isothermal hydrostatic reconstruction; explicit gravity, conduction.
 * Conservative E+rho*potential transport. Refusal leaves state and budget intact.
 * Walls are inviscid slip walls; reservoirs prescribe far-field states, not a
 * nonreflecting radiation boundary. No species or flame radiation. */
bool cfd_hotgas_domain3d_step(CfdHotGas3d *s, const CfdHotGasDomain *domain, const double *source,
                              double dt, CfdHotGasDomainBudget *budget);
/* Conducted flux, upward positive, on axis-normal face. side=+1 for an interior
 * upper face; boundary index is -1 for interior or 0..5 for domain face. */
bool cfd_hotgas_domain3d_conduction(const CfdHotGas3d *s, const CfdHotGasDomain *domain, int q,
                                    int axis, int boundary, double *upward_flux_w_m2);
/* Horizontal plane samples: downward conducted flux and upward advected total
 * enthalpy flux are distinct quantities. Not Gardon-gauge total heat flux.
 * Arrays have nx*ny values. Lower z face must be a prescribed-temperature wall. */
bool cfd_hotgas_domain3d_sample(const CfdHotGas3d *s, const CfdHotGasDomain *domain,
                                double height_m, double *temperature_k,
                                double *conducted_downward_w_m2, double *advected_upward_w_m2);
#endif
