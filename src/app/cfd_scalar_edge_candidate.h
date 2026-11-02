/* Experimental periodic-x qualifier only; not an application default.
 * Fourth-order finite-volume face value/gradient from cell averages.
 * Bounds: nonnegative donor <= 2*upwind average; gradient has the sign
 * of R-L and magnitude <= 2*|R-L|/h. Thus outgoing rates are bounded
 * by 2*|u|/h and 2*alpha/h^2 per face. Conservation uses one paired flux.
 * Accuracy order is reduced wherever either bound activates. */
#ifndef CFD_SCALAR_EDGE_CANDIDATE_H
#define CFD_SCALAR_EDGE_CANDIDATE_H
#include <math.h>
static inline void cfd_scalar_edge_candidate(double ll,double l,double r,double rr,
    double u,double h,double *donor,double *gradient) {
    double value=(-ll+7*l+7*r-rr)/12;
    value=fmax(fmin(l,r),fmin(fmax(l,r),value));
    *donor=fmax(0,fmin(value,2*(u>=0?l:r)));
    double central=(r-l)/h,high=(ll-15*l+15*r-rr)/(12*h);
    *gradient=fmax(fmin(0,2*central),fmin(fmax(0,2*central),high));
}
#endif
