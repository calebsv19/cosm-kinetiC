/* Qualifier-only deterministic line batching. Each line uses the existing
 * scalar operators and its own workspace; x then z barriers preserve order.
 * No source, wall, shared worker service or feedback runs inside this batch. */
#ifndef PHYSICS_SIM_SHEAR_GAS_LINES_H
#define PHYSICS_SIM_SHEAR_GAS_LINES_H
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#endif
enum { PHYSICS_SIM_GAS_LINE_WORKERS=4 };
typedef struct {PhysicsSimNonuniformAdvection adv;PhysicsSimTailDiffusion x,z;PhysicsSimTailTime tx,tz;double row[256];bool ok;} PhysicsSimGasLine;
static PhysicsSimGasLine gas_lines[PHYSICS_SIM_GAS_LINE_WORKERS];
typedef struct {double *field;const double *px,*pz;int nx,nz,axis;double alpha,dt;} PhysicsSimGasLineJob;
static bool physics_sim_gas_lines_init(int nx,int nz,const double *wx,const double *wz,const double *px,const double *pz,double alpha){
 for(int i=0;i<PHYSICS_SIM_GAS_LINE_WORKERS;i++){
  PhysicsSimGasLine *w=&gas_lines[i];
  if(!physics_sim_nonuniform_advection_init(&w->adv,nx,wx,1536)||!physics_sim_tail_diffusion_init(&w->x,nx,wx,true)||!physics_sim_tail_diffusion_init(&w->z,nz,wz,false)||!physics_sim_tail_heater_source(&w->x,px,alpha)||!physics_sim_tail_heater_source(&w->z,pz,alpha))return false;
 }return true;
}
static void physics_sim_gas_line_execute(void *context,size_t id){
 PhysicsSimGasLineJob *job=context;PhysicsSimGasLine *w=&gas_lines[id];w->ok=true;
 if(job->axis==0){
  for(int k=(int)id;k<job->nz;k+=PHYSICS_SIM_GAS_LINE_WORKERS){
   double *v=job->field+k*job->nx;w->x.heater_power=job->pz[k]?job->px:NULL;
   if(!physics_sim_nonuniform_advection_step(&w->adv,v,.05*(k+.5)/job->nz,job->dt,3)||!physics_sim_tail_time_step(&w->x,&w->tx,v,job->alpha,job->dt,2)){w->ok=false;return;}
  }
 }else for(int j=(int)id;j<job->nx;j+=PHYSICS_SIM_GAS_LINE_WORKERS){
  for(int k=0;k<job->nz;k++)w->row[k]=job->field[k*job->nx+j];w->z.heater_power=job->px[j]?job->pz:NULL;
  if(!physics_sim_tail_time_step(&w->z,&w->tz,w->row,job->alpha,job->dt,2)){w->ok=false;return;}
  for(int k=0;k<job->nz;k++)job->field[k*job->nx+j]=w->row[k];
 }
}
static bool physics_sim_gas_lines_step(double *v,int nx,int nz,const double *px,const double *pz,double alpha,double dt){
 PhysicsSimGasLineJob job={v,px,pz,nx,nz,0,alpha,dt};
 for(int axis=0;axis<2;axis++){
  job.axis=axis;
#ifdef __APPLE__
  dispatch_apply_f(PHYSICS_SIM_GAS_LINE_WORKERS,dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT,0),&job,physics_sim_gas_line_execute);
#else
  for(int id=0;id<PHYSICS_SIM_GAS_LINE_WORKERS;id++)physics_sim_gas_line_execute(&job,(size_t)id);
#endif
  for(int id=0;id<PHYSICS_SIM_GAS_LINE_WORKERS;id++)if(!gas_lines[id].ok)return false;
 }return true;
}
#endif
