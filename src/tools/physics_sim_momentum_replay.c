#include "physics_sim_momentum_capture.h"
#include "physics_sim_half_flow.h"
#include "app/cfd_open_momentum_diagnostics.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
int main(int argc,char **argv) {
    bool half=argc==5 && !strcmp(argv[4],"half-flow");
    if(argc!=4 && !half)return 2;
    CfdMemoryBudget budget={.limit_bytes=4294967296ULL};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s;
    if(!physics_sim_accepted_load(argv[1],&s))return 3;
    double *source=cfd_memory_malloc((size_t)2*s.grid.count*sizeof(double));if(!source)return 3;
    FILE *f=fopen(argv[2],"rb");if(!f)return 4;
    if(fread(source,8,2*s.grid.count,f)!=(size_t)2*s.grid.count || fgetc(f)!=EOF)return 4;
    fclose(f);uint64_t before=physics_sim_accepted_fingerprint(&s);
    int original_steps=s.steps;double original_time=s.time,original_j=s.input_j,original_kg=s.input_kg;
    unsigned long long original_work=s.scalar_work_cells;
    double source_j=0,source_kg=0;
    for(int q=0;q<s.grid.count;q++){source_j+=source[q];source_kg+=source[s.grid.count+q];}
    if(half && !physics_sim_half_flow(&s,source))return 3;
    bool accepted=true;double courants[2]={0,0},divs[2]={0,0},residuals[2]={0,0};int completed=0;
    for(int i=0;i<(half?2:1);i++) {
        before=physics_sim_accepted_fingerprint(&s);
        if(!cfd_open_atmosphere3d_step(&s,source,source+s.grid.count)){accepted=false;break;}
        courants[i]=cfd_open_momentum_diagnostics.courant;divs[i]=s.divergence;residuals[i]=s.residual;completed++;
    }
    if(half) {
        char path[4096];if(snprintf(path,sizeof path,"%s.retiming.json",argv[3])>=(int)sizeof path)return 4;
        f=fopen(path,"wx");if(!f)return 4;
        fprintf(f,"{\"schema\":\"half_flow_retry_v1\",\"original_steps\":%d,\"original_time_s\":%.17g,\"new_flow_dt_s\":%.17g,\"scalar_dt_s\":%.17g,\"completed_steps\":%d,\"accepted_steps\":%d,\"time_s\":%.17g,\"original_scalar_work_cells\":%llu,\"scalar_work_cells\":%llu,\"original_source_j\":%.17g,\"original_source_kg\":%.17g,\"added_j\":%.17g,\"added_kg\":%.17g,\"courants\":[%.17g,%.17g],\"divergence\":[%.17g,%.17g],\"pressure_residual\":[%.17g,%.17g]}\n",original_steps,original_time,s.dt,PHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT,completed,s.steps,s.time,original_work,s.scalar_work_cells,source_j,source_kg,s.input_j-original_j,s.input_kg-original_kg,courants[0],courants[1],divs[0],divs[1],residuals[0],residuals[1]);
        if(fclose(f))return 4;
    }
    bool saved=physics_sim_momentum_capture(&s,source,argv[3],-1,-1,before,&budget);
    printf("{\"accepted\":%s,\"capture_saved\":%s}\n",accepted?"true":"false",saved?"true":"false");
    cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    fprintf(stderr,"memory_live_bytes=%zu rejected_allocations=%zu\n",budget.live_bytes,budget.rejected_allocations);
    return !saved?4:budget.live_bytes||budget.rejected_allocations?6:accepted?0:5;
}
