/* Bounded prescribed-source numerical probe. This executes the production
 * solver directly; binary arrays are native doubles, X-fast, extensive SI.
 * It is a local qualification tool, not a worker/package protocol. */
#include "app/cfd_open_atmosphere3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER
#include "app/cfd_memory.h"
static int capacity_check(void) {
    CfdMemoryBudget budget={.limit_bytes=2147483648ULL};
    CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s;int n[3]={128,128,128};
    double length[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    if(!cfd_open_atmosphere3d_init(&s,n,length,1.2,1.8e-5,1005,300,.026,.0005,.005,zero,ambient,zero,true,9.81,1./300,.1))return 3;
    s.ground=true;s.muscl=true;s.cache_projection=true;
    if(!cfd_open_atmosphere3d_lateral(&s))return 3;
    double *source=cfd_memory_calloc((size_t)2*s.grid.count,sizeof(double));
    if(!source || !cfd_open_atmosphere3d_step(&s,source,source+s.grid.count) || !cfd_open_atmosphere3d_valid(&s))return 5;
    cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    printf("{\"qualification_profile\":\"full_grid_128_v1\",\"capacity_check\":true,\"cells\":2097152,\"memory_limit_bytes\":%zu,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu,\"rejected_allocations\":%zu}\n",budget.limit_bytes,budget.peak_bytes,budget.live_bytes,budget.rejected_allocations);
    return budget.live_bytes || budget.rejected_allocations ? 6 : 0;
}
#endif
int main(int argc,char **argv) {
#ifdef PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER
    if(argc==2 && !strcmp(argv[1],"--capacity-check"))return capacity_check();
#endif
    if(argc!=6){fprintf(stderr,"usage: probe grid dt box sources.bin fields.bin\n");return 2;}
    int n[3];char extra;int axes=sscanf(argv[1],"%dx%dx%d%c",n,n+1,n+2,&extra);
    if(axes!=3){int side;if(sscanf(argv[1],"%d%c",&side,&extra)!=1)return 2;n[0]=n[1]=n[2]=side;}
    for(int a=0;a<3;a++)if(n[a]<8 || n[a]>128 || n[a]%8)return 2;
#ifdef PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER
    if(n[0]!=n[1] || n[1]!=n[2] || (n[0]!=32 && n[0]!=64 && n[0]!=128) || strcmp(argv[3],"0"))return 2;
#else
    if((long long)n[0]*n[1]*n[2]>524288)return 2;
#endif
    char *end;double dt=strtod(argv[2],&end);
    if(*end || !isfinite(dt) || dt<.0005 || dt>.005 || fabs(.1/dt-round(.1/dt))>1e-9 ||
       (strcmp(argv[3],"0") && strcmp(argv[3],"1")))return 2;
    double length[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};CfdOpenAtmosphere3d s;
#ifdef PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER
    CfdMemoryBudget budget={.limit_bytes=2147483648ULL};
    CfdMemoryBudget *prior=cfd_memory_scope(&budget);
#endif
    if(!cfd_open_atmosphere3d_init(&s,n,length,1.2,1.8e-5,1005,300,.026,.0005,dt,zero,ambient,zero,true,9.81,1./300,.1))return 3;
    s.ground=true;s.muscl=true;s.cache_projection=true;s.max_scalar_work_cells=1000000000;
#ifdef PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER
    s.max_scalar_work_cells=8000000000ULL;
#endif
    if(!cfd_open_atmosphere3d_lateral(&s))return 3;
    if(atoi(argv[3])){int lo[3],hi[3];for(int a=0;a<3;a++){lo[a]=3*n[a]/8;hi[a]=5*n[a]/8;}if(!cfd_open_atmosphere3d_box(&s,lo,hi))return 3;}
    FILE *input=fopen(argv[4],"rb");if(!input)return 4;
    int count=s.grid.count,steps=(int)round(.1/dt);
#ifdef PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER
    double *source=cfd_memory_calloc((size_t)2*count,sizeof(double));
#else
    double *source=calloc((size_t)2*count,sizeof(double));
#endif
    if(!source)return 3;
    for(int frame=0;frame<32;frame++) {
        if(fread(source,sizeof(double),(size_t)2*count,input)!=(size_t)2*count)return 4;
        for(int q=0;q<2*count;q++)source[q]/=steps;
        for(int t=0;t<steps;t++)if(!cfd_open_atmosphere3d_step(&s,source,source+count)){fprintf(stderr,"rejected frame=%d step=%d %s\n",frame,t,s.step_rejection);return 5;}
        fprintf(stderr,"frame %d/32\n",frame+1);fflush(stderr);
    }
    if(fgetc(input)!=EOF)return 4;
    fclose(input);FILE *output=fopen(argv[5],"wb");if(!output)return 4;
    if(fwrite(s.energy,sizeof(double),(size_t)2*count,output)!=(size_t)2*count || fwrite(s.velocity,sizeof(double),s.velocity_count,output)!=(size_t)s.velocity_count || fclose(output))return 4;
    double totals[2]={0,0},out[2]={0,0},in[2]={0,0},peak=300;
    for(int q=0;q<count;q++){totals[0]+=s.energy[q];totals[1]+=s.smoke[q];peak=fmax(peak,300+s.energy[q]/(1.2*1005*s.grid.volume));}
    for(int a=0;a<2;a++)for(int q=0;q<s.boundary_faces;q++){in[a]+=s.flux[2*a*s.boundary_faces+q];out[a]+=s.flux[(2*a+1)*s.boundary_faces+q];}
    char grid[64];if(n[0]==n[1] && n[1]==n[2])snprintf(grid,sizeof grid,"%d",n[0]);else snprintf(grid,sizeof grid,"[%d,%d,%d]",n[0],n[1],n[2]);
    const char *format="{\"grid\":%s,\"dt_s\":%.17g,\"time_s\":%.17g,\"energy_j\":%.17g,\"smoke_kg\":%.17g,\"heat_balance_j\":%.17g,\"smoke_balance_kg\":%.17g,\"peak_temperature_k\":%.17g,\"divergence\":%.17g,\"pressure_residual\":%.17g}\n";
#ifdef PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER
    printf("{\"qualification_profile\":\"full_grid_128_v1\",\"scalar_transport_scheme\":\"muscl_minmod\",\"memory_limit_bytes\":%zu,\"memory_peak_bytes\":%zu,\"scalar_work_cells\":%llu,\"max_scalar_work_cells\":%llu,",budget.limit_bytes,budget.peak_bytes,s.scalar_work_cells,s.max_scalar_work_cells);format++;
#endif
    printf(format,grid,dt,s.time,totals[0],totals[1],totals[0]-s.input_j-in[0]+out[0],totals[1]-s.input_kg-in[1]+out[1],peak,s.divergence,s.residual);
#ifdef PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER
    cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    if(budget.live_bytes || budget.rejected_allocations)return 6;
    return 0;
#else
    free(source);cfd_open_atmosphere3d_destroy(&s);return 0;
#endif
}
