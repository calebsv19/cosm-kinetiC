#include "physics/chemistry/thermal.h"
#include "app/cfd_cartesian3d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static bool temperatures(const ChemMechanism *m,const ChemThermalModel *t,const ChemFlowModel *f,const CfdCartesian3d *g,const ChemState *c,const double *E,double *T) {
 for(int q=0;q<g->count;q++) {
  if(!chem_thermal_temperature(m,t,c+q,g->volume,E[q],T+q)) return false;
  double contrast=-t->expansion*(T[q]-t->reference_t);for(int s=0;s<m->species;s++) contrast+=f->beta[s]*(c[q].amount[s]/g->volume-f->reference[s]);
  if(!isfinite(contrast) || fabs(contrast)>.01) return false;
 }
 return true;
}
bool chem_thermal_flow_step(const ChemMechanism *m,const ChemGrid *cg,const ChemFlowModel *f,const ChemThermalModel *t,ChemState *c,double *v,double *p,double *energy,double dt,bool reacting,ChemFlowMetrics *metrics,double *external,double *viscous) {
 if(!c || !v || !p || !energy || !metrics || !external || !viscous || !f || !chem_thermal_valid(m,t) || !chem_grid_valid(cg) || !isfinite(dt) || dt<=0 || dt>.1) return false;
 CfdCartesian3d g;double length[3];for(int a=0;a<3;a++) {if(cg->n[a]<4 || cg->boundary[a]) return false;length[a]=cg->n[a]*cg->spacing[a];}
 if(!cfd_cartesian3d_init(&g,cg->n,length) || g.count>4096) return false;
 int n=g.count;size_t bytes=(size_t)n*sizeof(ChemState);ChemState *candidate=malloc(bytes),*old=malloc(bytes);double *data=calloc((size_t)13*n,sizeof(double));bool ok=candidate && old && data;double Q=0,Qvisc=0;ChemFlowMetrics stat={0};
 if(!ok) goto done;
 double *vn=data,*pn=vn+3*n,*E=pn+n,*Eold=E+n,*T=Eold+n,*extra=T+n,*output=extra+3*n;
 memcpy(candidate,c,bytes);memcpy(vn,v,(size_t)3*n*sizeof(double));memcpy(pn,p,(size_t)n*sizeof(double));memcpy(E,energy,(size_t)n*sizeof(double));
 if(!temperatures(m,t,f,&g,candidate,E,T)) {ok=false;goto done;}
 ChemMechanism chemistry=*m;if(!reacting) for(int r=0;r<m->reactions;r++) chemistry.k[r]=0;
 for(int q=0;q<n;q++) {double heat;if(!chem_thermal_react(&chemistry,t,candidate+q,E+q,g.volume,dt*.5,&heat)) {ok=false;goto done;}Q+=heat;}
 if(!temperatures(m,t,f,&g,candidate,E,T)) {ok=false;goto done;}
 double mean=0;for(int q=0;q<n;q++) mean+=-t->expansion*(T[q]-t->reference_t)/n;
 for(int a=0;a<3;a++) for(int q=0;q<n;q++) {int left=cfd_cartesian3d_neighbor(&g,q,a,-1);extra[a*n+q]=f->feedback?f->gravity[a]*(-t->expansion*(.5*(T[left]+T[q])-t->reference_t)-mean):0;}
 memcpy(old,candidate,bytes);memcpy(Eold,E,(size_t)n*sizeof(double));
 if(!chem_flow_step_extra(m,cg,f,candidate,vn,pn,dt,false,&stat,extra) || !chem_thermal_transport(m,cg,t,old,Eold,vn,dt,candidate,output)) {ok=false;goto done;}
 memcpy(E,output,(size_t)n*sizeof(double));
 for(int a=0;a<3;a++) for(int q=0;q<n;q++) for(int b=0;b<3;b++) {
  int plus=cfd_cartesian3d_neighbor(&g,q,b,1);double derivative=(vn[a*n+plus]-vn[a*n+q])/g.h[b];double heat=dt*f->rho*f->nu*g.volume*derivative*derivative;E[q]+=heat;Qvisc+=heat;
 }
 if(!isfinite(Qvisc) || fabs(Qvisc-stat.dissipation)>1e-10*fmax(1e-12,fabs(stat.dissipation))) {ok=false;goto done;}

 for(int q=0;q<n;q++) {double heat;if(!chem_thermal_react(&chemistry,t,candidate+q,E+q,g.volume,dt*.5,&heat)) {ok=false;goto done;}Q+=heat;}
 if(!temperatures(m,t,f,&g,candidate,E,T)) {ok=false;goto done;}
 stat.contrast=0;for(int q=0;q<n;q++) {double solutal=0;for(int s=0;s<m->species;s++) solutal+=f->beta[s]*(candidate[q].amount[s]/g.volume-f->reference[s]);stat.contrast=fmax(stat.contrast,fabs(solutal));}
 if(stat.contrast>.01) {ok=false;goto done;}
 double before=0,after=0;for(int q=0;q<n;q++) {before+=energy[q];after+=E[q];}
 if(!isfinite(Q) || !isfinite(after) || fabs(after-before-Q-Qvisc)>1e-9*fmax(1,fmax(fabs(after-before),fabs(Q)+Qvisc))) {ok=false;goto done;}
 memcpy(c,candidate,bytes);memcpy(v,vn,(size_t)3*n*sizeof(double));memcpy(p,pn,(size_t)n*sizeof(double));memcpy(energy,E,(size_t)n*sizeof(double));*metrics=stat;*external=Q;*viscous=Qvisc;
 done:free(candidate);free(old);free(data);return ok;
}
