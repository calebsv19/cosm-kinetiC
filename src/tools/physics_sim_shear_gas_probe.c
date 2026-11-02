/* Full fixed-grid gas-only prescribed shear prefix. No wall/solid owner. */
#include "physics_sim_localized_columns.h"
#include "physics_sim_shear_clock.h"
#include "physics_sim_localized_state.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static double band(const CfdOpenAtmosphere3d *s,double xl,double xh,double zl,double zh){
 double sum=0;
 for(int k=0;k<256;k++){for(int y=0;y<4;y++){for(int x=0;x<128;x++){
  double ox=fmax(0,fmin(xh,(x+1)*s->grid.h[0])-fmax(xl,x*s->grid.h[0]));
  double oz=fmax(0,fmin(zh,(k+1)*s->grid.h[2])-fmax(zl,k*s->grid.h[2]));
  sum+=s->energy[k*512+y*128+x]*ox/s->grid.h[0]*oz/s->grid.h[2];
 }}}
 return sum/(1206*.25*(xh-xl)*(zh-zl));
}
int main(int argc,char **argv){
 if(argc!=2&&argc!=3)return 2;int steps=atoi(argv[1]);if(steps!=1&&steps!=5&&steps!=160)return 2;
 setvbuf(stdout,NULL,_IOLBF,0);
 CfdMemoryBudget budget={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
 CfdOpenAtmosphere3d gas={0};gas.grid.n[0]=128;gas.grid.n[1]=4;gas.grid.n[2]=256;gas.grid.count=128*4*256;gas.plane=512;
 gas.grid.h[0]=.25/128;gas.grid.h[1]=.25/4;gas.grid.h[2]=.0625/256;gas.grid.volume=gas.grid.h[0]*gas.grid.h[1]*gas.grid.h[2];gas.dt=1./6400;
 gas.energy=cfd_memory_calloc((size_t)gas.grid.count,sizeof(double));assert(gas.energy&&physics_sim_localized_init(&gas));
 for(int tick=1;tick<=steps;tick++){
  assert(physics_sim_localized_gas_begin());assert(physics_sim_localized_step(&gas));physics_sim_localized_commit();
  if(tick==1||tick%5==0||tick==steps){double mass=0;for(int j=0;j<gas.grid.count;j++)mass+=gas.energy[j];
   printf("{\"tick\":%d,\"time_s\":%.17g,\"gas_j\":%.17g,\"source_j\":%.17g,\"balance_j\":%.17g,\"tail_k\":%.17g,\"transport_k\":%.17g,\"normal_band_k\":%.17g,\"normal_cell_k\":%.17g",tick,gas.time,mass,gas.input_j,mass-gas.input_j,band(&gas,3./32,1./8,1./64,1./32),band(&gas,1./16,3./32,1./64,1./32),band(&gas,0,.25,0,1./128),band(&gas,0,.25,0,gas.grid.h[2]));physics_sim_localized_fields();puts("}");
  }
 }
 if(argc==3){PhysicsSimLocalizedState snapshot={0};assert(physics_sim_localized_capture(&snapshot));FILE *f=fopen(argv[2],"wb");assert(f);assert(fwrite(snapshot.values,sizeof(double),snapshot.count,f)==snapshot.count);assert(!fclose(f));physics_sim_localized_state_free(&snapshot);}
 physics_sim_localized_destroy();cfd_memory_free(gas.energy);cfd_memory_scope(prior);
 printf("{\"terminal\":true,\"memory_live_bytes\":%zu,\"memory_peak_bytes\":%zu,\"wall_exchange_j\":0,\"feedback\":false}\n",budget.live_bytes,budget.peak_bytes);return budget.live_bytes?3:0;
}
