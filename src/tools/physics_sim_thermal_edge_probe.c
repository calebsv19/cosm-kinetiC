/* Frozen reference-field diagnostic: native scalar reconstruction/flux and
 * column exchange. No momentum evolution or long trajectory. */
#include "physics_sim_heated_face.h"
#include "app/cfd_passive3d.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../app/cfd_open_atmosphere3d.c"
static double read_value(FILE *f){double x;assert(fread(&x,sizeof x,1,f)==1&&isfinite(x));return x;}
static void array(const char *name,const double *v,int n){printf(",\"%s\":[",name);for(int i=0;i<n;i++)printf("%s%.17g",i?",":"",v[i]);printf("]");}
static double adv_flux(const CfdOpenAtmosphere3d *s,int q){
 int left=cfd_cartesian3d_neighbor(&s->grid,q,0,-1);
 #ifdef PHYSICS_SIM_OPEN_EDGE_CANDIDATE
 double donor,gradient;
 cfd_scalar_edge_candidate(s->energy[cfd_cartesian3d_neighbor(&s->grid,left,0,-1)],s->energy[left],s->energy[q],s->energy[cfd_cartesian3d_neighbor(&s->grid,q,0,1)],s->velocity[q],s->grid.h[0],&donor,&gradient);
 return s->velocity[q]*donor/s->grid.h[0];
#else
 return s->velocity[q]*(s->energy[left]+.5*scalar_slope(s,s->energy,left,0))/s->grid.h[0];
#endif
}
static double diff_flux(const CfdOpenAtmosphere3d *s,int q){
 int left=cfd_cartesian3d_neighbor(&s->grid,q,0,-1);
 #ifdef PHYSICS_SIM_OPEN_EDGE_CANDIDATE
 double donor,gradient;
 cfd_scalar_edge_candidate(s->energy[cfd_cartesian3d_neighbor(&s->grid,left,0,-1)],s->energy[left],s->energy[q],s->energy[cfd_cartesian3d_neighbor(&s->grid,q,0,1)],s->velocity[q],s->grid.h[0],&donor,&gradient);
 return -(.026/1206)*gradient/s->grid.h[0];
#else
 return -(.026/1206)*(s->energy[q]-s->energy[left])/(s->grid.h[0]*s->grid.h[0]);
#endif
}
int main(int argc,char **argv){
 if(argc!=6)return 2;int nx=atoi(argv[1]),nz=atoi(argv[2]);double dt=atof(argv[4]);
 if((nx!=64&&nx!=128)||(nz!=64&&nz!=128)||(dt!=.003125&&dt!=.000001)||(strcmp(argv[5],"mc")&&strcmp(argv[5],"minmod")))return 2;
 CfdMemoryBudget budget={.limit_bytes=64*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 int n[3]={nx,4,nz};double length[3]={.25,.25,.0625},zero[2]={0},ambient[2]={300,300};CfdOpenAtmosphere3d s;
 assert(cfd_open_atmosphere3d_init(&s,n,length,1.2,1.8e-5,1005,300,.026,0,1e-6,zero,ambient,zero,false,0,0,.1));
 s.ground=true;s.muscl=true;s.scalar_mc=!strcmp(argv[5],"mc");
 FILE *f=fopen(argv[3],"rb");assert(f);double capacity=1206*s.grid.volume,negative_min=0,clipped_j=0,ts[128];
 for(int k=0;k<nz;k++)for(int x=0;x<nx;x++){
  double v=read_value(f);negative_min=fmin(negative_min,v);assert(v>=-1e-12);
  if(v<0)clipped_j+=-v*capacity*4;
  for(int y=0;y<4;y++)s.energy[k*s.plane+y*nx+x]=fmax(0,v)*capacity;
 }
 for(int x=0;x<nx;x++){double v=read_value(f);negative_min=fmin(negative_min,v);assert(v>=-1e-12);ts[x]=fmax(0,v);}
 for(int i=0;i<5*nx;i++)(void)read_value(f);assert(fgetc(f)==EOF);fclose(f);
 for(int q=0;q<s.grid.count;q++)s.velocity[q]=.05*(q/s.plane+.5)/nz;
 memcpy(s.candidate_velocity,s.velocity,(size_t)s.velocity_count*sizeof(double));
 double a[256]={0},d[256]={0},full[128]={0};
 for(int k=0;k<nz;k++)for(int y=0;y<4;y++)for(int x=0;x<nx;x++){
  int q=k*s.plane+y*nx+x;double af=adv_flux(&s,q),df=diff_flux(&s,q);full[x]+=af+df;
  if(k<nz/8){a[x]+=af;d[x]+=df;}
  if(k>=nz/4&&k<nz/2){a[nx+x]+=af;d[nx+x]+=df;}
 }
 double *source=cfd_memory_calloc((size_t)s.grid.count,sizeof(double));assert(source);
 assert(scalar_euler(&s,s.energy,s.scalar_candidate,source,1e-6,0,.026/1206,0,1));
 double mismatch=0,norm=0;
 for(int x=0;x<nx;x++){
  double derivative=0;for(int k=0;k<nz;k++)for(int y=0;y<4;y++){int q=k*s.plane+y*nx+x;derivative+=(s.scalar_candidate[q]-s.energy[q])/1e-6;}
  double net=full[x]-full[(x+1)%nx];mismatch+=fabs(derivative-net);norm+=fabs(net);
 }
 assert(mismatch/fmax(norm,1e-30)<1e-7);
 PhysicsSimHeatedFace faces[128];double initial[128],exact[128],instant[128],actual[128],maxrel=0;
 double h=1/(s.grid.h[2]/(2*.026)+.1+.005/(2*.16)),cs=50./nx,cg=4*capacity,g=h*.0625/nx;
 for(int x=0;x<nx;x++){
  initial[x]=0;for(int y=0;y<4;y++)initial[x]+=s.energy[y*nx+x];
  double tg=initial[x]/cg;
  instant[x]=g*(tg-ts[x]);exact[x]=(tg-ts[x])/(1/cg+1/cs)*(-expm1(-g*(1/cg+1/cs)*dt));
  faces[x]=(PhysicsSimHeatedFace){2,-1,300+ts[x],cs,h,0,ts[x]-((300+ts[x])-300)};
 }
 assert(physics_sim_heated_columns_exchange(&s,faces,nx,dt));double balance=0;
 for(int x=0;x<nx;x++){
  actual[x]=faces[x].gas_to_surface_j;double debit=initial[x];for(int y=0;y<4;y++)debit-=s.energy[y*nx+x];
  balance=fmax(balance,fabs(debit-actual[x]));if(fabs(exact[x])>1e-25)maxrel=fmax(maxrel,fabs(actual[x]-exact[x])/fabs(exact[x]));
 }
 assert(balance<1e-12);
 printf("{\"nx\":%d,\"nz\":%d,\"dt\":%.17g,\"mode\":\"%s\",\"clipped_gas_j\":%.17g,\"negative_min_k\":%.17g,\"native_euler_flux_parity\":%.17g,\"exchange_balance_j\":%.17g,\"exchange_pair_max_relative\":%.17g",nx,nz,dt,argv[5],clipped_j,negative_min,mismatch/fmax(norm,1e-30),balance,maxrel);
 array("advection_w",a,2*nx);array("diffusion_w",d,2*nx);array("wall_instant_w",instant,nx);array("wall_exact_frozen_j",exact,nx);array("wall_native_j",actual,nx);
 cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);printf(",\"live_bytes\":%zu,\"peak_bytes\":%zu}\n",budget.live_bytes,budget.peak_bytes);return budget.live_bytes?3:0;
}
