#include "physics/chemistry/flow.h"
#include "app/cfd_cartesian3d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static bool fields(const ChemMechanism *m,const ChemGrid *g,const ChemFlowModel *f,const ChemState *c,const double *v,double *contrast,double *re) {
 int n=g->n[0]*g->n[1]*g->n[2];double vol=g->spacing[0]*g->spacing[1]*g->spacing[2],maxspeed=0,length=0;*contrast=0;
 for(int a=0;a<3;a++) length=fmax(length,g->n[a]*g->spacing[a]);
 for(int i=0;i<n;i++) {
  double delta=0,speed2=0;
  for(int s=0;s<m->species;s++) {if(!isfinite(c[i].amount[s]) || c[i].amount[s]<0) return false;delta+=f->beta[s]*(c[i].amount[s]/vol-f->reference[s]);}
  for(int a=0;a<3;a++) {if(!isfinite(v[a*n+i])) return false;speed2+=v[a*n+i]*v[a*n+i];}
  if(!isfinite(delta)) return false;
  *contrast=fmax(*contrast,fabs(delta));maxspeed=fmax(maxspeed,sqrt(speed2));
 }
 *re=maxspeed*length/f->nu;return isfinite(*contrast) && *contrast<=.01 && isfinite(*re) && *re<=.1;
}
static bool scalar_euler(const CfdCartesian3d *g,const ChemState *in,ChemState *out,int ns,const double *v,double D,double dt) {
 int n=g->count;memcpy(out,in,(size_t)n*sizeof(*out));
 for(int q=0;q<n;q++) {
  double bound=0;
  for(int a=0;a<3;a++) {int up=cfd_cartesian3d_neighbor(g,q,a,1);bound+=(fmax(v[a*n+up],0)+fmax(-v[a*n+q],0))/g->h[a]+2*D/(g->h[a]*g->h[a]);}
  if(!isfinite(bound) || dt*bound>1) return false;
 }
 for(int q=0;q<n;q++) for(int a=0;a<3;a++) {
  int left=cfd_cartesian3d_neighbor(g,q,a,-1);double u=v[a*n+q],h=g->h[a];
  for(int s=0;s<ns;s++) {
   double x=dt*(u/h*(u>=0?in[left].amount[s]:in[q].amount[s])+D/(h*h)*(in[left].amount[s]-in[q].amount[s]));
   out[left].amount[s]-=x;out[q].amount[s]+=x;
  }
 }
 for(int q=0;q<n;q++) for(int s=0;s<ns;s++) if(!isfinite(out[q].amount[s]) || out[q].amount[s]<0) return false;
 return true;
}
bool chem_flow_step_extra(const ChemMechanism *m,const ChemGrid *cg,const ChemFlowModel *f,ChemState *c,double *v,double *pressure,double dt,bool reacting,ChemFlowMetrics *metrics,const double *extra) {
 if(!c || !v || !pressure || !metrics || !f || !chem_valid(m) || !chem_grid_valid(cg) || !isfinite(dt) || dt<=0 || dt>.1 || !isfinite(f->rho) || f->rho<=0 || !isfinite(f->nu) || f->nu<=0 || (f->feedback!=0 && f->feedback!=1)) return false;
 for(int a=0;a<3;a++) if(cg->n[a]<4 || cg->boundary[a] || cg->velocity[a]!=0 || !isfinite(f->gravity[a])) return false;
 for(int s=0;s<m->species;s++) if(!isfinite(f->beta[s]) || !isfinite(f->reference[s]) || f->reference[s]<0) return false;
 CfdCartesian3d g;double length[3];for(int a=0;a<3;a++) length[a]=cg->n[a]*cg->spacing[a];
 if(!cfd_cartesian3d_init(&g,cg->n,length) || g.count>4096) return false;
 ChemFlowMetrics z={0};if(!fields(m,cg,f,c,v,&z.contrast,&z.reynolds)) return false;
 int n=g.count;size_t bytes=(size_t)n*sizeof(ChemState);
 ChemState *candidate=malloc(bytes),*t=malloc(bytes),*u=malloc(bytes);
 double *work=calloc((size_t)23*n,sizeof(double));
 bool periodic[3]={true,true,true};CfdCartesian3dLinear *H=cfd_cartesian3d_linear_create(&g,periodic,1,dt*f->nu,false),*P=cfd_cartesian3d_linear_create(&g,periodic,0,1,true);
 bool ok=candidate && t && u && work && H && P;
 if(!ok) goto done;
 double *rhs=work,*star=rhs+3*n,*next=star+3*n,*force=next+3*n,*grad=force+3*n,*div=grad+3*n,*phi=div+n,*hp=phi+n,*delta=hp+n,*scratch=delta+n;
 cfd_cartesian3d_divergence(&g,v,div);
 double mean=0,rate[CHEM_R];
 for(int q=0;q<n;q++) {
  if(!isfinite(pressure[q]) || !isfinite(div[q]) || fabs(div[q])>1e-8 || !chem_rates(m,c+q,g.volume,rate)) {ok=false;goto done;}
  for(int r=0;r<m->reactions;r++) if(!isfinite(c[q].extent[r]) || c[q].extent[r]<0) {ok=false;goto done;}
  for(int s=0;s<m->species;s++) delta[q]+=f->beta[s]*(c[q].amount[s]/g.volume-f->reference[s]);
  mean+=delta[q]/n;
 }
 for(int a=0;a<3;a++) for(int q=0;q<n;q++) {
  int left=cfd_cartesian3d_neighbor(&g,q,a,-1);force[a*n+q]=f->feedback?f->gravity[a]*(.5*(delta[q]+delta[left])-mean):0;
  if(extra) {if(!isfinite(extra[a*n+q])) {ok=false;goto done;}force[a*n+q]+=extra[a*n+q];}
  rhs[a*n+q]=v[a*n+q]+dt*force[a*n+q];
 }
 for(int a=0;a<3;a++) {int it;double res;if(!cfd_cartesian3d_linear_solve(H,rhs+a*n,star+a*n,&it,&res)) {ok=false;goto done;}}
 cfd_cartesian3d_divergence(&g,star,div);for(int q=0;q<n;q++) div[q]=-div[q];div[0]=0;
 {int it;double res;if(!cfd_cartesian3d_linear_solve(P,div,phi,&it,&res)) {ok=false;goto done;}}
 mean=0;for(int q=0;q<n;q++) mean+=phi[q]/n;for(int q=0;q<n;q++) phi[q]-=mean;
 cfd_cartesian3d_gradient(&g,phi,grad);for(int q=0;q<3*n;q++) next[q]=star[q]-grad[q];
 cfd_cartesian3d_divergence(&g,next,div);for(int q=0;q<n;q++) z.divergence=fmax(z.divergence,fabs(div[q]));
 if(!isfinite(z.divergence) || z.divergence>1e-8) {ok=false;goto done;}
 cfd_cartesian3d_linear_apply(H,phi,hp);cfd_cartesian3d_gradient(&g,hp,grad);
 double norm=0,err=0,oldenergy=0;
 for(int a=0;a<3;a++) {
  cfd_cartesian3d_linear_apply(H,next+a*n,scratch);
  for(int q=0;q<n;q++) {double error=scratch[q]+grad[a*n+q]-rhs[a*n+q];err+=error*error;norm+=rhs[a*n+q]*rhs[a*n+q];}
 }
 z.momentum_residual=sqrt(err/fmax(norm,1e-60));if(!isfinite(z.momentum_residual) || z.momentum_residual>1e-10) {ok=false;goto done;}
 memcpy(candidate,c,bytes);
 for(int q=0;reacting && q<n;q++) if(!chem_step(m,candidate+q,g.volume,dt*.5)) {ok=false;goto done;}
 if(!scalar_euler(&g,candidate,t,m->species,next,cg->diffusion,dt) || !scalar_euler(&g,t,u,m->species,next,cg->diffusion,dt)) {ok=false;goto done;}
 for(int q=0;q<n;q++) for(int s=0;s<m->species;s++) candidate[q].amount[s]=.5*candidate[q].amount[s]+.5*u[q].amount[s];
 for(int q=0;reacting && q<n;q++) if(!chem_step(m,candidate+q,g.volume,dt*.5)) {ok=false;goto done;}
 if(!fields(m,cg,f,candidate,next,&z.contrast,&z.reynolds)) {ok=false;goto done;}
 for(int s=0;s<m->species;s++) {
  double before=0,after=0,production=0;
  for(int q=0;q<n;q++) {before+=c[q].amount[s];after+=candidate[q].amount[s];for(int r=0;r<m->reactions;r++) production+=(m->product[r][s]-m->reactant[r][s])*(candidate[q].extent[r]-c[q].extent[r]);}
  if(!isfinite(after) || fabs(after-before-production)>1e-12*fmax(1,fabs(before))) {ok=false;goto done;}
 }
 for(int a=0;a<3;a++) for(int q=0;q<n;q++) {
  double vn=next[a*n+q],vo=v[a*n+q];oldenergy+=.5*f->rho*g.volume*vo*vo;z.kinetic+=.5*f->rho*g.volume*vn*vn;
  z.work+=dt*f->rho*g.volume*force[a*n+q]*vn;z.time_defect+=.5*f->rho*g.volume*(vn-vo)*(vn-vo);
  for(int b=0;b<3;b++) {int plus=cfd_cartesian3d_neighbor(&g,q,b,1);double derivative=(next[a*n+plus]-vn)/g.h[b];z.dissipation+=dt*f->rho*f->nu*g.volume*derivative*derivative;}
 }
 z.energy_residual=z.kinetic-oldenergy+z.dissipation+z.time_defect-z.work;
 if(!isfinite(z.energy_residual) || fabs(z.energy_residual)>1e-10*fmax(1e-12,fmax(oldenergy,fabs(z.work)))) {ok=false;goto done;}
 memcpy(c,candidate,bytes);memcpy(v,next,(size_t)3*n*sizeof(double));for(int q=0;q<n;q++) pressure[q]=f->rho*hp[q]/dt;*metrics=z;
 done:free(candidate);free(t);free(u);free(work);cfd_cartesian3d_linear_destroy(H);cfd_cartesian3d_linear_destroy(P);return ok;
}

bool chem_flow_step(const ChemMechanism *m,const ChemGrid *g,const ChemFlowModel *f,ChemState *c,double *v,double *p,double dt,bool reacting,ChemFlowMetrics *metrics) {
 return chem_flow_step_extra(m,g,f,c,v,p,dt,reacting,metrics,NULL);
}
