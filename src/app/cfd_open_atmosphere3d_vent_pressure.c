/* One residual-correction solve for new prescribed channel/paired boundaries.
 * Near shutoff the net boundary flow can be far smaller than the pressure RHS.
 * Refine the same operator instead of relaxing pressure or air-volume gates. */
#include "app/cfd_open_atmosphere3d.h"
#include <math.h>
#include <string.h>
static void apply(CfdOpenAtmosphere3d *s,CfdCartesian3dLinear *solver,const double *x,double *out) {
    if(solver)cfd_cartesian3d_linear_apply(solver,x,out);
    else cfd_open_atmosphere3d_box_apply(s,x,out);
}
bool cfd_open_wind_pressure_refine(CfdOpenAtmosphere3d *s,CfdCartesian3dLinear *solver,double *residual) {
    int n=s->grid.count;
    double *truth=s->scalar_work,*r=truth+n,*correction=s->scalar_candidate;
    apply(s,solver,s->candidate_pressure,truth);
    double norm=0;
    for(int q=0;q<n;q++){r[q]=s->rhs[q]-truth[q];norm+=s->rhs[q]*s->rhs[q];correction[q]=0;}
    double correction_residual;bool ok;
    if(solver) {
        int iterations;ok=cfd_cartesian3d_linear_solve(solver,r,correction,&iterations,&correction_residual);
    } else {
        double *rhs=s->rhs,*pressure=s->candidate_pressure;
        s->rhs=r;s->candidate_pressure=correction;
        ok=cfd_open_atmosphere3d_box_pressure(s,&correction_residual);
        s->rhs=rhs;s->candidate_pressure=pressure;
    }
    if(!ok)return false;
    for(int q=0;q<n;q++){s->candidate_pressure[q]+=correction[q];if(!isfinite(s->candidate_pressure[q]))return false;}
    apply(s,solver,s->candidate_pressure,truth);
    double error=0;
    for(int q=0;q<n;q++){double delta=s->rhs[q]-truth[q];error+=delta*delta;}
    *residual=norm?sqrt(error/norm):sqrt(error);
    return isfinite(*residual) && *residual<1e-11;
}
