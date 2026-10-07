#ifndef PHYSICS_SIM_CFD_OPEN_ATMOSPHERE3D_H
#define PHYSICS_SIM_CFD_OPEN_ATMOSPHERE3D_H
#include "app/cfd_cartesian3d.h"
/* MAC projection: periodic XY with open Z reservoirs or an opt-in no-slip
 * impermeable bottom and open top. Pressure uses half-cell Dirichlet reservoirs
 * and homogeneous Neumann at the ground. No internal bodies. Time is first order;
 * limited MUSCL spatial advection is opt-in.
 * X/Y lower faces have N values each; Z lower faces plus top have N+Nx*Ny.
 * Reservoir scalar diffusion is zero normal flux; advection uses ambient on inflow.
 * Pressure is relative to base hydrostatic atmosphere. No full traction claim. */
typedef struct {
    CfdCartesian3d grid;
    int plane, velocity_count, steps;
    double rho, mu, cp, reference_k, conductivity, diffusivity, dt;
    double pressure_boundary[2], ambient_k[2], ambient_smoke[2];
    bool buoyancy, ground, muscl, cache_projection;
    CfdCartesian3dLinear *projection;
    bool projection_ground;
    unsigned long long max_scalar_work_cells;
    double gravity, beta, max_contrast;
    double time, initial_j, initial_kg, input_j, input_kg, divergence, residual;
    unsigned long long scalar_work_cells;
    double *storage, *velocity, *pressure, *energy, *smoke;
    double *candidate_velocity, *candidate_pressure, *rhs, *scalar_candidate, *scalar_work;
    /* Boundary order: bottom,top; per-face X-fast.
     * Blocks: energy inflow,outflow, smoke inflow,outflow. Four * 2*plane values. */
    double *flux, *candidate_flux;
} CfdOpenAtmosphere3d;
bool cfd_open_atmosphere3d_init(CfdOpenAtmosphere3d *s,const int n[3],const double length[3],
    double rho,double mu,double cp,double reference_k,double conductivity,double diffusivity,
    double dt,const double pressure_pa[2],const double ambient_k[2],const double ambient_smoke[2],
    bool buoyancy,double gravity,double beta,double max_contrast);
void cfd_open_atmosphere3d_destroy(CfdOpenAtmosphere3d *s);
bool cfd_open_atmosphere3d_valid(const CfdOpenAtmosphere3d *s);
/* Opt-in stationary no-slip bottom, impermeable to heat/smoke. Set before
 * loading fields; bottom normal face must be exactly zero. Top remains open. */
/* Integrated source arrays. Failure preserves accepted fields, flux receipts,
 * time, counters and diagnostics. Scratch is not checkpoint state. */
bool cfd_open_atmosphere3d_step(CfdOpenAtmosphere3d *s,const double *source_j,const double *source_kg);
#endif
