/* Periodic-x mean of the prescribed-shear qualifier. Native wall/receiver;
 * retained normal BE/source order and late scalar-time transition. No feedback. */
#include "receiver.h"
#include "inert.h"
#include "physics_sim_heated_face.h"
#include "physics_sim_local_matrix.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static double v[1024],width[1024];
static double inventory(int n){double e=0;for(int k=0;k<n;k++)e+=v[k]*1206*.0625*.0625/n;return e;}
static void observation(int tick,int n,const ThermalReceiver *r,double source){
 double q=0,e[2]={0};for(int x=0;x<4;x++){q+=r[x].received_q_j;for(int d=0;d<2;d++)e[d]+=r[x].solid.native.energy_j[d];}
 double near=0;for(int k=0;k<n/8;k++)near+=v[k]/(n/8);
 printf("{\"tick\":%d,\"time_s\":%.17g,\"q_j\":%.17g,\"gas_wall_k\":%.17g,\"near_k\":%.17g,\"solid0_j\":%.17g,\"solid1_j\":%.17g,\"source_j\":%.17g,\"gas_balance_j\":%.17g,\"solid_balance_j\":%.17g}\n",tick,tick/51200.,q,v[0],near,e[0],e[1],source,inventory(n)+q-source,e[0]+e[1]-q);
}
int main(int argc,char **argv){
 if(argc!=7)return 2;
 int n=atoi(argv[1]),scalar=atoi(argv[2]),window=atoi(argv[3]),tps=atoi(argv[4]),steps=atoi(argv[5]);
 if((n!=128&&n!=256&&n!=512)||(scalar!=1&&scalar!=2)||(window!=160&&window!=80&&window!=40)||(tps!=1600&&tps!=3200&&tps!=6400)||(steps!=2560&&steps!=25600))return 2;
 assert(51200%tps==0&&window%(51200/tps)==0&&steps%window==0);
 FILE *trace=fopen(argv[6],"wx");assert(trace);setvbuf(stdout,NULL,_IOLBF,0);
 CfdMemoryBudget budget={.limit_bytes=16*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 CfdOpenAtmosphere3d gas={0};int cells[3]={4,4,n};double lengths[3]={.25,.25,.0625},zero[2]={0,0},temp[2]={300,300};
 assert(cfd_open_atmosphere3d_init(&gas,cells,lengths,1.2,1.8e-5,1005,300,.026,0,1./tps,zero,temp,zero,false,0,0,.1));gas.ground=true;
 const double dz=.0625/n,alpha=.026/1206,heff=1/(dz/(2*.026)+.1+.005/(2*.16)),cg=1206*gas.grid.volume;
 for(int k=0;k<n;k++)width[k]=dz;
 PhysicsSimLocalMatrix fine={0},coarse={0};
 assert(physics_sim_local_matrix_init(&fine,n,width,0,alpha,scalar/51200.,false));
 assert(physics_sim_local_matrix_init(&coarse,n,width,0,alpha,4*scalar/51200.,false));
 ThermalReceiverConfig c={0};c.version=c.quantity=c.unit=c.sign=1;c.cells=2;c.reference_k=300;c.max_k=500;c.area_m2=.0625/4;
 strcpy(c.fixture_sha256,COUPLED_FIXTURE_ID);strcpy(c.producer_sha256,COUPLED_SOURCE_ID);strcpy(c.support_sha256,COUPLED_SUPPORT_ID);
 c.face_hi_m[1]=.25;c.timebase=(CoreSimTimebase){716,(uint64_t)tps};c.total_steps=(uint64_t)steps/(51200/tps);c.window_steps=window/(51200/tps);
 for(int d=0;d<2;d++){c.width_m[d]=.005;c.cv_j_m3_k[d]=160000;c.conductivity_w_m_k[d]=.16;c.initial_k[d]=300;}
 ThermalReceiver r[4],next[4];for(int x=0;x<4;x++){c.face_lo_m[0]=x*.25/4;c.face_hi_m[0]=(x+1)*.25/4;assert(thermal_receiver_init(&c,&r[x]));}
 double source=0,worst=0,minimum=0,maximum=0;unsigned updates=0;
 observation(0,n,r,source);
 for(int tick=0;tick<steps;tick+=window){
  for(int k=0;k<n;k++)for(int j=0;j<16;j++)gas.energy[k*16+j]=v[k]*cg;
  PhysicsSimHeatedFace f[4];for(int x=0;x<4;x++){
   ThermalReceiverObservation o;assert(thermal_receiver_observe(&r[x],&o));
   f[x]=(PhysicsSimHeatedFace){2,-1,o.temperature_k[0],o.capacity_j_k[0],heff,0,o.temperature_rise_k[0]-(o.temperature_k[0]-300)};
  }
  assert(physics_sim_heated_columns_exchange(&gas,f,4,window/51200.));
  double after=0;for(int j=0;j<16;j++)after+=gas.energy[j]/cg/16;v[0]=after;
  double amounts[100]={0},debit=0,energies[2]={0};
  for(int x=0;x<4;x++){
   CoreSimTimeInterval interval={r[x].solid.time,{r[x].config.timebase,r[x].solid.time.ticks+r[x].config.window_steps}};
   ThermalReceiverExchange packet;ThermalReceiverReceipt receipt;
   assert(thermal_receiver_exchange_make(&r[x],interval,tick+1,f[x].gas_to_surface_j,&packet));
   assert(thermal_receiver_stage(&r[x],&packet,interval.end,&next[x],&receipt));
   double local[100];assert(thermal_inert_rebin(&r[x].plan,r[x].solid.completed_steps,r[x].config.window_steps,f[x].gas_to_surface_j,local));
   for(unsigned j=0;j<c.window_steps;j++)amounts[j]+=local[j];debit+=f[x].gas_to_surface_j;
   for(int d=0;d<2;d++)energies[d]+=next[x].solid.native.energy_j[d];
  }
  fprintf(trace,"%d %d %a %a %a",tick+window,(int)c.window_steps,debit,energies[0],energies[1]);
  for(unsigned j=0;j<c.window_steps;j++)fprintf(trace," %a",amounts[j]);fputc('\n',trace);
  memcpy(r,next,sizeof r);
  for(int now=tick;now<tick+window;){
   int stride=now<25024?scalar:4*scalar;assert(now+stride<=tick+window&&(now>=25024||now+stride<=25024));double dt=stride/51200.;
   if(now<20480)for(int k=n/4;k<n/2;k++){double increment=dt/8;v[k]+=increment;source+=increment*1206*.0625*dz;}
   assert(physics_sim_local_matrix_step(now<25024?&fine:&coarse,v));
   for(int k=0;k<n;k++){assert(isfinite(v[k])&&v[k]>=0&&v[k]<=.05+1e-10);minimum=fmin(minimum,v[k]);maximum=fmax(maximum,v[k]);}
   now+=stride;updates++;
  }
  double q=0;for(int x=0;x<4;x++)q+=r[x].received_q_j;
  worst=fmax(worst,fabs(inventory(n)+q-source));assert(worst<1e-10);
  if((tick+window)%1280==0)observation(tick+window,n,r,source);
 }
 physics_sim_local_matrix_destroy(&fine);physics_sim_local_matrix_destroy(&coarse);cfd_open_atmosphere3d_destroy(&gas);cfd_memory_scope(prior);assert(!budget.live_bytes);assert(fclose(trace)==0);
 printf("{\"terminal\":true,\"normal_cells\":%d,\"scalar_ticks\":%d,\"window_ticks\":%d,\"receiver_tps\":%d,\"steps\":%d,\"normal_updates\":%u,\"minimum_k\":%.17g,\"maximum_k\":%.17g,\"worst_balance_j\":%.17g,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu}\n",n,scalar,window,tps,steps,updates,minimum,maximum,worst,budget.peak_bytes,budget.live_bytes);
 return 0;
}
