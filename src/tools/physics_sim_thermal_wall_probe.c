/* Impermeable planar wall + prescribed tangential shear + native receiver.
 * Authored contact resistance; no resolved convection or combustion claim. */
#include "receiver.h"
#include "inert.h"
#include "physics_sim_heated_face.h"
#include "app/cfd_passive3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static double total(const CfdOpenAtmosphere3d *s){return cfd_passive3d_total(s->energy,s->grid.count);}
static double slab(const CfdOpenAtmosphere3d *s,double lo,double hi){
 double sum=0;
 for(int k=0;k<s->grid.n[2];k++){for(int j=0;j<s->plane;j++){
  double overlap=fmax(0,fmin(hi,(k+1)*s->grid.h[2])-fmax(lo,k*s->grid.h[2]));
  sum+=s->energy[k*s->plane+j]*overlap/s->grid.h[2];
 }}
 return sum/(1206*.0625*(hi-lo));
}
static int init(CfdOpenAtmosphere3d *s,int nz){
 int n[3]={4,4,nz};double l[3]={.25,.25,.0625},z[2]={0,0},t[2]={300,300};
 if(!cfd_open_atmosphere3d_init(s,n,l,1.2,1.8e-5,1005,300,.026,0,1./1600,z,t,z,false,0,0,.1))return 0;
 s->ground=true;
 s->scalar_rk2=s->scalar_mc=s->muscl=s->cache_projection=true;s->max_scalar_work_cells=200000000;
 for(int i=0;i<s->grid.count;i++)s->velocity[i]=.05*(i/s->plane+.5)/s->grid.n[2];
 return 1;
}
static double gas_balance(const CfdOpenAtmosphere3d *s,double q){
 return total(s)-s->initial_j-s->input_j-cfd_passive3d_total(s->flux,s->boundary_faces)+cfd_passive3d_total(s->flux+s->boundary_faces,s->boundary_faces)+q;
}
static int stage(const CfdOpenAtmosphere3d *gas,CfdOpenAtmosphere3d *work,
 const ThermalReceiver *r,ThermalReceiver *out,const double *source,double h,int fail){
 ThermalReceiverObservation o;if(!thermal_receiver_observe(r,&o))return 0;
 if(gas==work||gas->storage==work->storage||out==r||gas->steps!=(int)r->solid.completed_steps)return 0;
 thermal_inert_copy_gas(work,gas);
 double effective_h=h==0?0:1/(gas->grid.h[2]/(2*.026)+1/h+.005/(2*.16));
 PhysicsSimHeatedFace f={2,-1,o.temperature_k[0],o.capacity_j_k[0],effective_h,0,
  o.temperature_rise_k[0]-(o.temperature_k[0]-300)};
 double dt=(double)r->config.window_steps/1600;
 if(!physics_sim_heated_slab_exchange(work,&f,dt,0,gas->grid.h[2]))return 0;
 if(fabs(total(gas)-total(work)-f.gas_to_surface_j)>1e-9)return 0;
 CoreSimTimeInterval interval={r->solid.time,{r->config.timebase,r->solid.time.ticks+r->config.window_steps}};
 ThermalReceiverExchange packet;ThermalReceiver next;ThermalReceiverReceipt receipt;
 if(!thermal_receiver_exchange_make(r,interval,gas->steps+1,f.gas_to_surface_j,&packet))return 0;
 if(fail==1)packet.q_j+=1; /* tampered packet must not publish gas debit */
 if(!thermal_receiver_stage(r,&packet,interval.end,&next,&receipt))return 0;
 if(fail==2)return 0; /* failure after solid staging */
 for(unsigned i=0;i<r->config.window_steps;i++)if(!cfd_open_atmosphere3d_step(work,source,source+work->grid.count))return 0;
 if(work->steps!=(int)next.solid.time.ticks||fabs(work->time-(double)next.solid.time.ticks/1600)>1e-12||
    fabs(gas_balance(work,next.received_q_j))>1e-8||fabs(receipt.accepted_candidate.balance_residual_j)>1e-9)return 0;
 *out=next;return 1;
}
static void observe(const CfdOpenAtmosphere3d *s,const ThermalReceiver *r){
 ThermalReceiverObservation o;assert(thermal_receiver_observe(r,&o));
 printf("{\"tick\":%llu,\"near_k\":%.17g,\"tail_k\":%.17g,\"solid0_k\":%.17g,\"solid1_k\":%.17g,\"q_j\":%.17g,\"gas_j\":%.17g,\"source_j\":%.17g,\"gas_balance_j\":%.17g,\"solid_balance_j\":%.17g}\n",(unsigned long long)o.time.ticks,slab(s,0,1./128),slab(s,1./128,1./64),o.temperature_rise_k[0],o.temperature_rise_k[1],r->received_q_j,total(s),s->input_j,gas_balance(s,r->received_q_j),o.balance_residual_j);
}
int main(int argc,char **argv){
 setvbuf(stdout,NULL,_IOLBF,0);if(argc!=5)return 2;
 int nz=atoi(argv[1]),window=atoi(argv[2]),steps=atoi(argv[3]);double h=atof(argv[4]);
 if((nz!=64&&nz!=128&&nz!=256)||(window!=10&&window!=5)||(steps!=80&&steps!=5120)||(h!=0&&h!=10))return 2;
 CfdMemoryBudget budget={.limit_bytes=32*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 CfdOpenAtmosphere3d gas={0},work={0};assert(init(&gas,nz)&&init(&work,nz));
 ThermalReceiverConfig c={0};c.version=c.quantity=c.unit=c.sign=1;c.cells=2;c.area_m2=.0625;c.reference_k=300;c.max_k=500;
 strcpy(c.fixture_sha256,COUPLED_FIXTURE_ID);strcpy(c.producer_sha256,COUPLED_SOURCE_ID);strcpy(c.support_sha256,COUPLED_SUPPORT_ID);
 c.face_hi_m[0]=c.face_hi_m[1]=.25;c.face_lo_m[2]=c.face_hi_m[2]=0;
 c.timebase=(CoreSimTimebase){716,1600};c.total_steps=steps;c.window_steps=window;
 for(int i=0;i<2;i++){c.width_m[i]=.005;c.cv_j_m3_k[i]=160000;c.conductivity_w_m_k[i]=.16;c.initial_k[i]=300;}
 ThermalReceiver r;assert(thermal_receiver_init(&c,&r));double *source=cfd_memory_calloc((size_t)2*gas.grid.count,sizeof(double));assert(source);
 for(int k=0;k<nz;k++)for(int j=0;j<gas.plane;j++)source[k*gas.plane+j]=1206*gas.grid.volume*gas.dt*fmax(0,fmin(1./32,(k+1)*gas.grid.h[2])-fmax(1./64,k*gas.grid.h[2]))/gas.grid.h[2];
 observe(&gas,&r);unsigned refusals=0;
 while(gas.steps<steps){
  /* Test failures again after the gas carries nonzero heat, not only at t=0. */
  if(gas.steps==0||gas.steps==40){
   ThermalReceiver saved=r,out;memset(&out,0xa5,sizeof out);ThermalReceiver sentinel=out;
   double *energy=malloc((size_t)gas.grid.count*sizeof(double));assert(energy);memcpy(energy,gas.energy,(size_t)gas.grid.count*sizeof(double));
   for(int fail=1;fail<=2;fail++){assert(!stage(&gas,&work,&r,&out,source,h,fail));assert(!memcmp(&r,&saved,sizeof r)&&!memcmp(&out,&sentinel,sizeof out)&&!memcmp(energy,gas.energy,(size_t)gas.grid.count*sizeof(double)));refusals++;}
   free(energy);
  }
  ThermalReceiver next;if(!stage(&gas,&work,&r,&next,source,h,0)){fprintf(stderr,"joint refusal at tick %d\n",gas.steps);return 3;}
  r=next;CfdOpenAtmosphere3d swap=gas;gas=work;work=swap;
  if(!(gas.steps%40))observe(&gas,&r);
 }
 double velocity=0;for(int a=0;a<3;a++)for(int j=0;j<gas.grid.count+(a==2?gas.plane:0);j++)velocity=fmax(velocity,fabs(gas.velocity[a*gas.grid.count+j]-(a==0?.05*(j/gas.plane+.5)/gas.grid.n[2]:0)));
 double boundary=0;for(int i=0;i<4*gas.boundary_faces;i++)boundary+=fabs(gas.flux[i]);
 assert(boundary==0);assert(gas.scalar_work_cells==(unsigned long long)steps*gas.grid.count*4);
 double balance=gas_balance(&gas,r.received_q_j);size_t peak=budget.peak_bytes;
 cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&gas);cfd_open_atmosphere3d_destroy(&work);cfd_memory_scope(prior);
 printf("{\"terminal\":true,\"n\":%d,\"window_ticks\":%d,\"steps\":%d,\"h\":%.17g,\"gas_balance_j\":%.17g,\"velocity_error\":%.17g,\"refusal_checks\":%u,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu}\n",nz,window,steps,h,balance,velocity,refusals,peak,budget.live_bytes);
 return budget.live_bytes||velocity>1e-12||fabs(balance)>1e-8;
}
