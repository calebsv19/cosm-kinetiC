#include "physics/chemistry/phase.h"
#include <math.h>
static const double gas_constant=8.31446261815324;
bool chem_phase_valid(const ChemPhaseModel *m) {
    if(!m) return false;
    const double values[]={m->temperature,m->henry,m->delta_u,m->gas_volume,m->liquid_volume,m->area,m->permeance};
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++) if(!isfinite(values[i])) return false;
    return m->temperature>=273 && m->temperature<=303 && m->henry>0 && m->gas_volume>=1e-15 && m->gas_volume<=1 && m->liquid_volume>=1e-15 && m->liquid_volume<=1 && m->area>=0 && m->permeance>=0;
}
bool chem_phase_admit(const ChemPhaseModel *m,const ChemState *s,double energy) {
    if(!chem_phase_valid(m) || !s || !isfinite(energy)) return false;
    for(int i=0;i<2;i++) if(!isfinite(s->amount[i]) || s->amount[i]<0 || !isfinite(s->extent[i]) || s->extent[i]<0) return false;
    double p=s->amount[0]*gas_constant*m->temperature/m->gas_volume,c=s->amount[1]/m->liquid_volume;
    return isfinite(p) && isfinite(c) && p<=10000*(1+1e-12) && c<=m->henry*10000*(1+1e-12) && fabs(energy-m->delta_u*s->amount[1])<=1e-11*fmax(1e-12,fabs(energy));
}
bool chem_phase_step(const ChemPhaseModel *m,ChemState *s,double *energy,double dt,double *heat) {
    if(!energy || !heat || !chem_phase_admit(m,s,*energy) || !isfinite(dt) || dt<0 || dt>1e6) return false;
    double conductance=m->area*m->permeance,a=conductance*gas_constant*m->temperature/m->gas_volume,b=conductance/(m->henry*m->liquid_volume),lambda=a+b;
    if(!isfinite(lambda)) return false;
    if(lambda==0 || dt==0) {*heat=0;return true;}
    double total=s->amount[0]+s->amount[1],liquid_eq=total*(a/lambda),relax=-expm1(-lambda*dt),change=(liquid_eq-s->amount[1])*relax;
    ChemState candidate=*s;candidate.amount[1]=s->amount[1]+change;candidate.amount[0]=total-candidate.amount[1];
    /* Stable integrated occupations, including very small lambda*dt. */
    double z=lambda*dt,tail=z<1e-3?dt*z*(.5-z/6+z*z/24-z*z*z/120+z*z*z*z/720):dt-relax/lambda;
    double integral_l=s->amount[1]*dt+(liquid_eq-s->amount[1])*tail;
    double integral_g=s->amount[0]*dt-(liquid_eq-s->amount[1])*tail;
    candidate.extent[0]+=a*integral_g;candidate.extent[1]+=b*integral_l;
    double Q=m->delta_u*change,E=*energy+Q;
    if(!isfinite(Q) || !chem_phase_admit(m,&candidate,E) || fabs((candidate.extent[0]-s->extent[0])-(candidate.extent[1]-s->extent[1])-change)>1e-10*fmax(1e-12,total)) return false;
    *s=candidate;*energy=E;*heat=Q;return true;
}
