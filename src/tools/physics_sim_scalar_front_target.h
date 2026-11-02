/* Diagnostic sharp-front target; paired conservative FCT remains unchanged.
 * A wrong-sign high-order derivative must not suppress Fourier transport.
 * Fall back to the two-cell finite-volume gradient only in that case.
 * Variants 2/3 isolate exponential advection and central diffusion respectively;
 * they are controlled diagnostic ablations, not application choices. */
#ifndef PHYSICS_SIM_SCALAR_FRONT_TARGET_H
#define PHYSICS_SIM_SCALAR_FRONT_TARGET_H
#include "physics_sim_scalar_exponential.h"
static inline void physics_sim_scalar_front_target(double ll,double l,double r,
    double rr,double u,double h,double *value,double *gradient) {
    cfd_scalar_edge_candidate(ll,l,r,rr,u,h,value,gradient);
#if PHYSICS_SIM_FRONT_TARGET >= 2
    double unused;
    physics_sim_scalar_exponential(ll,l,r,rr,u,h,value,&unused);
#endif
    double central=(r-l)/h;
#if PHYSICS_SIM_FRONT_TARGET == 3
    *gradient=central;
#else
    if(*gradient==0 && central!=0)*gradient=central;
#endif
}
#endif
