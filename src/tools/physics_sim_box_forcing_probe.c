/* Bounded current-operator boxed forcing, fixed physical support, no thermal
 * debit. Original full source frames are read-only. Native outputs are private
 * qualification carriers, not ordinary worker protocols. */
#include "physics_sim_heated_face.h"
#include "app/cfd_passive3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
/* A supervisor requests a bounded stop; finish the current atomic step, then
 * preserve accepted fields and numerical closure instead of losing the run. */
static volatile sig_atomic_t stop_requested;
static void request_stop(int signal_number) {(void)signal_number;stop_requested=1;}
#ifndef PHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT
#error Fixed scalar timing is required
#endif

static bool observe(const CfdOpenAtmosphere3d *s) {
    PhysicsSimHeatedFace f={2,-1,300,100,10,0,0};double rise;
    if(!physics_sim_heated_face_sample(s,&f,1./32,&rise))return false;
    printf("{\"time_s\":%.17g,\"support_mean_rise_k\":%.17g,\"patch_rise_k\":[",s->time,rise);
    int n=s->grid.n[0],ratio=n/32;
    for(int j=0;j<8;j++)for(int i=0;i<8;i++) {
        double e=0;
        for(int z=11*ratio;z<12*ratio;z++)for(int y=(12+j)*ratio;y<(13+j)*ratio;y++)for(int x=(12+i)*ratio;x<(13+i)*ratio;x++)
            e+=s->energy[(z*n+y)*n+x];
        printf("%s%.17g",i || j?",":"",e/(1206./32768));
    }
    printf("]}\n");fflush(stdout);return true;
}

int main(int argc,char **argv) {
    if(argc<4 || argc>6)return 2;
    if(argc==6 && strcmp(argv[5],"mc"))return 2;
    if(signal(SIGTERM,request_stop)==SIG_ERR || signal(SIGINT,request_stop)==SIG_ERR)return 2;
    char *end;long side=strtol(argv[1],&end,10);
    if(*end || (side!=32 && side!=64))return 2;
    long sample_steps=10;
    if(argc>=5){sample_steps=strtol(argv[4],&end,10);if(*end || (sample_steps!=2 && sample_steps!=5 && sample_steps!=10))return 2;}
    int n[3]={(int)side,(int)side,(int)side},lo[3],hi[3];
    for(int a=0;a<3;a++){lo[a]=3*n[a]/8;hi[a]=5*n[a]/8;}
    double l[3]={1,1,1},p[2]={0,0},t[2]={300,300};
    CfdMemoryBudget budget={.limit_bytes=1073741824};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s;
    if(!cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,.026,.0005,.0025,p,t,p,true,9.81,1./300,.1))return 3;
    s.ground=true;s.muscl=true;s.cache_projection=true;s.max_scalar_work_cells=1000000000;
    s.scalar_mc=argc==6;
    if(!cfd_open_atmosphere3d_lateral(&s) || !cfd_open_atmosphere3d_box(&s,lo,hi))return 3;
    FILE *input=fopen(argv[2],"rb");if(!input)return 4;
    double *source=cfd_memory_calloc((size_t)2*s.grid.count,sizeof(double));if(!source)return 3;
    if(!observe(&s))return 5;
    for(int frame=0;frame<32 && !stop_requested;frame++) {
        if(fread(source,sizeof(double),(size_t)2*s.grid.count,input)!=(size_t)2*s.grid.count)return 4;
        for(int q=0;q<2*s.grid.count;q++)source[q]/=40;
        for(int tick=0;tick<40 && !stop_requested;tick++) {
            if(!cfd_open_atmosphere3d_step(&s,source,source+s.grid.count)) {
                fprintf(stderr,"refused frame %d step %d: %s\n",frame,tick,s.step_rejection);return 5;
            }
            if((tick+1)%sample_steps==0 && !observe(&s))return 5;
        }
        fprintf(stderr,"window %d/32\n",frame+1);fflush(stderr);
    }
    if(s.steps==1280 && fgetc(input)!=EOF)return 4;
    fclose(input);FILE *output=fopen(argv[3],"wb");if(!output)return 4;
    if(fwrite(s.energy,sizeof(double),(size_t)2*s.grid.count,output)!=(size_t)2*s.grid.count ||
       fwrite(s.velocity,sizeof(double),(size_t)s.velocity_count,output)!=(size_t)s.velocity_count || fclose(output))return 4;
    double heat=cfd_passive3d_total(s.energy,s.grid.count);
    double incoming=cfd_passive3d_total(s.flux,s.boundary_faces),outgoing=cfd_passive3d_total(s.flux+s.boundary_faces,s.boundary_faces);
    int complete=s.steps==1280;
    printf("{\"terminal\":true,\"complete\":%s,\"scalar_mc\":%s,\"grid\":%ld,\"time_s\":%.17g,\"flow_steps\":%d,\"flow_dt_s\":0.0025,\"scalar_dt_s\":0.00125,\"observation_dt_s\":%.17g,\"scalar_work_cells\":%llu,\"heat_balance_j\":%.17g,\"maximum_final_divergence\":%.17g,\"final_pressure_residual\":%.17g,\"memory_peak_bytes\":%zu",complete?"true":"false",s.scalar_mc?"true":"false",side,s.time,s.steps,sample_steps*.0025,s.scalar_work_cells,heat-s.input_j-incoming+outgoing,s.divergence,s.residual,budget.peak_bytes);
    cfd_memory_free(source);cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    printf(",\"memory_live_bytes\":%zu,\"rejected_allocations\":%zu}\n",budget.live_bytes,budget.rejected_allocations);
    return budget.live_bytes || budget.rejected_allocations?6:complete?0:7;
}
