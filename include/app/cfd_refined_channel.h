#ifndef PHYSICS_SIM_CFD_REFINED_CHANNEL_H
#define PHYSICS_SIM_CFD_REFINED_CHANNEL_H
#include "app/cfd_refined_diffusion.h"
#include "app/cfd_refined_mixed.h"
#include "app/cfd_refined_transport.h"
/* Verification-stage 2D channel: prescribed parabolic inlet, no-slip walls,
 * zero external vector-Laplacian traction at the outlet. Fixed mesh, constant dt,
 * BDF2/explicit extrapolated transport, implicit viscosity and compatible mixed
 * momentum/continuity. The rejected split prototype is retained only behind
 * CFD_REFINED_VERIFY_SPLIT for verification comparisons. No final face pressure trace is provided.
 * No obstacle/force or agent qualification is implied by this prototype. */
typedef struct {
    const CfdRefinedMesh *mesh;
    CfdRefinedDiffusion *velocity_operator, *pressure_operator;
    CfdRefinedTransport *transport;
    CfdRefinedMixed *mixed_operator;
    int mixed_iterations;
    size_t mixed_storage_bytes;
    double rho, nu, mean, time, dt;
    int tick, pressure_iterations, velocity_iterations;
    int correctors, max_correctors;
    double coupling_residual, coupling_tolerance, mixed_relative_residual;
    bool failed; /* A numerical failure after prediction makes this state terminal. */
    double divergence, transport_cfl;
    double transport_cpu_ms, mixed_solve_cpu_ms;
    double *storage, *u, *v, *old_u, *old_v, *adv_u, *adv_v, *old_adv_u, *old_adv_v;
    double *next_u, *next_v, *p, *dp, *gx, *gy, *rhs, *balance;
    double *face_storage, *face_u, *face_v, *normal, *face_p, *face_dp, *flux, *bc, *bc_adv;
} CfdRefinedChannel;
bool cfd_refined_channel_init(CfdRefinedChannel *c, const CfdRefinedMesh *m, double rho, double mu,
                              double mean);
void cfd_refined_channel_destroy(CfdRefinedChannel *c);
bool cfd_refined_channel_step(CfdRefinedChannel *c, double dt);
/* Optional physical body acceleration at the new time, per cell, m/s^2.
 * NULL means zero. Used by manufactured verification and physical forcing. */
bool cfd_refined_channel_step_forced(CfdRefinedChannel *c, double dt, const double *acceleration_x,
                                     const double *acceleration_y);
#endif
