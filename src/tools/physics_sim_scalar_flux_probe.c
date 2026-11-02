/* Single Euler step on independently supplied exact cell averages, measuring
 * the native instantaneous spatial flux. Not an evolved trajectory comparison. */
#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_passive3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
    if(argc!=5 || (strcmp(argv[4],"mc") && strcmp(argv[4],"minmod")))return 2;
    int nz=atoi(argv[1]);double length=atof(argv[2]);
    if((nz!=32 && nz!=64 && nz!=128 && nz!=256) || (length!=1 && length!=.5))return 2;
    int n[3]={4,4,nz};double l[3]={1,1,length},z[2]={0,0},t[2]={300,300},profile[256];
    FILE *f=fopen(argv[3],"rb");if(!f)return 2;
    if(fread(profile,sizeof(double),(size_t)nz,f)!=(size_t)nz || fgetc(f)!=EOF)return 2;
    fclose(f);
    CfdMemoryBudget b={.limit_bytes=16777216};CfdMemoryBudget *prior=cfd_memory_scope(&b);CfdOpenAtmosphere3d s;
    if(!cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,.026,0,1e-6,z,t,z,false,0,0,.1))return 3;
    s.muscl=true;s.scalar_mc=!strcmp(argv[4],"mc");s.cache_projection=true;
    for(int k=0;k<nz;k++)for(int q=0;q<s.plane;q++)s.energy[k*s.plane+q]=1206*s.grid.volume*profile[k];
    s.initial_j=cfd_passive3d_total(s.energy,s.grid.count);
    for(int q=0;q<s.grid.count+s.plane;q++)s.velocity[2*s.grid.count+q]=.05;
    double *source=cfd_memory_calloc((size_t)2*s.grid.count,sizeof(double));if(!source)return 3;
    int first=(int)round(.4375/s.grid.h[2])*s.plane;
    double before=cfd_passive3d_total(s.energy+first,s.grid.count-first);
    if(!cfd_open_atmosphere3d_step(&s,source,source+s.grid.count))return 4;
    double after=cfd_passive3d_total(s.energy+first,s.grid.count-first);
    double outward=cfd_passive3d_total(s.flux+s.boundary_faces+s.plane,s.plane);
    printf("{\"n\":%d,\"length_m\":%.17g,\"reconstruction\":\"%s\",\"flux_k_m_s\":%.17g",nz,length,argv[4],(after-before+outward)/(1206*s.dt));
    cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    printf(",\"memory_live_bytes\":%zu,\"rejected_allocations\":%zu}\n",b.live_bytes,b.rejected_allocations);
    return b.live_bytes || b.rejected_allocations?5:0;
}
