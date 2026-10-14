/* Fixed-scalar temporal qualification of evolving flow. This executes the production
 * solver directly; binary arrays are native doubles, X-fast, extensive SI.
 * It is a local qualification tool, not a worker/package protocol. */
#include "app/cfd_open_atmosphere3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER) || !defined(PHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT)
#error This tool requires the contained fixed-scalar qualification profile
#endif
#include "app/cfd_memory.h"
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
#include "physics_sim_momentum_capture.h"
#endif
#ifdef PHYSICS_SIM_VERTICAL_FLOW_QUALIFIER
#include "physics_sim_vertical_source.h"
#ifdef PHYSICS_SIM_HORIZONTAL_MATCHED_TIME_QUALIFIER
#include "physics_sim_horizontal_source.h"
#endif
#endif
#ifdef PHYSICS_SIM_FLOW_SPATIAL_QUALIFIER
#include "physics_sim_flow_source.h"
#endif
int main(int argc,char **argv) {
    int windows=32;
#ifdef PHYSICS_SIM_VERTICAL_FLOW_QUALIFIER
    if(argc==7 && !strcmp(argv[6],"1"))windows=1;
    else if(argc!=6)return 2;
#else
    if(argc!=6){fprintf(stderr,"usage: probe grid dt box sources.bin fields.bin\n");return 2;}
#endif
    int n[3];char extra;int axes=sscanf(argv[1],"%dx%dx%d%c",n,n+1,n+2,&extra);
    if(axes!=3){int side;if(sscanf(argv[1],"%d%c",&side,&extra)!=1)return 2;n[0]=n[1]=n[2]=side;}
#ifdef PHYSICS_SIM_VERTICAL_FLOW_QUALIFIER
    #ifdef PHYSICS_SIM_HORIZONTAL_MATCHED_TIME_QUALIFIER
    if(n[0]!=256 || n[1]!=128 || n[2]!=256 || strcmp(argv[3],"0"))return 2;
    #else
    if(n[0]!=128 || n[1]!=128 || (n[2]!=128 && n[2]!=256) || strcmp(argv[3],"0"))return 2;
    #endif
#else
    for(int a=0;a<3;a++)if(n[a]<8 || n[a]>128 || n[a]%8)return 2;
    if(n[0]!=n[1] || n[1]!=n[2] || strcmp(argv[3],"0"))return 2;
#endif
#ifdef PHYSICS_SIM_FLOW_SPATIAL_QUALIFIER
    #ifndef PHYSICS_SIM_HORIZONTAL_MATCHED_TIME_QUALIFIER
    if(n[0]!=64 && n[0]!=128)return 2;
    #endif
#else
    if(n[0]!=64)return 2;
#endif
    char *end;double dt=strtod(argv[2],&end);
    if(*end || !isfinite(dt) || (dt!=.005 && dt!=.0025 && dt!=.00125) || fabs(.1/dt-round(.1/dt))>1e-9 ||
       (strcmp(argv[3],"0") && strcmp(argv[3],"1")))return 2;
    double length[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};CfdOpenAtmosphere3d s;
    CfdMemoryBudget budget={.limit_bytes=2147483648ULL};
#ifdef PHYSICS_SIM_VERTICAL_FLOW_QUALIFIER
    budget.limit_bytes=4294967296ULL;
#endif
    #ifdef PHYSICS_SIM_HORIZONTAL_MATCHED_TIME_QUALIFIER
    budget.limit_bytes=8589934592ULL;
    #endif
    CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    if(!cfd_open_atmosphere3d_init(&s,n,length,1.2,1.8e-5,1005,300,.026,.0005,dt,zero,ambient,zero,true,9.81,1./300,.1))return 3;
    s.ground=true;s.muscl=true;s.cache_projection=true;s.max_scalar_work_cells=8000000000ULL;
#ifdef PHYSICS_SIM_VERTICAL_FLOW_QUALIFIER
    s.max_scalar_work_cells=12000000000ULL;
#endif
    #ifdef PHYSICS_SIM_HORIZONTAL_MATCHED_TIME_QUALIFIER
    s.max_scalar_work_cells=22000000000ULL;
    #endif
    if(!cfd_open_atmosphere3d_lateral(&s))return 3;
    FILE *input=fopen(argv[4],"rb");if(!input)return 4;
    int count=s.grid.count,steps=(int)round(.1/dt);
    double *source=cfd_memory_calloc((size_t)2*count,sizeof(double));
    if(!source)return 3;
#ifdef PHYSICS_SIM_FLOW_SPATIAL_QUALIFIER
    #ifdef PHYSICS_SIM_HORIZONTAL_MATCHED_TIME_QUALIFIER
    if(dt!=.0025)return 2;
    #elif defined(PHYSICS_SIM_VERTICAL_MATCHED_TIME_QUALIFIER)
    if(dt!=.0025 || n[0]!=128 || n[1]!=128 || n[2]!=256)return 2;
    #elif !defined(PHYSICS_SIM_FINE_FLOW_TIME_QUALIFIER)
    if(dt!=.005)return 2;
#else
    if(n[0]!=128)return 2;
#endif
    double *parent=cfd_memory_calloc(2*64*64*64,sizeof(double));
    if(!parent)return 3;
#endif
    double maximum_divergence=0,maximum_residual=0;
    for(int frame=0;frame<windows;frame++) {
#ifdef PHYSICS_SIM_FLOW_SPATIAL_QUALIFIER
#ifdef PHYSICS_SIM_VERTICAL_FLOW_QUALIFIER
        if(fread(parent,sizeof(double),2*64*64*64,input)!=2*64*64*64 ||
           #ifdef PHYSICS_SIM_HORIZONTAL_MATCHED_TIME_QUALIFIER
           !physics_sim_horizontal_source(parent,128,source)
#else
           !physics_sim_vertical_source(parent,n[0],n[2],source)
#endif
           )return 4;
#else
        if(fread(parent,sizeof(double),2*64*64*64,input)!=2*64*64*64 ||
           !(n[0]==64 ? physics_sim_flow_source(parent,64,source,64) :
                         physics_sim_flow_support(parent,128,source)))return 4;
#endif
#else
        if(fread(source,sizeof(double),(size_t)2*count,input)!=(size_t)2*count)return 4;
#endif
        for(int q=0;q<2*count;q++)source[q]/=steps;
        for(int t=0;t<steps;t++) {
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
            uint64_t before=frame>=29?physics_sim_accepted_fingerprint(&s):0;
            if(!cfd_open_atmosphere3d_step(&s,source,source+count)) {
                fprintf(stderr,"rejected frame=%d step=%d %s\n",frame,t,s.step_rejection);
                bool saved=physics_sim_momentum_capture(&s,source,argv[5],frame,t,before,&budget);
                fclose(input);cfd_memory_free(parent);cfd_memory_free(source);
                cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
                fprintf(stderr,"capture_saved=%d memory_live_bytes=%zu rejected_allocations=%zu\n",saved,budget.live_bytes,budget.rejected_allocations);
                return !saved?7:budget.live_bytes||budget.rejected_allocations?6:5;
            }
#else
            if(!cfd_open_atmosphere3d_step(&s,source,source+count)){fprintf(stderr,"rejected frame=%d step=%d %s\n",frame,t,s.step_rejection);return 5;}
#endif
            maximum_divergence=fmax(maximum_divergence,s.divergence);
            maximum_residual=fmax(maximum_residual,s.residual);
        }
        fprintf(stderr,"frame %d/%d\n",frame+1,windows);fflush(stderr);
    }
    if(fgetc(input)!=EOF)return 4;
    fclose(input);FILE *output=fopen(argv[5],"wbx");if(!output)return 4;
    if(fwrite(s.energy,sizeof(double),(size_t)2*count,output)!=(size_t)2*count || fwrite(s.velocity,sizeof(double),s.velocity_count,output)!=(size_t)s.velocity_count || fclose(output))return 4;
    double totals[2]={0,0},out[2]={0,0},in[2]={0,0},peak=300;
    for(int q=0;q<count;q++){totals[0]+=s.energy[q];totals[1]+=s.smoke[q];peak=fmax(peak,300+s.energy[q]/(1.2*1005*s.grid.volume));}
    for(int a=0;a<2;a++)for(int q=0;q<s.boundary_faces;q++){in[a]+=s.flux[2*a*s.boundary_faces+q];out[a]+=s.flux[(2*a+1)*s.boundary_faces+q];}
    char grid[64];if(n[0]==n[1] && n[1]==n[2])snprintf(grid,sizeof grid,"%d",n[0]);else snprintf(grid,sizeof grid,"[%d,%d,%d]",n[0],n[1],n[2]);
    const char *format="{\"grid\":%s,\"dt_s\":%.17g,\"time_s\":%.17g,\"energy_j\":%.17g,\"smoke_kg\":%.17g,\"heat_balance_j\":%.17g,\"smoke_balance_kg\":%.17g,\"peak_temperature_k\":%.17g,\"divergence\":%.17g,\"pressure_residual\":%.17g}\n";
#ifdef PHYSICS_SIM_FLOW_SPATIAL_QUALIFIER
    #ifdef PHYSICS_SIM_HORIZONTAL_MATCHED_TIME_QUALIFIER
    const char *profile="flow_horizontal_matched_0025_v1";
    #elif defined(PHYSICS_SIM_VERTICAL_MATCHED_TIME_QUALIFIER)
    const char *profile="flow_vertical_matched_0025_v1";
    #elif defined(PHYSICS_SIM_VERTICAL_FLOW_QUALIFIER)
    const char *profile="flow_vertical_fixed_scalar_v1";
    #elif defined(PHYSICS_SIM_FINE_FLOW_TIME_QUALIFIER)
    const char *profile="flow_time_128_v1";
#else
    const char *profile="flow_spatial_fixed_scalar_v1";
#endif
    cfd_memory_free(parent);
#else
    const char *profile="flow_time_64_v1";
#endif
    printf("{\"qualification_profile\":\"%s\",\"scalar_dt_s\":%.17g,\"flow_steps\":%d,\"scalar_cycles\":%llu,\"maximum_divergence\":%.17g,\"maximum_pressure_residual\":%.17g,\"scalar_transport_scheme\":\"muscl_minmod\",\"memory_limit_bytes\":%zu,\"memory_peak_bytes\":%zu,\"scalar_work_cells\":%llu,\"max_scalar_work_cells\":%llu,",profile,PHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT,s.steps,s.scalar_work_cells/count,maximum_divergence,maximum_residual,budget.limit_bytes,budget.peak_bytes,s.scalar_work_cells,s.max_scalar_work_cells);format++;
    printf(format,grid,dt,s.time,totals[0],totals[1],totals[0]-s.input_j-in[0]+out[0],totals[1]-s.input_kg-in[1]+out[1],peak,s.divergence,s.residual);
    cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    fprintf(stderr,"memory_live_bytes=%zu rejected_allocations=%zu\n",budget.live_bytes,budget.rejected_allocations);
    if(budget.live_bytes || budget.rejected_allocations)return 6;
    return 0;
}
