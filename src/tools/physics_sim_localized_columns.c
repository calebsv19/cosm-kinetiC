/* Localized x subcells under prescribed z shear, y-invariant gas means.
 * Wall support remains 128 native strips; source remains continuous to .4 s. */
#include "physics_sim_localized_columns.h"
#include "physics_sim_shear_clock.h"
#include "physics_sim_local_matrix.h"
#ifdef PHYSICS_SIM_REPAIRED_SCALARS
#include "physics_sim_nonuniform_diffusion.h"
/* One sequential workspace per operator; shared across z rows. */
static PhysicsSimNonuniformAdvection repaired_advection;
static PhysicsSimNonuniformDiffusion repaired_diffusion;
#ifdef PHYSICS_SIM_TAIL_DIFFUSION
#include "physics_sim_tail_diffusion.h"
static PhysicsSimTailDiffusion tail_x,tail_z;
#ifdef PHYSICS_SIM_TAIL_TIME
#include "physics_sim_tail_time.h"
#ifndef PHYSICS_SIM_GAS_PARALLEL_LINES
static PhysicsSimTailTime time_x,time_z;
#endif
#endif
#endif
#endif
#ifdef PHYSICS_SIM_SHEAR_GAS_REPAIRED
#if !defined(PHYSICS_SIM_TAIL_TIME) || !defined(PHYSICS_SIM_TAIL_DIFFUSION) || !defined(PHYSICS_SIM_SHEAR_NORMAL256)
#error "repaired gas profile requires the fixed normal256 time/diffusion profile"
#endif
#include "physics_sim_source_patch.h"
static PhysicsSimSourcePatch source_patch;
static double patch_power_x[4096],patch_power_z[256];
#ifdef PHYSICS_SIM_GAS_PARALLEL_LINES
#include "physics_sim_shear_gas_lines.h"
#endif
#endif
#define PHYSICS_SIM_LOGQUADRATIC_TARGET 1
#include "physics_sim_scalar_fct.h"
#include <stdio.h>
#include <string.h>
static const double alpha=.026/1206,dx=.25/128;
static int nf,nz,offset[129],fine_steps,coarse_steps,accepted_n,candidate_n;
static int restriction_tick,last_source_tick;
static double dz,*width,*center,*accepted,*candidate,*row;
static PhysicsSimLocalMatrix xm[PHYSICS_SIM_SHEAR_NORMAL_LIMIT],zf,zc;
static double max_lift_balance,max_restriction,minimum=0,maximum=0;
static uint64_t work_cells;
static int response_ready;static double mean_retention[128],deviation_retention[128];
#ifndef PHYSICS_SIM_LOCAL_MICRO_FACTOR
#define PHYSICS_SIM_LOCAL_MICRO_FACTOR 1
#endif
#ifndef PHYSICS_SIM_LOCAL_FINE_END
#define PHYSICS_SIM_LOCAL_FINE_END 48
#endif
static double parent_mean(const double *v,int k,int x,int n){
 if(n==128)return v[k*n+x];double sum=0;int count=offset[x+1]-offset[x];
 for(int j=offset[x];j<offset[x+1];j++)sum+=v[k*n+j]/count;return sum;
}
#ifdef PHYSICS_SIM_INTERFACE_TRACE
#include "physics_sim_local_interface_trace.h"
#endif
static double inventory(const double *v,int n){double sum=0;
 for(int k=0;k<nz;k++)for(int j=0;j<n;j++)sum+=v[k*n+j]*1206*.25*dz*(n==128?dx:width[j]);return sum;
}
static void project(CfdOpenAtmosphere3d *gas){
 for(int k=0;k<nz;k++)for(int x=0;x<128;x++){
  double energy=parent_mean(candidate,k,x,candidate_n)*1206*gas->grid.volume;
  for(int y=0;y<4;y++)gas->energy[(k*4+y)*128+x]=energy;
 }
}
int physics_sim_localized_init(const CfdOpenAtmosphere3d *gas){
 if(gas->grid.n[0]!=128||gas->grid.n[1]!=4||gas->grid.n[2]>PHYSICS_SIM_SHEAR_NORMAL_LIMIT||gas->dt!=1./PHYSICS_SIM_SHEAR_TPS)return 0;
 nz=gas->grid.n[2];dz=.0625/nz;nf=0;
 for(int x=0;x<128;x++){offset[x]=nf;nf+=(x<PHYSICS_SIM_LOCAL_FINE_END||x>=112)?32:1;}offset[128]=nf;
 width=cfd_memory_calloc((size_t)nf,sizeof(double));center=cfd_memory_calloc((size_t)nf,sizeof(double));
 accepted=cfd_memory_calloc((size_t)nf*nz,sizeof(double));candidate=cfd_memory_calloc((size_t)nf*nz,sizeof(double));row=cfd_memory_calloc((size_t)nf,sizeof(double));
 if(!width||!center||!accepted||!candidate||!row)return 0;
 for(int x=0;x<128;x++){int count=offset[x+1]-offset[x];for(int j=offset[x];j<offset[x+1];j++){width[j]=dx/count;center[j]=x*dx+(j-offset[x]+.5)*width[j];}}
 #ifndef PHYSICS_SIM_SHEAR_NORMAL256
 double finest=dx/32,rate=.05/finest+2*alpha/(finest*finest)+2*alpha/(dz*dz);
 fine_steps=(int)ceil(rate/3200)*PHYSICS_SIM_LOCAL_MICRO_FACTOR;if(fine_steps<1)fine_steps=1;
 coarse_steps=PHYSICS_SIM_LOCAL_MICRO_FACTOR;
#else
 fine_steps=PHYSICS_SIM_SHEAR_FINE_TPS/(2*PHYSICS_SIM_SHEAR_TPS);
 coarse_steps=PHYSICS_SIM_SHEAR_COARSE_TPS/(2*PHYSICS_SIM_SHEAR_TPS);
#endif
 double dt=1./(2*PHYSICS_SIM_SHEAR_TPS)/fine_steps;
#ifdef PHYSICS_SIM_TAIL_DIFFUSION
 double matrix_dt=dt/2;
#else
 double matrix_dt=dt;
#endif
 #ifdef PHYSICS_SIM_REPAIRED_SCALARS
 if(!physics_sim_nonuniform_advection_init(&repaired_advection,nf,width,offset[48])||!physics_sim_nonuniform_diffusion_init(&repaired_diffusion,nf,width,offset[48]))return 0;
 for(int k=0;k<nz;k++)if(!physics_sim_local_matrix_init(&xm[k],nf,width,0,alpha,matrix_dt,true))return 0;
#else
 for(int k=0;k<nz;k++)if(!physics_sim_local_matrix_init(&xm[k],nf,width,.05*(k+.5)/nz,alpha,dt,true))return 0;
#endif
 double normal[PHYSICS_SIM_SHEAR_NORMAL_LIMIT];for(int k=0;k<nz;k++)normal[k]=dz;
 if(!physics_sim_local_matrix_init(&zf,nz,normal,0,alpha,matrix_dt,false)||!physics_sim_local_matrix_init(&zc,nz,normal,0,alpha,1./(2*PHYSICS_SIM_SHEAR_TPS)/coarse_steps,false))return 0;
#ifdef PHYSICS_SIM_TAIL_DIFFUSION
 if(!physics_sim_tail_diffusion_init(&tail_x,nf,width,true)||!physics_sim_tail_diffusion_init(&tail_z,nz,normal,false))return 0;
#endif
#ifdef PHYSICS_SIM_SHEAR_GAS_REPAIRED
 for(int j=0;j<nf;j++)patch_power_x[j]=center[j]>=1./32&&center[j]<1./16;
 for(int k=0;k<nz;k++)patch_power_z[k]=(k+.5)*dz>=1./64&&(k+.5)*dz<1./32;
 if(!physics_sim_source_patch_prepare(&source_patch,nf,nz,width,normal,patch_power_x,patch_power_z,alpha,dt,64)||
    !physics_sim_tail_heater_source(&tail_x,patch_power_x,alpha)||!physics_sim_tail_heater_source(&tail_z,patch_power_z,alpha))return 0;
#endif
#ifdef PHYSICS_SIM_GAS_PARALLEL_LINES
 if(!physics_sim_gas_lines_init(nf,nz,width,normal,patch_power_x,patch_power_z,alpha))return 0;
#endif
 accepted_n=candidate_n=nf;return 1;
}
/* Gas-only qualifier: explicit bypass, with no wall response or receiver call. */
int physics_sim_localized_gas_begin(void){
 if(!accepted||!candidate||accepted_n!=nf)return 0;
 candidate_n=accepted_n;memcpy(candidate,accepted,(size_t)accepted_n*nz*sizeof(double));return 1;
}
int physics_sim_localized_begin(const CfdOpenAtmosphere3d *old,CfdOpenAtmosphere3d *next,double dt,const PhysicsSimHeatedFace *initial_faces){
 candidate_n=accepted_n;memcpy(candidate,accepted,(size_t)accepted_n*nz*sizeof(double));
 double before=inventory(accepted,accepted_n),q=0;
 if(!response_ready){
  double saved[512],cg=1206*next->grid.volume;PhysicsSimHeatedFace unit[128];
  for(int j=0;j<512;j++){saved[j]=next->energy[j];next->energy[j]=cg;}
  for(int x=0;x<128;x++){unit[x]=initial_faces[x];unit[x].temperature_k=300;unit[x].temperature_correction_k=unit[x].gas_to_surface_j=0;}
  int ok=physics_sim_heated_columns_exchange(next,unit,128,dt);
  for(int x=0;x<128&&ok;x++){
   double value=0;for(int y=0;y<4;y++)value+=next->energy[y*128+x]/cg/4;
   mean_retention[x]=value;deviation_retention[x]=exp(-initial_faces[x].h_w_m2_k/(1206*dz)*dt);
   if(value<deviation_retention[x]||value>1)ok=0;
  }
  for(int j=0;j<512;j++)next->energy[j]=saved[j];
  if(!ok)return 0;response_ready=1;
 }
 for(int x=0;x<128;x++){
  double prior=0,after=0;for(int y=0;y<4;y++){prior+=old->energy[y*128+x]/(1206*old->grid.volume)/4;after+=next->energy[y*128+x]/(1206*old->grid.volume)/4;}
  q+=(prior-after)*1206*.25*dx*dz;
  if(candidate_n==128)candidate[x]=after;
  else {
   /* Preserve native aggregate transfer, while subcell deviations relax at
    * the physical gas-side rate. The common term is nonnegative for this
    * positive native pair exchange with nonnegative solid rise. */
   double attenuation=deviation_retention[x];
   double wall=(initial_faces[x].temperature_k-300)+initial_faces[x].temperature_correction_k;
   double common=(mean_retention[x]-attenuation)*prior+(1-mean_retention[x])*wall;
   if(!isfinite(common)||common<0)return 0;
   for(int j=offset[x];j<offset[x+1];j++)candidate[j]=attenuation*accepted[j]+common;
  }
 }
 double residual=fabs(before-inventory(candidate,candidate_n)-q);max_lift_balance=fmax(max_lift_balance,residual);
 return residual<1e-9;
}
int physics_sim_localized_step(CfdOpenAtmosphere3d *gas){
 int powered=gas->steps<PHYSICS_SIM_SHEAR_SOURCE_OFF;
 if(powered)last_source_tick=gas->steps+1;double added=0;
 for(int scalar=0;scalar<2;scalar++){
  int count=candidate_n==nf?fine_steps:coarse_steps;double dt=1./(2*PHYSICS_SIM_SHEAR_TPS)/count;
  for(int sub=0;sub<count;sub++){
   int n=candidate_n;
#ifdef PHYSICS_SIM_GAS_PARALLEL_LINES
   if(n!=nf||!physics_sim_gas_lines_step(candidate,n,nz,patch_power_x,patch_power_z,alpha,dt))return 0;
   if(powered)for(int k=0;k<nz;k++){
    double sz=fmax(0,fmin(1./32,(k+1)*dz)-fmax(1./64,k*dz))/dz;
    for(int j=0;j<n;j++){
     double w=width[j],c=center[j],sx=fmax(0,fmin(1./16,c+w/2)-fmax(1./32,c-w/2))/w;
     double increment=dt*sx*sz;added+=increment*1206*.25*w*dz;
    }
   }
#else
   for(int k=0;k<nz;k++){
    double source_z=fmax(0,fmin(1./32,(k+1)*dz)-fmax(1./64,k*dz))/dz;
    for(int j=0;j<n;j++)if(powered){
     double w=n==128?dx:width[j],c=n==128?(j+.5)*dx:center[j];
     double sx=fmax(0,fmin(1./16,c+w/2)-fmax(1./32,c-w/2))/w;
     double increment=dt*sx*source_z;
#if defined(PHYSICS_SIM_SHEAR_GAS_REPAIRED)
     /* Prepared joint-age source is added once after directional transport. */
#elif defined(PHYSICS_SIM_TAIL_DIFFUSION)
     candidate[k*n+j]+=increment/2;
#else
     candidate[k*n+j]+=increment;
#endif
     added+=increment*1206*.25*w*dz;
    }
    if(n==nf){
#ifdef PHYSICS_SIM_REPAIRED_SCALARS
     #ifdef PHYSICS_SIM_SHEAR_GAS_REPAIRED
     tail_x.heater_power=patch_power_z[k]?patch_power_x:NULL;
     int advection_method=3;
#else
     int advection_method=2;
#endif
     if(!physics_sim_nonuniform_advection_step(&repaired_advection,candidate+k*n,.05*(k+.5)/nz,dt,advection_method)){
      fprintf(stderr,"advection refused tick %d row %d sub %d n %d u %a dt %a\n",gas->steps,k,sub,n,.05*(k+.5)/nz,dt);
      for(int j=0;j<n;j++)fprintf(stderr,"%a%c",candidate[k*n+j],j==n-1?'\n':' ');return 0;
     }
#ifdef PHYSICS_SIM_TAIL_DIFFUSION
#ifdef PHYSICS_SIM_TAIL_TIME
     if(!physics_sim_tail_time_step(&tail_x,&time_x,candidate+k*n,alpha,dt,2))
#else
     if(!physics_sim_tail_diffusion_step(&tail_x,&xm[k],candidate+k*n,alpha,dt,2))
#endif
     {fprintf(stderr,"tail x refusal tick %d row %d sub %d\n",gas->steps,k,sub);return 0;}
#else
     if(!physics_sim_nonuniform_diffusion_step(&repaired_diffusion,&xm[k],candidate+k*n,alpha,dt))return 0;
#endif
#else
     if(!physics_sim_local_matrix_step(&xm[k],candidate+k*n))return 0;
#endif
#ifdef PHYSICS_SIM_INTERFACE_TRACE
     physics_sim_interface_flux(k,dt);
#endif
    }
    else {
     double stage[128],out[128],u=.05*(k+.5)/nz;
     if(!physics_sim_scalar_fct(candidate+k*n,stage,128,u,alpha,dx,dt,true)||!physics_sim_scalar_fct(stage,out,128,u,alpha,dx,dt,true))return 0;
     for(int j=0;j<128;j++)candidate[k*n+j]=.5*(candidate[k*n+j]+out[j]);
    }
   }
   for(int j=0;j<n;j++){
    for(int k=0;k<nz;k++)row[k]=candidate[k*n+j];
#ifdef PHYSICS_SIM_SHEAR_GAS_REPAIRED
    tail_z.heater_power=patch_power_x[j]?patch_power_z:NULL;
#endif
#ifdef PHYSICS_SIM_TAIL_DIFFUSION
    if(n==nf){
#ifdef PHYSICS_SIM_TAIL_TIME
     int ok=physics_sim_tail_time_step(&tail_z,&time_z,row,alpha,dt,2);
#else
     int ok=physics_sim_tail_diffusion_step(&tail_z,&zf,row,alpha,dt,2);
#endif
     if(!ok){fprintf(stderr,"tail z refusal tick %d column %d sub %d\n",gas->steps,j,sub);return 0;}
    }
    else if(!physics_sim_local_matrix_step(&zc,row))return 0;
#else
    if(!physics_sim_local_matrix_step(n==nf?&zf:&zc,row))return 0;
#endif
    for(int k=0;k<nz;k++){candidate[k*n+j]=row[k];minimum=fmin(minimum,row[k]);maximum=fmax(maximum,row[k]);if(row[k]<0||row[k]>.4+1e-10)return 0;}
   }
#endif
#if defined(PHYSICS_SIM_SHEAR_GAS_REPAIRED)
   if(powered&&!physics_sim_source_patch_add(&source_patch,candidate,dt))return 0;
   for(int q=0;q<n*nz;q++){minimum=fmin(minimum,candidate[q]);maximum=fmax(maximum,candidate[q]);}
#elif defined(PHYSICS_SIM_TAIL_DIFFUSION)
   if(powered)for(int k=0;k<nz;k++){
    double sz=fmax(0,fmin(1./32,(k+1)*dz)-fmax(1./64,k*dz))/dz;
    for(int j=0;j<n;j++){double w=n==128?dx:width[j],c=n==128?(j+.5)*dx:center[j];double sx=fmax(0,fmin(1./16,c+w/2)-fmax(1./32,c-w/2))/w;candidate[k*n+j]+=dt*sx*sz/2;if(!isfinite(candidate[k*n+j])||candidate[k*n+j]<0||candidate[k*n+j]>.4+1e-10)return 0;minimum=fmin(minimum,candidate[k*n+j]);maximum=fmax(maximum,candidate[k*n+j]);}
   }
#endif
   work_cells+=(uint64_t)n*nz;
  }
 }
 gas->steps++;gas->time=gas->steps*gas->dt;gas->input_j+=added;
 if(candidate_n==nf&&gas->steps>=PHYSICS_SIM_SHEAR_RESTRICT){
  double before=inventory(candidate,nf);
  /* Prefix compaction is safe: each destination precedes its unread source. */
  for(int k=0;k<nz;k++)for(int x=0;x<128;x++)candidate[k*128+x]=parent_mean(candidate,k,x,nf);
  candidate_n=128;restriction_tick=gas->steps;
  max_restriction=fmax(max_restriction,fabs(before-inventory(candidate,128)));
 }
 project(gas);
#ifdef PHYSICS_SIM_INTERFACE_TRACE
 interface_tick=gas->steps;
#endif
 return 1;
}
void physics_sim_localized_commit(void){double *swap=accepted;accepted=candidate;candidate=swap;accepted_n=candidate_n;
#ifdef PHYSICS_SIM_INTERFACE_TRACE
 physics_sim_interface_profile();
#endif
}
uint64_t physics_sim_localized_checksum(void){uint64_t hash=1469598103934665603ULL;
 const unsigned char *p=(const unsigned char *)accepted;for(size_t i=0;i<(size_t)accepted_n*nz*sizeof(double);i++)hash=(hash^p[i])*1099511628211ULL;return hash^(uint64_t)accepted_n;
}
void physics_sim_localized_fields(void){printf(",\"localized_fine_x\":%d,\"localized_internal_cells\":%d,\"fine_substeps\":%d,\"handoff_s\":%.17g,\"lift_balance_max_j\":%.17g,\"restriction_max_j\":%.17g,\"minimum_k\":%.17g,\"maximum_k\":%.17g,\"localized_work_cells\":%llu",nf,nf*nz,fine_steps,(double)PHYSICS_SIM_SHEAR_RESTRICT/PHYSICS_SIM_SHEAR_TPS,max_lift_balance,max_restriction,minimum,maximum,(unsigned long long)work_cells);
#ifdef PHYSICS_SIM_SHEAR_NORMAL256
 printf(",\"native_ticks_per_second\":%d,\"fine_scalar_dt_s\":%.17g,\"coarse_scalar_dt_s\":%.17g,\"configured_source_off_tick\":%d,\"last_source_tick\":%d,\"restriction_tick\":%d,\"observation_ticks\":%d",PHYSICS_SIM_SHEAR_TPS,1./PHYSICS_SIM_SHEAR_FINE_TPS,1./PHYSICS_SIM_SHEAR_COARSE_TPS,PHYSICS_SIM_SHEAR_SOURCE_OFF,last_source_tick,restriction_tick,PHYSICS_SIM_SHEAR_OBSERVE);
#endif
}
void physics_sim_localized_destroy(void){
#ifdef PHYSICS_SIM_SHEAR_GAS_REPAIRED
physics_sim_source_patch_destroy(&source_patch);
#endif
for(int k=0;k<nz;k++)physics_sim_local_matrix_destroy(&xm[k]);physics_sim_local_matrix_destroy(&zf);physics_sim_local_matrix_destroy(&zc);cfd_memory_free(width);cfd_memory_free(center);cfd_memory_free(accepted);cfd_memory_free(candidate);cfd_memory_free(row);}

/* Persistence seam: no numerical operation changes. The native gas projection
 * is checked against the fine/coarse accepted field before either is restored. */
#include "physics_sim_localized_state.h"
#include <stdlib.h>
int physics_sim_localized_capture(PhysicsSimLocalizedState *out){
 if(!out||!accepted||nz!=256)return 0;
 PhysicsSimLocalizedState s={.n=accepted_n,.nz=nz,.last_source_tick=last_source_tick,
  .restriction_tick=restriction_tick,.work_cells=work_cells,.max_lift_balance=max_lift_balance,
  .max_restriction=max_restriction,.minimum=minimum,.maximum=maximum,.count=(size_t)accepted_n*nz};
 s.values=malloc(s.count*sizeof(double));if(!s.values)return 0;memcpy(s.values,accepted,s.count*sizeof(double));*out=s;return 1;
}
int physics_sim_localized_state_valid(const PhysicsSimLocalizedState *s,const CfdOpenAtmosphere3d *gas){
 if(!s||!gas||!accepted||!s->values||s->nz!=nz||nz!=256||
  (s->n!=128&&s->n!=nf)||s->count!=(size_t)s->n*nz||
  s->n!=(gas->steps<PHYSICS_SIM_SHEAR_RESTRICT?nf:128)||
  s->restriction_tick!=(gas->steps<PHYSICS_SIM_SHEAR_RESTRICT?0:PHYSICS_SIM_SHEAR_RESTRICT)||
  s->last_source_tick<0||s->last_source_tick>PHYSICS_SIM_SHEAR_SOURCE_OFF||s->work_cells>UINT64_C(100000000000)||
  !isfinite(s->max_lift_balance)||s->max_lift_balance<0||s->max_lift_balance>1e-9||
  !isfinite(s->max_restriction)||s->max_restriction<0||s->max_restriction>1e-9||
  !isfinite(s->minimum)||s->minimum<0||!isfinite(s->maximum)||s->maximum>.4+1e-10||s->maximum<s->minimum)return 0;
 for(size_t i=0;i<s->count;i++)if(!isfinite(s->values[i])||s->values[i]<0||s->values[i]>.4+1e-10)return 0;
 for(int k=0;k<nz;k++)for(int x=0;x<128;x++){
  double energy=parent_mean(s->values,k,x,s->n)*1206*gas->grid.volume;
  for(int y=0;y<4;y++)if(gas->energy[(k*4+y)*128+x]!=energy)return 0;
 }
 return 1;
}
int physics_sim_localized_restore(const PhysicsSimLocalizedState *s,const CfdOpenAtmosphere3d *gas){
 if(!physics_sim_localized_state_valid(s,gas))return 0;
 memcpy(accepted,s->values,s->count*sizeof(double));memcpy(candidate,s->values,s->count*sizeof(double));accepted_n=candidate_n=s->n;
 last_source_tick=s->last_source_tick;restriction_tick=s->restriction_tick;work_cells=s->work_cells;
 max_lift_balance=s->max_lift_balance;max_restriction=s->max_restriction;minimum=s->minimum;maximum=s->maximum;
 /* The affine unit-wall response depends only on immutable geometry/capacity.
  * Recompute it on the first new stage from the same unit experiment. */
 response_ready=0;return 1;
}
void physics_sim_localized_state_free(PhysicsSimLocalizedState *s){if(s){free(s->values);memset(s,0,sizeof *s);}}
