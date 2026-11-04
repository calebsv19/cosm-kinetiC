#ifndef PHYSICS_SIM_CFD_OPEN_ATMOSPHERE3D_H
#define PHYSICS_SIM_CFD_OPEN_ATMOSPHERE3D_H
#include "app/cfd_cartesian3d.h"
#include "app/cfd_sparse_mg.h"
#include "app/cfd_open_atmosphere3d_wind.h"
#include "app/cfd_open_atmosphere3d_weather.h"
/* MAC projection: periodic XY with open Z reservoirs or an opt-in no-slip
 * impermeable bottom and open top. Pressure uses half-cell Dirichlet reservoirs
 * and homogeneous Neumann at the ground and an optional aligned stationary box. Momentum time integration is first order;
 * limited MUSCL advection and scalar SSPRK2 time integration are opt-in.
 * X/Y lower faces have N values each; Z lower faces plus top have N+Nx*Ny.
 * Reservoir scalar diffusion is zero normal flux; advection uses ambient on inflow.
 * Pressure is relative to base hydrostatic atmosphere. No full traction claim. */
typedef struct CfdOpenAtmosphere3d {
    CfdCartesian3d grid;
    int plane, velocity_count, steps;
    double rho, mu, cp, reference_k, conductivity, diffusivity, dt;
    double pressure_boundary[2], ambient_k[2], ambient_smoke[2];
    bool buoyancy, ground, muscl, cache_projection;
    /* Experimental scalar-only monotonized-central reconstruction. Independent
     * of momentum MUSCL selection; zero/default preserves the existing scalar operator. Momentum
     * reconstruction is unchanged. Qualify each coupled use independently. */
    bool scalar_mc;
    /* Opt-in SSPRK2 for scalar transport plus constant-in-substep sources.
     * Velocity remains frozen at the projected flow step; this does not make
     * momentum or time-varying coupled forcing second order. Default is Euler. */
    bool scalar_rk2;
    CfdCartesian3dLinear *projection;
    bool projection_ground;
    bool solid_box, lateral_open;
    unsigned char *rig_mask; /* Explicit fixed partition/window profile; NULL keeps legacy topology. */
    int boundary_faces;
    int solid_lo[3], solid_hi[3];
    double *solid_work;
    CfdSparseMg *solid_projection;
    int *solid_row, *solid_col;
    double *solid_values;
    const char *step_rejection;
    /* Last attempted scalar work, diagnostic only; never accepted checkpoint state. */
    unsigned long long scalar_work_requested;
    unsigned max_scalar_cycles, scalar_cycles_requested;
    double momentum_courant;
    double attempted_projection_residual, attempted_projection_divergence;
    unsigned long long max_scalar_work_cells;
    double gravity, beta, max_contrast;
    double time, initial_j, initial_kg, input_j, input_kg, divergence, residual;
    unsigned long long scalar_work_cells;
    double *storage, *velocity, *pressure, *energy, *smoke;
    double *candidate_velocity, *candidate_pressure, *rhs, *scalar_candidate, *scalar_work;
    /* Boundary order: bottom,top; per-face X-fast.
     * Blocks: energy inflow,outflow, smoke inflow,outflow. Four * 2*plane values. */
    double *flux, *candidate_flux;
    CfdOpenAtmosphereWind wind;
    double wind_air_in, wind_air_out, wind_inlet_volume;
    CfdOpenAtmosphereWeather weather;
    CfdOpenWeatherState weather_state;
    double candidate_weather_work_j;
    /* Accepted cumulative discrete kinetic ledger; only active with explicit
     * quiescent-backflow policy. Scratch ledger commits with the whole step. */
    double kinetic_ledger[10], candidate_kinetic_ledger[10];
} CfdOpenAtmosphere3d;
/* Opt-in fixed unit wind rig; default/legacy box geometry is unchanged. */
bool cfd_open_atmosphere3d_rig(CfdOpenAtmosphere3d *s,bool window_open);
bool cfd_open_atmosphere3d_rig_blocked(const CfdOpenAtmosphere3d *s,int a,int i,int j,int k);
bool cfd_open_atmosphere3d_rig_drive(const CfdOpenAtmosphere3d *s,int i,int j,int k);
bool cfd_open_atmosphere3d_rig_valid(const CfdOpenAtmosphere3d *s);
bool cfd_open_atmosphere3d_init(CfdOpenAtmosphere3d *s,const int n[3],const double length[3],
    double rho,double mu,double cp,double reference_k,double conductivity,double diffusivity,
    double dt,const double pressure_pa[2],const double ambient_k[2],const double ambient_smoke[2],
    bool buoyancy,double gravity,double beta,double max_contrast);
void cfd_open_atmosphere3d_destroy(CfdOpenAtmosphere3d *s);
bool cfd_open_atmosphere3d_valid(const CfdOpenAtmosphere3d *s);
/* Configure before loading state: one interior grid-aligned, insulated no-slip box. */
/* Opt-in ambient lateral pressure reservoirs; zero-gradient predictor and
 * zero diffusive scalar flux. Append X/Y upper normal faces after Z top.
 * Flux order remains Z min/max, then X min/max, Y min/max. */
bool cfd_open_atmosphere3d_lateral(CfdOpenAtmosphere3d *s);
int cfd_open_atmosphere3d_face(const CfdOpenAtmosphere3d *s, int axis, int i, int j, int k);
bool cfd_open_atmosphere3d_box(CfdOpenAtmosphere3d *s, const int lo[3], const int hi[3]);
bool cfd_open_atmosphere3d_solid(const CfdOpenAtmosphere3d *s, int q);
bool cfd_open_atmosphere3d_blocked(const CfdOpenAtmosphere3d *s, int axis, int i, int j, int k);
bool cfd_open_atmosphere3d_box_pressure(CfdOpenAtmosphere3d *s, double *residual);
CfdSparseMg *cfd_open_atmosphere3d_box_preconditioner(CfdOpenAtmosphere3d *s);
/* Opt-in stationary no-slip bottom, impermeable to heat/smoke. Set before
 * loading fields; bottom normal face must be exactly zero. Top remains open. */
/* Integrated source arrays. Failure preserves accepted fields, flux receipts,
 * time, counters and diagnostics. Scratch is not checkpoint state. */
bool cfd_open_atmosphere3d_step(CfdOpenAtmosphere3d *s,const double *source_j,const double *source_kg);
/* Opt-in interior aerodynamic impulse. SI N*s, COMPONENT-MAJOR native face
 * order (use cfd_open_atmosphere3d_face); not interleaved exchange order.
 * First envelope: periodic XY, ground/open top, no obstacles/wind/weather.
 * Nonzero boundary-layer faces refuse. Receipt changes only on full success. */
typedef struct CfdOpenMomentumReceipt {
    double impulse_ns[3], predictor_momentum_ns[3], forced_momentum_ns[3];
    double projected_momentum_ns[3], work_j, quadratic_j;
    double forced_kinetic_j, projection_work_j, forced_courant;
} CfdOpenMomentumReceipt;
bool cfd_open_atmosphere3d_step_momentum(CfdOpenAtmosphere3d *s,
    const double *source_j, const double *source_kg,
    const double *face_impulse_ns, size_t count, CfdOpenMomentumReceipt *receipt);
void cfd_open_atmosphere3d_box_apply(const CfdOpenAtmosphere3d *s,const double *x,double *out);
/* Separate bounded stationary-screen proof: 8^3/16^3 unit cube, periodic XY,
 * two open Z pressure reservoirs, cold rho=1.2/mu=1.8e-5/dt=.005, <=20steps,
 * |bottom pressure|<=.4Pa/top=0; uniform normal through-flow |q|<=2m/s.
 * Only spanwise-uniform interior Z resistance impulses are admitted; no
 * lateral/solid/rig/wind/weather/scalar source. Legacy inlet is unchanged. */
bool cfd_open_atmosphere3d_step_screen_momentum(CfdOpenAtmosphere3d *s,
    const double *source_j,const double *source_kg,
    const double *face_impulse_ns,size_t count,CfdOpenMomentumReceipt *receipt);
#endif
