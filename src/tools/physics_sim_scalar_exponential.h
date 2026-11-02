/* Diagnostic face-local exponential through two positive cell averages.
 * On x/h in [-1,1], T(x)=face*exp(k*x/h). Its left/right integrals
 * are exactly l/r. Value and derivative therefore belong to one profile.
 * No temperature floor. Zeros, extrema and signed data use the retained target.
 * Adjacent faces fit separate profiles; this is not a global continuous fit. */
#ifndef PHYSICS_SIM_SCALAR_EXPONENTIAL_H
#define PHYSICS_SIM_SCALAR_EXPONENTIAL_H
#include "../app/cfd_scalar_edge_candidate.h"
static inline void physics_sim_scalar_exponential(double ll,double l,double r,
    double rr,double u,double h,double *value,double *gradient) {
    cfd_scalar_edge_candidate(ll,l,r,rr,u,h,value,gradient);
    if(!(ll>0 && l>0 && r>0 && rr>0))return;
    if(!((ll<=l && l<=r && r<=rr)||(ll>=l && l>=r && r>=rr)))return;
    if(l==r){*value=l;*gradient=0;return;}
    /* Log differences avoid overflow of r/l. Use the smaller mean so that
     * expm1 only sees a nonpositive argument, even across extreme ratios. */
    double k=log(r)-log(l),a=fabs(k);
    /* Only replace a sub-cell decay: |T/Tprime|=h/|k| < h.
     * Resolved smooth regions keep the higher-order polynomial target. */
    if(a<=1)return;
    double face=fmin(l,r)*a/(-expm1(-a)),g=k*face/h;
    if(isfinite(face) && isfinite(g)){*value=face;*gradient=g;}
}
#endif
