#include "physics/chemistry/transport.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
bool chem_grid_valid(const ChemGrid *g) {
 if(!g || !isfinite(g->diffusion) || g->diffusion<0) return false;
 int count=1;
 for(int a=0;a<3;a++) {
  if(g->n[a]<1 || g->n[a]>4096 || count>4096/g->n[a] || (g->boundary[a]!=0 && g->boundary[a]!=1) || !isfinite(g->spacing[a]) || g->spacing[a]<=0 || !isfinite(g->velocity[a])) return false;
  if((g->boundary[a]==1 || g->n[a]==1) && g->velocity[a]!=0) return false;
  count*=g->n[a];
 }
 double volume=g->spacing[0]*g->spacing[1]*g->spacing[2];
 return isfinite(volume) && volume>0;
}
static bool euler(const ChemGrid *g,const ChemState *in,ChemState *out,int ns,double dt) {
 int count=g->n[0]*g->n[1]*g->n[2],stride[3]={1,g->n[0],g->n[0]*g->n[1]};
 memcpy(out,in,(size_t)count*sizeof(*out));
 for(int i=0;i<count;i++) for(int a=0;a<3;a++) {
  if(g->n[a]==1) continue;
  int x=(i/stride[a])%g->n[a],j=i+stride[a];
  if(x==g->n[a]-1) {if(g->boundary[a]) continue;j=i-(g->n[a]-1)*stride[a];}
  for(int s=0;s<ns;s++) {
   double u=g->velocity[a],h=g->spacing[a];
   double transfer=dt*(u/h*(u>=0?in[i].amount[s]:in[j].amount[s])+g->diffusion/(h*h)*(in[i].amount[s]-in[j].amount[s]));
   if(!isfinite(transfer)) return false;
   out[i].amount[s]-=transfer;out[j].amount[s]+=transfer;
  }
 }
 for(int i=0;i<count;i++) for(int s=0;s<ns;s++) if(!isfinite(out[i].amount[s]) || out[i].amount[s]<0) return false;
 return true;
}
bool chem_spatial_step(const ChemMechanism *m,const ChemGrid *g,ChemState *cells,double dt,bool reacting) {
 if(!cells || !chem_valid(m) || !chem_grid_valid(g) || !isfinite(dt) || dt<0) return false;
 int count=g->n[0]*g->n[1]*g->n[2];double bound=0;
 for(int a=0;a<3;a++) if(g->n[a]>1) bound+=fabs(g->velocity[a])/g->spacing[a]+2*g->diffusion/(g->spacing[a]*g->spacing[a]);
 if(!isfinite(bound) || dt*bound>1) return false;
 double volume=g->spacing[0]*g->spacing[1]*g->spacing[2],rates[CHEM_R];
 for(int i=0;i<count;i++) {
  if(!chem_rates(m,cells+i,volume,rates)) return false;
  for(int r=0;r<m->reactions;r++) if(!isfinite(cells[i].extent[r]) || cells[i].extent[r]<0) return false;
 }
 if(dt==0) return true;
 size_t bytes=(size_t)count*sizeof(ChemState);
 ChemState *c=malloc(bytes),*t=malloc(bytes),*u=malloc(bytes);bool ok=c && t && u;
 if(ok) memcpy(c,cells,bytes);
 for(int i=0;ok && reacting && i<count;i++) ok=chem_step(m,c+i,volume,dt*.5);
 if(ok) ok=euler(g,c,t,m->species,dt) && euler(g,t,u,m->species,dt);
 if(ok) for(int i=0;i<count;i++) for(int s=0;s<m->species;s++) c[i].amount[s]=.5*c[i].amount[s]+.5*u[i].amount[s];
 for(int i=0;ok && reacting && i<count;i++) ok=chem_step(m,c+i,volume,dt*.5);
 /* Whole-domain species budget includes the reaction extents from this step.
  * Periodic/sealed faces exchange no material with the exterior. */
 for(int species=0;ok && species<m->species;species++) {
  double before=0,after=0,production=0;
  for(int i=0;i<count;i++) {
   before+=cells[i].amount[species];after+=c[i].amount[species];
   for(int r=0;r<m->reactions;r++) production+=(m->product[r][species]-m->reactant[r][species])*(c[i].extent[r]-cells[i].extent[r]);
  }
  if(!isfinite(before) || !isfinite(after) || !isfinite(production) || fabs(after-before-production)>1e-12*fmax(1,fabs(before))) ok=false;
 }
 if(ok) memcpy(cells,c,bytes);
 free(c);free(t);free(u);return ok;
}
