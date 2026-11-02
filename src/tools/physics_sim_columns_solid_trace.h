/* Accepted all-column packets and native energies for independent BE audit.
 * Exact hex doubles; no source replay, temperature reconstruction or cutoff. */
#ifndef PHYSICS_SIM_COLUMNS_SOLID_TRACE_H
#define PHYSICS_SIM_COLUMNS_SOLID_TRACE_H
static FILE *columns_solid_trace;
static void columns_solid_open(const char *path,int nx,int nz,int window,int steps){
 assert(nx==128&&(nz==128||nz==256)&&window==5);columns_solid_trace=fopen(path,"wx");assert(columns_solid_trace);
#ifdef PHYSICS_SIM_SHEAR_NORMAL256
 assert(fprintf(columns_solid_trace,"columns_solid_v2 %d %d %d %d %d %s %s\n",nx,nz,window,steps,PHYSICS_SIM_SHEAR_TPS,COUPLED_SOURCE_ID,COUPLED_FIXTURE_ID)>0);
#else
 assert(fprintf(columns_solid_trace,"columns_solid_v1 %d %d %d %d %s %s\n",nx,nz,window,steps,COUPLED_SOURCE_ID,COUPLED_FIXTURE_ID)>0);
#endif
}
static void columns_solid_commit(const ThermalReceiver *before,const ThermalReceiver *after,const PhysicsSimHeatedFace *face,const double *debit,int nx){
 assert(fprintf(columns_solid_trace,"%llu",(unsigned long long)after[0].solid.time.ticks)>0);
 for(int x=0;x<nx;x++){
  double amounts[100];assert(thermal_inert_rebin(&before[x].plan,before[x].solid.completed_steps,before[x].config.window_steps,face[x].gas_to_surface_j,amounts));
  assert(fprintf(columns_solid_trace," %a %a %a %a %a",face[x].gas_to_surface_j,debit[x],after[x].received_q_j-before[x].received_q_j,after[x].solid.native.energy_j[0],after[x].solid.native.energy_j[1])>0);
  for(unsigned j=0;j<before[x].config.window_steps;j++)assert(fprintf(columns_solid_trace," %a",amounts[j])>0);
 }
 assert(fputc('\n',columns_solid_trace)!=EOF);
}
#endif
