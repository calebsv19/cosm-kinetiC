/* Finite along-wall source pulse + prescribed shear + native receiver.
 * Authored contact resistance; no resolved convection or combustion claim. */
#include "receiver.h"
#include "physics_sim_shear_clock.h"
#ifdef PHYSICS_SIM_SHEAR_DURABLE
#include "physics_sim_shear_checkpoint.h"
#endif
#ifdef PHYSICS_SIM_SHEAR_NORMAL256
#include "physics_sim_shear_receiver_fixture.h"
#endif
#if defined(PHYSICS_SIM_LIVE_RECEIVER)
#include "physics_sim_receiver_client.h"
#include <signal.h>
#include "physics_sim_shear_receiver_fixture.h"
static PhysicsSimReceiverClient receiver_client;
static const char *receiver_path;
static char receiver_profile[32];
#endif
#ifdef PHYSICS_SIM_LOCALIZED_COLUMNS
#include "physics_sim_localized_columns.h"
#endif
#include "inert.h"
#include "physics_sim_heated_face.h"
#include "app/cfd_passive3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef PHYSICS_SIM_PATCH6_TRACE
#include "physics_sim_patch6_trace.h"
#endif
#ifdef PHYSICS_SIM_COLUMNS_SOLID_TRACE
#include "physics_sim_columns_solid_trace.h"
#endif
static double total(const CfdOpenAtmosphere3d *s){return cfd_passive3d_total(s->energy,s->grid.count);}
static double replay_difference[128];
static double band(const CfdOpenAtmosphere3d *s,double xl,double xh,double zl,double zh){
 double sum=0;
 for(int k=0;k<s->grid.n[2];k++)for(int j=0;j<s->grid.n[1];j++)for(int i=0;i<s->grid.n[0];i++){
  double ox=fmax(0,fmin(xh,(i+1)*s->grid.h[0])-fmax(xl,i*s->grid.h[0]));
  double oz=fmax(0,fmin(zh,(k+1)*s->grid.h[2])-fmax(zl,k*s->grid.h[2]));
  sum+=s->energy[k*s->plane+j*s->grid.n[0]+i]*ox/s->grid.h[0]*oz/s->grid.h[2];
 }
 return sum/(1206*.25*(xh-xl)*(zh-zl));
}
static int init(CfdOpenAtmosphere3d *s,int nx,int nz){
 int n[3]={nx,4,nz};double l[3]={.25,.25,.0625},z[2]={0,0},t[2]={300,300};
 if(!cfd_open_atmosphere3d_init(s,n,l,1.2,1.8e-5,1005,300,.026,0,1./PHYSICS_SIM_SHEAR_TPS,z,t,z,false,0,0,.1))return 0;
 s->ground=true;
 s->scalar_rk2=s->scalar_mc=s->muscl=s->cache_projection=true;s->max_scalar_work_cells=2000000000ULL;
 for(int i=0;i<s->grid.count;i++)s->velocity[i]=.05*(i/s->plane+.5)/s->grid.n[2];
 return 1;
}
static double gas_balance(const CfdOpenAtmosphere3d *s,double q){
 return total(s)-s->initial_j-s->input_j-cfd_passive3d_total(s->flux,s->boundary_faces)+cfd_passive3d_total(s->flux+s->boundary_faces,s->boundary_faces)+q;
}
static double received(const ThermalReceiver *r,int nx){double q=0;for(int i=0;i<nx;i++)q+=r[i].received_q_j;return q;}
static int stage(const CfdOpenAtmosphere3d *gas,CfdOpenAtmosphere3d *work,
 const ThermalReceiver *r,ThermalReceiver *out,const double *source,double h,int fail){
 int nx=gas->grid.n[0];
 if(gas==work||gas->storage==work->storage||out==r)return 0;
 thermal_inert_copy_gas(work,gas);
 ThermalReceiver *next=cfd_memory_calloc((size_t)nx,sizeof(*next));
 PhysicsSimHeatedFace *f=cfd_memory_calloc((size_t)nx,sizeof(*f));
 if(!next||!f){cfd_memory_free(next);cfd_memory_free(f);return 0;}
 const char *phase="receiver-observe";int column=-1;
 int ok=0;double dt=(double)r[0].config.window_steps/PHYSICS_SIM_SHEAR_TPS;
 for(int i=0;i<nx;i++){
  ThermalReceiverObservation o;
  if(!thermal_receiver_observe(&r[i],&o)||gas->steps!=(int)r[i].solid.completed_steps)goto done;
  f[i]=(PhysicsSimHeatedFace){2,-1,o.temperature_k[0],o.capacity_j_k[0],h==0?0:1/(gas->grid.h[2]/(2*.026)+1/h+.005/(2*.16)),0,o.temperature_rise_k[0]-(o.temperature_k[0]-300)};
 }
 #ifdef PHYSICS_SIM_LOCALIZED_COLUMNS
 PhysicsSimHeatedFace initial_faces[128];memcpy(initial_faces,f,(size_t)nx*sizeof(*f));
#endif
 phase="wall-exchange";
 if(!physics_sim_heated_columns_exchange(work,f,nx,dt))goto done;
#ifdef PHYSICS_SIM_PATCH6_TRACE
 double patch6_debit[16]={0};
 for(int x=96;x<112;x++)for(int y=0;y<4;y++)patch6_debit[x-96]+=gas->energy[y*nx+x]-work->energy[y*nx+x];
#endif
#ifdef PHYSICS_SIM_COLUMNS_SOLID_TRACE
 double solid_debit[128]={0};
 for(int x=0;x<nx;x++)for(int y=0;y<4;y++)solid_debit[x]+=gas->energy[y*nx+x]-work->energy[y*nx+x];
#endif
 double q=0;for(int i=0;i<nx;i++)q+=f[i].gas_to_surface_j;
 if(fabs(total(gas)-total(work)-q)>1e-9)goto done;
 phase="receiver-stage";
 for(int i=0;i<nx;i++){
  column=i;CoreSimTimeInterval interval={r[i].solid.time,{r[i].config.timebase,r[i].solid.time.ticks+r[i].config.window_steps}};
  ThermalReceiverExchange packet;ThermalReceiverReceipt receipt;
  /* Late-column refusal tests exercise partially staged candidate batches. */
  const ThermalReceiver *producer=&r[(fail==2&&i==nx-1)?0:i];
  if(!thermal_receiver_exchange_make(producer,interval,gas->steps+1,f[i].gas_to_surface_j,&packet))goto done;
  if(fail==1&&i==nx-1)packet.q_j+=1;
#ifdef PHYSICS_SIM_LIVE_RECEIVER
  if(fail==5&&i==nx/2)assert(kill(receiver_client.pid,SIGKILL)==0);
#endif
#ifdef PHYSICS_SIM_LIVE_RECEIVER
  int accepted=receiver_path?physics_sim_receiver_client_stage(&receiver_client,i,&r[i],&packet,interval.end,&next[i],&receipt):thermal_receiver_stage(&r[i],&packet,interval.end,&next[i],&receipt);
#else
  int accepted=thermal_receiver_stage(&r[i],&packet,interval.end,&next[i],&receipt);
#endif
  if(!accepted||fabs(receipt.accepted_candidate.balance_residual_j)>1e-9){if(!fail)fprintf(stderr,"receiver refused tick %d column %d q %a e0 %a e1 %a\n",gas->steps,i,packet.q_j,r[i].solid.native.energy_j[0],r[i].solid.native.energy_j[1]);goto done;}
 }
 if(fail==3)goto done;
 #ifdef PHYSICS_SIM_LOCALIZED_COLUMNS
 (void)source;
 phase="wall-lift";if(!physics_sim_localized_begin(gas,work,dt,initial_faces))goto done;
 phase="gas-transport";
 for(unsigned j=0;j<r[0].config.window_steps;j++)if(!physics_sim_localized_step(work))goto done;
#else
 for(unsigned j=0;j<r[0].config.window_steps;j++)if(!cfd_open_atmosphere3d_step(work,source+(work->steps>=PHYSICS_SIM_SHEAR_SOURCE_OFF?2*work->grid.count:0),source+work->grid.count))goto done;
#endif
 if(fail==4)goto done;
 phase="post-transport-balance";if(fabs(gas_balance(work,received(next,nx)))>1e-8)goto done;
 for(int i=0;i<nx;i++)if(work->steps!=(int)next[i].solid.time.ticks)goto done;
 #ifdef PHYSICS_SIM_LOCALIZED_COLUMNS
 physics_sim_localized_commit();
#endif
#ifdef PHYSICS_SIM_PATCH6_TRACE
 patch6_commit(r,next,f,patch6_debit);
#endif
#ifdef PHYSICS_SIM_COLUMNS_SOLID_TRACE
 columns_solid_commit(r,next,f,solid_debit,nx);
#endif
 memcpy(out,next,(size_t)nx*sizeof(*out));ok=1;
 done:if(!ok&&!fail)fprintf(stderr,"joint stage refused tick %d work_tick %d phase %s column %d\n",gas->steps,work->steps,phase,column);cfd_memory_free(next);cfd_memory_free(f);return ok;
}
static void observe(const CfdOpenAtmosphere3d *s,const ThermalReceiver *r){
 int nx=s->grid.n[0];double q=received(r,nx),balance=0;
 double bins[4][8]={{0}},replay[2][8]={{0}},peak=0;int peak_i=0;
 for(int i=0;i<nx;i++){
  ThermalReceiverObservation o;assert(thermal_receiver_observe(&r[i],&o));
  balance+=o.balance_residual_j;int b=i*8/nx;
  for(int d=0;d<2;d++)bins[d][b]+=o.temperature_rise_k[d]*8/nx;
  bins[2][b]+=r[i].received_q_j;
  replay[0][b]+=(r[i].received_q_j*nx/100+replay_difference[i]/2)*8/nx;
  replay[1][b]+=(r[i].received_q_j*nx/100-replay_difference[i]/2)*8/nx;
  if(o.temperature_rise_k[0]>peak){peak=o.temperature_rise_k[0];peak_i=i;}
 }
 printf("{\"tick\":%d,\"transport_k\":%.17g,\"near_k\":%.17g,\"tail_k\":%.17g,\"q_j\":%.17g,\"gas_balance_j\":%.17g,\"solid_balance_j\":%.17g,\"source_j\":%.17g,\"peak_k\":%.17g,\"peak_x_m\":%.17g",s->steps,band(s,1./16,3./32,1./64,1./32),band(s,1./16,3./32,0,1./128),band(s,3./32,1./8,1./64,1./32),q,gas_balance(s,q),balance,s->input_j,peak,(peak_i+.5)*s->grid.h[0]);
 for(int d=0;d<3;d++){printf(",\"%s\":[",d==0?"solid0_k":d==1?"solid1_k":"local_q_j");for(int b=0;b<8;b++)printf("%s%.17g",b?",":"",bins[d][b]);printf("]");}
 for(int d=0;d<2;d++){printf(",\"replay%d_k\":[",d);for(int b=0;b<8;b++)printf("%s%.17g",b?",":"",replay[d][b]);printf("]");}
 printf("}\n");
}
int main(int argc,char **argv){
 setvbuf(stdout,NULL,_IOLBF,0);
#if defined(PHYSICS_SIM_PATCH6_TRACE) || defined(PHYSICS_SIM_COLUMNS_SOLID_TRACE)
#ifdef PHYSICS_SIM_LIVE_RECEIVER
#ifdef PHYSICS_SIM_SHEAR_DURABLE
 int durable=argc==12||argc==13;
 if(!durable&&argc!=7&&argc!=8&&argc!=9)return 2;
 if(durable&&(strcmp(argv[8],"new")&&strcmp(argv[8],"resume")))return 2;
 int end_tick=durable?atoi(argv[10]):0;int publish_fail=durable&&argc==13?atoi(argv[12]):0;
 if(durable&&(end_tick<5||end_tick>3200||end_tick%5||publish_fail<0||publish_fail>6))return 2;
#else
 if(argc!=7&&argc!=8&&argc!=9)return 2;
#endif
 if(argc==9&&strcmp(argv[8],"restart"))return 2;
#else
 if(argc!=7)return 2;
#endif
#else
 if(argc!=6)return 2;
#endif
 int nx=atoi(argv[1]),nz=atoi(argv[2]),window=atoi(argv[3]),steps=atoi(argv[4]);double h=atof(argv[5]);
 if((nx!=32&&nx!=64&&nx!=128)||(nz!=64&&nz!=128&&nz!=PHYSICS_SIM_SHEAR_NORMAL_LIMIT)||(window!=10&&window!=5)||(steps!=PHYSICS_SIM_SHEAR_OBSERVE&&steps!=PHYSICS_SIM_SHEAR_SMOKE&&steps!=PHYSICS_SIM_SHEAR_EARLY&&steps!=PHYSICS_SIM_SHEAR_SHORT&&steps!=PHYSICS_SIM_SHEAR_LONG)||(h!=0&&h!=10))return 2;
#ifdef PHYSICS_SIM_SHEAR_NORMAL256
 if(nx!=128||nz!=256||window!=5||steps>PHYSICS_SIM_SHEAR_SHORT)return 2;
#endif
#ifdef PHYSICS_SIM_TAIL_DIFFUSION
 if(steps>PHYSICS_SIM_SHEAR_EARLY)return 2;
#endif
 CfdMemoryBudget budget={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 CfdOpenAtmosphere3d gas={0},work={0};assert(init(&gas,nx,nz)&&init(&work,nx,nz));
 #ifdef PHYSICS_SIM_LOCALIZED_COLUMNS
 assert(nx==128&&physics_sim_localized_init(&gas));
#endif
 ThermalReceiverConfig c={0};c.version=c.quantity=c.unit=c.sign=1;c.cells=2;c.area_m2=.0625;c.reference_k=300;c.max_k=500;
 strcpy(c.fixture_sha256,COUPLED_FIXTURE_ID);strcpy(c.producer_sha256,COUPLED_SOURCE_ID);strcpy(c.support_sha256,COUPLED_SUPPORT_ID);
 c.face_hi_m[0]=c.face_hi_m[1]=.25;c.face_lo_m[2]=c.face_hi_m[2]=0;
 c.timebase=(CoreSimTimebase){716,PHYSICS_SIM_SHEAR_TPS};c.total_steps=steps;c.window_steps=window;
 for(int i=0;i<2;i++){c.width_m[i]=.005;c.cv_j_m3_k[i]=160000;c.conductivity_w_m_k[i]=.16;c.initial_k[i]=300;}
 ThermalReceiver *r=cfd_memory_calloc((size_t)nx,sizeof(*r));assert(r);
 for(int x=0;x<nx;x++){
  c.face_lo_m[0]=x*.25/nx;c.face_hi_m[0]=(x+1)*.25/nx;c.area_m2=.0625/nx;
#ifdef PHYSICS_SIM_SHEAR_NORMAL256
  assert(physics_sim_shear_receiver_init(x,steps,&r[x]));
#else
  assert(thermal_receiver_init(&c,&r[x]));
#endif
 }double *source=cfd_memory_calloc((size_t)3*gas.grid.count,sizeof(double));assert(source);
 for(int k=0;k<nz;k++)for(int j=0;j<gas.plane;j++)source[k*gas.plane+j]=1206*gas.grid.volume*gas.dt*fmax(0,fmin(1./32,(k+1)*gas.grid.h[2])-fmax(1./64,k*gas.grid.h[2]))/gas.grid.h[2]*fmax(0,fmin(1./16,(j%nx+1)*gas.grid.h[0])-fmax(1./32,(j%nx)*gas.grid.h[0]))/gas.grid.h[0];
#ifdef PHYSICS_SIM_LIVE_RECEIVER
 uint64_t receiver_stages=0,receiver_requests=0;unsigned receiver_restarts=0;
#endif
 unsigned refusals=0;
#ifdef PHYSICS_SIM_SHEAR_DURABLE
 PhysicsSimShearView checkpoint={.gas=&gas,.receivers=r,.replay=replay_difference};uint64_t checkpoint_digest=0;int begin_tick=0;
 if(durable){
  if(steps!=3200||h!=10)return 2;
  if(!strcmp(argv[8],"new")){if(physics_sim_shear_save(argv[9],&checkpoint,argv[11],0,0,0,0,&checkpoint_digest)!=SHEAR_CP_OK)return 4;}
  else if(!physics_sim_shear_load(argv[9],&checkpoint,argv[11],&checkpoint_digest))return 4;
  begin_tick=gas.steps;refusals=checkpoint.refusals;
  if(checkpoint.pending_end&&checkpoint.pending_end!=end_tick)return 4;
  if(end_tick<gas.steps)return 4;
  if(end_tick==gas.steps){fprintf(stderr,"durable: reused committed tick %d digest %016llx\n",gas.steps,(unsigned long long)checkpoint_digest);physics_sim_localized_destroy();cfd_memory_free(r);cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&gas);cfd_open_atmosphere3d_destroy(&work);cfd_memory_scope(prior);return 0;}
 }
#endif
#ifdef PHYSICS_SIM_LIVE_RECEIVER
 if(argc>=8){receiver_path=argv[7];snprintf(receiver_profile,sizeof receiver_profile,"shear256-%d",steps);assert(physics_sim_receiver_client_open_profile(&receiver_client,receiver_path,receiver_profile));}
#endif
#ifdef PHYSICS_SIM_PATCH6_TRACE
 patch6_open(argv[6],nx,nz,window,steps);
#endif
#ifdef PHYSICS_SIM_COLUMNS_SOLID_TRACE
 columns_solid_open(argv[6],nx,nz,window,steps);
#endif
 if(!gas.steps)observe(&gas,r);
 while(gas.steps<
#ifdef PHYSICS_SIM_SHEAR_DURABLE
 (durable?end_tick:steps)
#else
 steps
#endif
 ){
#ifdef PHYSICS_SIM_SHEAR_DURABLE
 int phase=gas.steps==0?1:gas.steps==PHYSICS_SIM_SHEAR_OBSERVE?2:0;
#endif
#ifdef PHYSICS_SIM_LIVE_RECEIVER
  if(receiver_path&&argc==9&&(gas.steps==2560||gas.steps==3125||gas.steps==3130)){
   receiver_stages+=receiver_client.stages;receiver_requests+=receiver_client.transactions;physics_sim_receiver_client_close(&receiver_client);
   assert(physics_sim_receiver_client_open_profile(&receiver_client,receiver_path,receiver_profile));receiver_restarts++;
  }
#endif
  /* Test failures again after the gas carries nonzero heat, not only at t=0. */
  if((gas.steps==0||gas.steps==PHYSICS_SIM_SHEAR_OBSERVE)
#ifdef PHYSICS_SIM_SHEAR_DURABLE
   &&(!durable||!(checkpoint.tested&(unsigned)phase))
#endif
  ){
   ThermalReceiver *saved=malloc((size_t)nx*sizeof(*saved)),*out=malloc((size_t)nx*sizeof(*out)),*sentinel=malloc((size_t)nx*sizeof(*sentinel));assert(saved&&out&&sentinel);
   memcpy(saved,r,(size_t)nx*sizeof(*r));memset(out,0xa5,(size_t)nx*sizeof(*out));memcpy(sentinel,out,(size_t)nx*sizeof(*out));
   double *energy=malloc((size_t)gas.grid.count*sizeof(double));assert(energy);memcpy(energy,gas.energy,(size_t)gas.grid.count*sizeof(double));
   #ifdef PHYSICS_SIM_LOCALIZED_COLUMNS
   uint64_t local_saved=physics_sim_localized_checksum();
#endif
   int failures=3;
#ifdef PHYSICS_SIM_SHEAR_NORMAL256
   failures=4;
#endif
#ifdef PHYSICS_SIM_LIVE_RECEIVER
   if(receiver_path)failures=5;
#endif
   int saved_steps=gas.steps;double saved_time=gas.time,saved_input=gas.input_j;
   for(int fail=1;fail<=failures;fail++){assert(!stage(&gas,&work,r,out,source,h,fail));assert(!memcmp(r,saved,(size_t)nx*sizeof(*r))&&!memcmp(out,sentinel,(size_t)nx*sizeof(*out))&&!memcmp(energy,gas.energy,(size_t)gas.grid.count*sizeof(double)));assert(gas.steps==saved_steps&&gas.time==saved_time&&gas.input_j==saved_input);refusals++;
#ifdef PHYSICS_SIM_LOCALIZED_COLUMNS
   assert(local_saved==physics_sim_localized_checksum());
#endif
#ifdef PHYSICS_SIM_LIVE_RECEIVER
   if(receiver_path&&fail==5){receiver_stages+=receiver_client.stages;receiver_requests+=receiver_client.transactions;physics_sim_receiver_client_close(&receiver_client);assert(physics_sim_receiver_client_open_profile(&receiver_client,receiver_path,receiver_profile));receiver_restarts++;}
#endif
   }
   free(energy);free(saved);free(out);free(sentinel);
#ifdef PHYSICS_SIM_SHEAR_DURABLE
   if(durable)checkpoint.tested|=(unsigned)phase;
#endif
  }
#ifdef PHYSICS_SIM_SHEAR_DURABLE
  if(durable&&!checkpoint.pending_end){
   checkpoint.gas=&gas;checkpoint.refusals=refusals;
   PhysicsSimShearCheckpointStatus status=physics_sim_shear_save(argv[9],&checkpoint,argv[11],checkpoint_digest,end_tick,checkpoint.last_operation,0,&checkpoint_digest);
   if(status!=SHEAR_CP_OK&&status!=SHEAR_CP_RELOAD){physics_sim_receiver_client_close(&receiver_client);return 4;}
   if(status==SHEAR_CP_RELOAD&&!physics_sim_shear_load(argv[9],&checkpoint,argv[11],&checkpoint_digest)){physics_sim_receiver_client_close(&receiver_client);return 4;}
   if(publish_fail==4){physics_sim_receiver_client_close(&receiver_client);return 75;}
  }
#endif
  ThermalReceiver *next=cfd_memory_calloc((size_t)nx,sizeof(*next));assert(next);
  if(!stage(&gas,&work,r,next,source,h,0)){observe(&gas,r);fprintf(stderr,"joint refusal at tick %d\n",gas.steps);
#ifdef PHYSICS_SIM_LIVE_RECEIVER
   physics_sim_receiver_client_close(&receiver_client);
#endif
   return 3;}
  for(int i=0;i<nx;i++){
   double dt=(double)window/PHYSICS_SIM_SHEAR_TPS,power=(next[i].received_q_j-r[i].received_q_j)/dt;
   replay_difference[i]=replay_difference[i]*exp(-.08*dt)+power*nx/4*(-expm1(-.08*dt));
  }
  memcpy(r,next,(size_t)nx*sizeof(*r));cfd_memory_free(next);
  CfdOpenAtmosphere3d swap=gas;gas=work;work=swap;
#ifdef PHYSICS_SIM_SHEAR_DURABLE
  if(durable&&publish_fail==5){physics_sim_receiver_client_close(&receiver_client);return 75;}
#endif
  if(!(gas.steps%PHYSICS_SIM_SHEAR_OBSERVE))observe(&gas,r);
 }
#ifdef PHYSICS_SIM_SHEAR_DURABLE
 if(durable){
  checkpoint.gas=&gas;checkpoint.refusals=refusals;uint64_t operation=checkpoint.operation;
  PhysicsSimShearCheckpointStatus status=physics_sim_shear_save(argv[9],&checkpoint,argv[11],checkpoint_digest,0,operation,publish_fail==6?0:publish_fail,&checkpoint_digest);
  if(status!=SHEAR_CP_OK&&status!=SHEAR_CP_RELOAD){physics_sim_receiver_client_close(&receiver_client);return 74;}
  if(publish_fail==6){physics_sim_receiver_client_close(&receiver_client);_Exit(76);}
  if(status==SHEAR_CP_RELOAD){uint64_t published=checkpoint_digest;if(!physics_sim_shear_load(argv[9],&checkpoint,argv[11],&checkpoint_digest)||published!=checkpoint_digest||checkpoint.pending_end||gas.steps!=end_tick){physics_sim_receiver_client_close(&receiver_client);return 74;}}
  fprintf(stderr,"durable: committed range %d %d operation %016llx digest %016llx\n",begin_tick,gas.steps,(unsigned long long)operation,(unsigned long long)checkpoint_digest);
 }
#endif
 double velocity=0;for(int a=0;a<3;a++)for(int j=0;j<gas.grid.count+(a==2?gas.plane:0);j++)velocity=fmax(velocity,fabs(gas.velocity[a*gas.grid.count+j]-(a==0?.05*(j/gas.plane+.5)/gas.grid.n[2]:0)));
 double boundary=0;for(int i=0;i<4*gas.boundary_faces;i++)boundary+=fabs(gas.flux[i]);
 assert(boundary==0);
#ifndef PHYSICS_SIM_LOCALIZED_COLUMNS
 assert(gas.scalar_work_cells==(unsigned long long)steps*gas.grid.count*4);
#endif
 double balance=gas_balance(&gas,received(r,nx));size_t peak=budget.peak_bytes;
 #ifdef PHYSICS_SIM_LOCALIZED_COLUMNS
 physics_sim_localized_destroy();
#endif
#ifdef PHYSICS_SIM_LIVE_RECEIVER
 if(receiver_path){receiver_stages+=receiver_client.stages;receiver_requests+=receiver_client.transactions;physics_sim_receiver_client_close(&receiver_client);
  fprintf(stderr,"receiver: %llu staged columns; %llu requests; %u restarts\n",(unsigned long long)receiver_stages,(unsigned long long)receiver_requests,receiver_restarts);}
#endif
 cfd_memory_free(r);cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&gas);cfd_open_atmosphere3d_destroy(&work);cfd_memory_scope(prior);
 printf("{\"terminal\":true,\"n\":%d,\"window_ticks\":%d,\"steps\":%d,\"h\":%.17g,\"gas_balance_j\":%.17g,\"velocity_error\":%.17g,\"refusal_checks\":%u,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu",nz,window,steps,h,balance,velocity,refusals,peak,budget.live_bytes);
#ifdef PHYSICS_SIM_LOCALIZED_COLUMNS
 physics_sim_localized_fields();
#endif
 puts("}");
#ifdef PHYSICS_SIM_PATCH6_TRACE
 assert(fclose(patch6_trace)==0);patch6_trace=NULL;
#endif
#ifdef PHYSICS_SIM_COLUMNS_SOLID_TRACE
 assert(fclose(columns_solid_trace)==0);columns_solid_trace=NULL;
#endif
 return budget.live_bytes||velocity>1e-12||fabs(balance)>1e-8;
}
