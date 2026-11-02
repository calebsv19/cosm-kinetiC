/* Export exact-in-x, refined-in-z reference averages and face fluxes.
 * Includes the existing reference algorithm without duplicating its evolution. */
#include <complex.h>
#include <assert.h>
static const char *snapshot_root;
static void snapshot(const double complex *,const double complex *,int,int,int,int,double);
#define PHYSICS_SIM_THERMAL_REFERENCE_SNAPSHOT snapshot
#define main column_reference_main
#include "physics_sim_thermal_columns_reference.c"
#undef main
static void put(FILE *f,double v){assert(fwrite(&v,sizeof(v),1,f)==1);}
static void snapshot(const double complex *t,const double complex *s0,int n,int modes,int step,int steps,double h){
 if(step!=steps*15/32&&step!=steps)return;
 assert(h==10);int tick=step==steps?5120:2400;double dz=.0625/n,alpha=.026/1206;
 for(int nx=64;nx<=128;nx*=2)for(int nz=64;nz<=128;nz*=2){
  char path[2048];snprintf(path,sizeof path,"%s/snapshot-%d-%d-%d.bin",snapshot_root,tick,nx,nz);
  FILE *f=fopen(path,"wb");assert(f);double dx=.25/nx;
  for(int z=0;z<nz;z++)for(int x=0;x<nx;x++)put(f,sample(t,n,modes,x*dx,(x+1)*dx,z*n/nz,(z+1)*n/nz));
  for(int x=0;x<nx;x++){
   double v=0;for(int m=0;m<modes;m++)v+=(m?2:1)*creal(s0[m]*average(m,x*dx,(x+1)*dx));put(f,v);
  }
  double heff=1/(dz/(2*.026)+1/h+.005/(2*.16));
  for(int x=0;x<nx;x++){
   double v=0;for(int m=0;m<modes;m++)v+=(m?2:1)*creal((t[m*n]-s0[m])*average(m,x*dx,(x+1)*dx));
   put(f,heff*dx*.25*v); /* W into solid, refined Robin flux */
  }
  for(int kind=0;kind<2;kind++)for(int band=0;band<2;band++)for(int x=0;x<nx;x++){
   double flux=0;int lo=band?n/4:0,hi=band?n/2:n/8;
   for(int m=0;m<modes;m++){
    double k=2*acos(-1)*m/.25;double complex phase=cexp(I*k*x*dx),sum=0;
    for(int z=lo;z<hi;z++)sum+=t[m*n+z]*(kind?-alpha*I*k:.05*(z+.5)/n)*dz;
    flux+=(m?2:1)*creal(sum*phase);
   }
   put(f,1206*.25*flux); /* W in +x across the chosen z band */
  }
  assert(!fclose(f));
 }
}
int main(int argc,char **argv){if(argc!=6)return 2;snapshot_root=argv[5];return column_reference_main(argc-1,argv);}
