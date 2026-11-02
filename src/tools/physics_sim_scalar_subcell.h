/* Bounded 1D periodic subcell startup. Backward Euler upwind-advection /
 * centered-diffusion finite volumes form a conservative positive M-matrix.
 * Positive cyclic reduction avoids subtractive wrap correction in cold tails.
 * This is internal state evolved from initial means, never oracle replacement. */
#ifndef PHYSICS_SIM_SCALAR_SUBCELL_H
#define PHYSICS_SIM_SCALAR_SUBCELL_H
#include <math.h>
#include <stdbool.h>
#define PHYSICS_SIM_SUBCELL_MAX 4096
struct physics_sim_subcell {
    int n;double lower,upper;
    double inverse[PHYSICS_SIM_SUBCELL_MAX],p[PHYSICS_SIM_SUBCELL_MAX];
    double q[PHYSICS_SIM_SUBCELL_MAX],y[PHYSICS_SIM_SUBCELL_MAX];
};
static inline void physics_sim_subcell_open(struct physics_sim_subcell *s,double *v) {
    for(int i=0;i<s->n;i++)v[i]=(v[i]+(i?s->lower*v[i-1]:0))*s->inverse[i];
    for(int i=s->n-2;i>=0;i--)v[i]+=s->upper*s->inverse[i]*v[i+1];
}
static inline bool physics_sim_subcell_init(struct physics_sim_subcell *s,int n,
    double u,double alpha,double dx,double dt) {
    if(!s||n<4||n>PHYSICS_SIM_SUBCELL_MAX||!isfinite(u)||!isfinite(alpha)||alpha<0||
       !isfinite(dx)||dx<=0||!isfinite(dt)||dt<=0)return false;
    s->n=n;s->lower=dt*(alpha/(dx*dx)+fmax(u,0)/dx);
    s->upper=dt*(alpha/(dx*dx)+fmax(-u,0)/dx);
    double diagonal=1+s->lower+s->upper;
    for(int i=0;i<n;i++) {
        s->inverse[i]=1/(diagonal-(i?s->lower*s->upper*s->inverse[i-1]:0));
        s->p[i]=i==0?s->lower:0;s->q[i]=i==n-1?s->upper:0;
    }
    physics_sim_subcell_open(s,s->p);physics_sim_subcell_open(s,s->q);return true;
}
static inline bool physics_sim_subcell_step(struct physics_sim_subcell *s,double *v) {
    int n=s->n;
    for(int i=0;i<n;i++){if(!isfinite(v[i])||v[i]<0)return false;s->y[i]=v[i];}
    physics_sim_subcell_open(s,s->y);
    double a=1-s->q[0],d=1-s->p[n-1],det=a*d-s->p[0]*s->q[n-1];
    if(!(det>0))return false;
    double first=(d*s->y[0]+s->p[0]*s->y[n-1])/det;
    double last=(a*s->y[n-1]+s->q[n-1]*s->y[0])/det;
    for(int i=0;i<n;i++) {
        v[i]=s->y[i]+s->p[i]*last+s->q[i]*first;
        if(!isfinite(v[i])||v[i]<0)return false;
    }
    return true;
}
#endif
