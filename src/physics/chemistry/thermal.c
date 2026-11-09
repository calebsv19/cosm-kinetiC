#include "physics/chemistry/thermal.h"
#include <math.h>
#include <string.h>
bool chem_thermal_valid(const ChemMechanism *m,const ChemThermalModel *t) {
 if(!chem_valid(m) || !t || !isfinite(t->reference_t) || !isfinite(t->min_t) || !isfinite(t->max_t) || t->min_t<=0 || t->max_t<t->min_t || t->reference_t<t->min_t || t->reference_t>t->max_t || !isfinite(t->carrier_cv) || t->carrier_cv<=0) return false;
 if(!isfinite(t->conductivity) || t->conductivity<0 || !isfinite(t->bath_t) || t->bath_t<t->min_t || t->bath_t>t->max_t || !isfinite(t->bath_conductance) || t->bath_conductance<0 || !isfinite(t->power) || !isfinite(t->expansion) || t->expansion<0) return false;
 for(int s=0;s<m->species;s++) if(!isfinite(t->formation[s]) || !isfinite(t->cv[s]) || t->cv[s]<0) return false;
 for(int r=0;r<m->reactions;r++) {
  if(!isfinite(t->activation[r]) || t->activation[r]<0) return false;
  for(int q=0;q<r;q++) if(m->k[r]>0 && m->k[q]>0) {
   bool reverse=true;for(int s=0;s<m->species;s++) if(m->reactant[r][s]!=m->product[q][s] || m->product[r][s]!=m->reactant[q][s]) reverse=false;
   if(reverse) return false; /* Reverse kinetics requires an entropy/equilibrium model. */
  }
 }
 return true;
}
static bool caloric(const ChemMechanism *m,const ChemThermalModel *t,const ChemState *s,double v,double *capacity,double *formation) {
 if(!s || !chem_thermal_valid(m,t) || !isfinite(v) || v<=0) return false;
 *capacity=v*t->carrier_cv;*formation=0;
 for(int i=0;i<m->species;i++) {if(!isfinite(s->amount[i]) || s->amount[i]<0) return false;*capacity+=s->amount[i]*t->cv[i];*formation+=s->amount[i]*t->formation[i];}
 return isfinite(*capacity) && *capacity>0 && isfinite(*formation);
}
bool chem_thermal_energy(const ChemMechanism *m,const ChemThermalModel *t,const ChemState *s,double v,double T,double *out) {
 double cap,form;if(!out || !caloric(m,t,s,v,&cap,&form) || !isfinite(T) || T<t->min_t || T>t->max_t) return false;
 double energy=form+cap*(T-t->reference_t);if(!isfinite(energy)) return false;*out=energy;return true;
}
bool chem_thermal_temperature(const ChemMechanism *m,const ChemThermalModel *t,const ChemState *s,double v,double energy,double *out) {
 double cap,form;if(!out || !caloric(m,t,s,v,&cap,&form) || !isfinite(energy)) return false;
 double T=t->reference_t+(energy-form)/cap;if(!isfinite(T) || T<t->min_t || T>t->max_t) return false;*out=T;return true;
}
static bool derivative(const ChemMechanism *m,const ChemThermalModel *t,const ChemState *s,double energy,double v,double *d,double *rates,double *power) {
 double T;if(!chem_thermal_temperature(m,t,s,v,energy,&T)) return false;
 ChemMechanism evaluated=*m;
 for(int r=0;r<m->reactions;r++) {evaluated.k[r]=m->k[r]*exp(-t->activation[r]/8.31446261815324*(1/T-1/t->reference_t));if(!isfinite(evaluated.k[r])) return false;}
 if(!chem_rates(&evaluated,s,v,rates)) return false;
 for(int i=0;i<m->species;i++) {d[i]=0;for(int r=0;r<m->reactions;r++) d[i]+=(m->product[r][i]-m->reactant[r][i])*rates[r];if(!isfinite(d[i])) return false;}
 *power=v*(t->power+t->bath_conductance*(t->bath_t-T));return isfinite(*power);
}
bool chem_thermal_react(const ChemMechanism *m,const ChemThermalModel *t,ChemState *s,double *energy,double v,double dt,double *external) {
 if(!s || !energy || !external || !chem_thermal_valid(m,t) || !isfinite(dt) || dt<0) return false;
 double cap,form;if(!caloric(m,t,s,v,&cap,&form) || dt*v*t->bath_conductance/cap>1) return false;
 for(int r=0;r<m->reactions;r++) if(!isfinite(s->extent[r]) || s->extent[r]<0) return false;
 ChemState c=*s,stage=*s;double d[4][CHEM_S]={{0}},q[4][CHEM_R]={{0}},p[4]={0};
 for(int k=0;k<4;k++) {
  double E=*energy;
  if(k) {double scale=dt*(k==3?1:.5);for(int i=0;i<m->species;i++) stage.amount[i]=s->amount[i]+scale*d[k-1][i];E+=scale*p[k-1];}
  if(!derivative(m,t,&stage,E,v,d[k],q[k],p+k)) return false;
 }
 for(int r=0;r<m->reactions;r++) {double x=dt*(q[0][r]+2*q[1][r]+2*q[2][r]+q[3][r])/6;c.extent[r]+=x;if(!isfinite(c.extent[r])) return false;for(int i=0;i<m->species;i++) c.amount[i]+=(m->product[r][i]-m->reactant[r][i])*x;}
 double heat=dt*(p[0]+2*p[1]+2*p[2]+p[3])/6,E=*energy+heat,T;
 if(!chem_thermal_temperature(m,t,&c,v,E,&T)) return false;
 for(int e=0;e<=m->elements;e++) {double before=0,after=0;for(int i=0;i<m->species;i++) {int w=e==m->elements?m->charge[i]:m->atoms[i][e];before+=w*s->amount[i];after+=w*c.amount[i];}if(!isfinite(after) || fabs(after-before)>1e-12*fmax(1,fabs(before))) return false;}
 *s=c;*energy=E;*external=heat;return true;
}
