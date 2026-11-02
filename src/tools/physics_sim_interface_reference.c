/* Short profile sampling of the existing independently qualified Fourier/CN
 * reference. Avoids another gas grid; exits at .5 s. Weak distant profiles
 * remain cancellation-limited and must not be claimed as qualified. */
#include <complex.h>
static void interface_snapshot(const double complex *,const double complex *,int,int,int,int,double);
#define PHYSICS_SIM_THERMAL_REFERENCE_SNAPSHOT interface_snapshot
#define main columns_reference_main
#include "physics_sim_thermal_columns_reference.c"
#undef main
static void interface_snapshot(const double complex *t,const double complex *solid,int n,int modes,int step,int steps,double h){
 (void)solid;(void)h;
 double time=step*3.2/steps;
 if(fabs(time-.4)<1e-12||fabs(time-.475)<1e-12||fabs(time-.4875)<1e-12||fabs(time-.490625)<1e-12||fabs(time-.5)<1e-12){
  fprintf(stderr,"{\"time_s\":%.17g,\"profiles\":[",time);
  for(int x=44;x<=68;x++)fprintf(stderr,"%s{\"parent\":%d,\"band_k\":%.17g,\"near_k\":%.17g}",x==44?"":",",x,sample(t,n,modes,x*.25/128,(x+1)*.25/128,n/4,n/2),sample(t,n,modes,x*.25/128,(x+1)*.25/128,0,n/8));
  fprintf(stderr,"]}\n");
 }
 if(time>=.5) {free((void *)t);exit(0);}
}
int main(int argc,char **argv){return columns_reference_main(argc,argv);}
