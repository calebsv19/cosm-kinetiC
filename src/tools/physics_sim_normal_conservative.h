/* Uniform normal diffusion, conservative theta-method flux form. The positive matrix
 * supplies implicit face gradients; the accepted update applies their equal/opposite
 * heat fluxes. Centering removes the constant mode from the linear solve.
 * Compensated cell increments retain uniform source heat without an absolute
 * temperature round trip. No clipping, heat floor or tolerance relaxation. */
#ifndef PHYSICS_SIM_NORMAL_CONSERVATIVE_H
#define PHYSICS_SIM_NORMAL_CONSERVATIVE_H
#include "physics_sim_local_matrix.h"
static inline bool physics_sim_normal_conservative_step(PhysicsSimLocalMatrix *matrix,
    double *temperature,double *correction,int n,double alpha,double dt,double width,double source,double theta) {
    if(!matrix||!temperature||!correction||n<4||n>512||matrix->n!=n||
       !isfinite(alpha)||alpha<0||!isfinite(dt)||dt<=0||!isfinite(width)||width<=0||
       !isfinite(source)||source<0||(theta!=1&&theta!=.5))return false;
    const double coefficient=alpha*dt/(width*width);
    if(theta==.5&&coefficient>1)return false;
    double work[512],flux[512],next[512],low[512],base=temperature[0];
    for(int i=0;i<n;i++) {
        if(!isfinite(temperature[i])||temperature[i]<0||!isfinite(correction[i]))return false;
        base=fmin(base,temperature[i]);
    }
    /* Explicit side is a convex combination when (1-theta)*2*lambda<=1.
     * The implicit matrix must be initialized with theta*dt by the caller. */
    for(int i=0;i<n;i++) {
        double side=(1-theta)*coefficient;
        work[i]=(1-side*((i>0)+(i<n-1)))*(temperature[i]-base)
            +(i?side*(temperature[i-1]-base):0)
            +(i<n-1?side*(temperature[i+1]-base):0);
    }
    if(!physics_sim_local_matrix_step(matrix,work))return false;
    for(int i=0;i<n-1;i++)flux[i]=coefficient*(theta*(work[i]-work[i+1])+(1-theta)*(temperature[i]-temperature[i+1]));
    for(int i=0;i<n;i++) {
        double delta=(i?flux[i-1]:0)-(i<n-1?flux[i]:0)+source;
        double retained=delta-correction[i];next[i]=temperature[i]+retained;
        low[i]=(next[i]-temperature[i])-retained;
        if(!isfinite(next[i])||next[i]<0||!isfinite(low[i]))return false;
    }
    for(int i=0;i<n;i++){temperature[i]=next[i];correction[i]=low[i];}
    return true;
}
#endif
