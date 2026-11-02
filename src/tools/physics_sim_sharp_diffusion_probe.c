/* Uniform transverse sharp source: native scalar, no momentum/buoyancy.
 * Insulated diffusion boundaries permit an independent cosine-series reference. */
#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_passive3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
    if(argc!=5 && argc!=6)return 2;
    if(argc==6 && strcmp(argv[5],"rk2"))return 2;
    int nz=atoi(argv[1]),steps=atoi(argv[3]);double length=atof(argv[2]),end=atof(argv[4]);
    if((nz!=32 && nz!=64 && nz!=128 && nz!=256) || (length!=1 && length!=.0625 && length!=.03125) || steps<1 || steps>8192 || (end!=3.2 && end!=.0025))return 2;
    int n[3]={4,4,nz};double l[3]={1,1,length},zero[2]={0,0},ambient[2]={300,300},dt=end/steps;
    CfdMemoryBudget b={.limit_bytes=16777216};CfdMemoryBudget *prior=cfd_memory_scope(&b);CfdOpenAtmosphere3d s;
    if(!cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,.026,0,dt,zero,ambient,zero,false,0,0,.1))return 3;
    s.scalar_rk2=argc==6;s.muscl=true;s.scalar_mc=true;s.cache_projection=true;
    int edge=length==1?nz/8:nz/2;
    double *source=cfd_memory_calloc((size_t)2*s.grid.count,sizeof(double));if(!source)return 3;
    for(int q=0;q<edge*s.plane;q++)source[q]=1206*s.grid.volume*dt;
    for(int step=0;step<steps;step++)if(!cfd_open_atmosphere3d_step(&s,source,source+s.grid.count)){fprintf(stderr,"%d: %s\n",step,s.step_rejection);return 4;}
    if(s.scalar_work_cells!=(unsigned long long)steps*s.grid.count*(s.scalar_rk2?2:1))return 6;
    double balance=cfd_passive3d_total(s.energy,s.grid.count)-s.input_j;
    printf("{\"n\":%d,\"length_m\":%.17g,\"edge_m\":%.17g,\"time_s\":%.17g,\"steps\":%d,\"dt_s\":%.17g,\"heat_balance_j\":%.17g,\"profile_k\":[",nz,length,edge*length/nz,s.time,steps,dt,balance);
    for(int k=0;k<nz;k++)printf("%s%.17g",k?",":"",s.energy[k*s.plane]/(1206*s.grid.volume));
    printf("],\"memory_peak_bytes\":%zu",b.peak_bytes);
    cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    printf(",\"memory_live_bytes\":%zu,\"rejected_allocations\":%zu}\n",b.live_bytes,b.rejected_allocations);
    return b.live_bytes || b.rejected_allocations || fabs(balance)>1e-8?5:0;
}
