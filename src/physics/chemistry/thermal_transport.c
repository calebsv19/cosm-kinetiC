#include "physics/chemistry/thermal.h"
#include "app/cfd_cartesian3d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static bool euler(const ChemMechanism *m,const ChemGrid *cg,const CfdCartesian3d *g,const ChemThermalModel *t,const ChemState *in,const double *Ein,const double *v,double dt,ChemState *out,double *Eout,double *T) {
 int n=g->count;memcpy(out,in,(size_t)n*sizeof(*out));memcpy(Eout,Ein,(size_t)n*sizeof(double));
 for(int q=0;q<n;q++) {
  if(!chem_thermal_temperature(m,t,in+q,g->volume,Ein[q],T+q)) return false;
  double bound=0;for(int a=0;a<3;a++) {int up=cfd_cartesian3d_neighbor(g,q,a,1);bound+=(fmax(v[a*n+up],0)+fmax(-v[a*n+q],0))/g->h[a]+2*(cg->diffusion+t->conductivity/t->carrier_cv)/(g->h[a]*g->h[a]);}
  if(!isfinite(bound) || dt*bound>.5) return false;
 }
 for(int q=0;q<n;q++) for(int a=0;a<3;a++) {
  int left=cfd_cartesian3d_neighbor(g,q,a,-1);double u=v[a*n+q],h=g->h[a],faceT=.5*(T[left]+T[q]);
  double Eflux=dt*u/h*(u>=0?Ein[left]:Ein[q])+dt*t->conductivity*g->area[a]/h*(T[left]-T[q]);
  for(int s=0;s<m->species;s++) {
   double diff=dt*cg->diffusion/(h*h)*(in[left].amount[s]-in[q].amount[s]);
   double x=dt*u/h*(u>=0?in[left].amount[s]:in[q].amount[s])+diff;
   out[left].amount[s]-=x;out[q].amount[s]+=x;
   Eflux+=diff*(t->formation[s]+t->cv[s]*(faceT-t->reference_t));
  }
  Eout[left]-=Eflux;Eout[q]+=Eflux;
 }
 for(int q=0;q<n;q++) {double temp;if(!chem_thermal_temperature(m,t,out+q,g->volume,Eout[q],&temp)) return false;}
 return true;
}
bool chem_thermal_transport(const ChemMechanism *m,const ChemGrid *cg,const ChemThermalModel *t,const ChemState *old,const double *E,const double *v,double dt,const ChemState *expected,double *result) {
 if(!old || !E || !v || !expected || !result || !chem_thermal_valid(m,t) || !chem_grid_valid(cg) || !isfinite(dt) || dt<=0) return false;
 CfdCartesian3d g;double lengths[3];for(int a=0;a<3;a++) {if(cg->boundary[a] || cg->n[a]<4) return false;lengths[a]=cg->n[a]*cg->spacing[a];}if(!cfd_cartesian3d_init(&g,cg->n,lengths) || g.count>4096) return false;
 int n=g.count;ChemState *a=malloc((size_t)n*sizeof(*a)),*b=malloc((size_t)n*sizeof(*b));double *data=calloc((size_t)4*n,sizeof(double));bool ok=a && b && data;
 if(ok) ok=euler(m,cg,&g,t,old,E,v,dt,a,data,data+3*n) && euler(m,cg,&g,t,a,data,v,dt,b,data+n,data+3*n);
 if(ok) for(int q=0;q<n;q++) {
  for(int s=0;s<m->species;s++) if(fabs(.5*old[q].amount[s]+.5*b[q].amount[s]-expected[q].amount[s])>1e-13*fmax(1,fabs(expected[q].amount[s]))) ok=false;
  data[2*n+q]=.5*E[q]+.5*data[n+q];double temp;if(!chem_thermal_temperature(m,t,expected+q,g.volume,data[2*n+q],&temp)) ok=false;
 }
 if(ok) memcpy(result,data+2*n,(size_t)n*sizeof(double));free(a);free(b);free(data);return ok;
}
