/* Local entrance/corner qualification of the native wall option. Coordinates
 * are the original unit-room box translated by (-.25,-.25,0), with a .5 m
 * local open domain. Public native stepping and forcing own all evolution. */
#include "../app/cfd_open_atmosphere3d.c"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static size_t accepted_copy(const CfdOpenAtmosphere3d *s,unsigned char *out) {
    size_t used=0;
#define COPY(pointer,bytes) do {memcpy(out+used,(pointer),(bytes));used+=(bytes);} while(0)
    COPY(s->velocity,(size_t)s->velocity_count*sizeof(double));
    COPY(s->pressure,(size_t)3*s->grid.count*sizeof(double));
    COPY(s->flux,(size_t)4*s->boundary_faces*sizeof(double));
    COPY(s->kinetic_ledger,sizeof(s->kinetic_ledger));
    double values[]={s->time,s->initial_j,s->initial_kg,s->input_j,s->input_kg,
        s->divergence,s->residual,s->wind_air_in,s->wind_air_out,s->wind_inlet_volume,
        s->weather_state.external_work_j,s->weather_state.air_in_m3,s->weather_state.air_out_m3};
    COPY(values,sizeof(values));COPY(&s->steps,sizeof(s->steps));
    COPY(&s->scalar_work_cells,sizeof(s->scalar_work_cells));
#undef COPY
    return used;
}

static uint64_t checksum(const unsigned char *data,size_t size) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<size;i++){hash^=data[i];hash*=UINT64_C(1099511628211);}
    return hash;
}

static double gap_flow(const CfdOpenAtmosphere3d *s,int i) {
    double total=0;
    for(int k=0;k<s->grid.n[2]/8;k++)for(int j=8;j<24;j++)
        total+=s->velocity[cfd_open_atmosphere3d_face(s,0,i,j,k)]*s->grid.h[1]*s->grid.h[2];
    return total;
}

int main(int argc,char **argv) {
    if(argc!=7)return 2;
    int gap=atoi(argv[1]),candidate=atoi(argv[2]),direction=atoi(argv[3]),muscl=atoi(argv[4]),faults=atoi(argv[6]);
    double dt=strtod(argv[5],NULL);
    if((gap!=4 && gap!=8) || candidate<0 || candidate>1 || direction < -1 || direction>1 ||
       muscl<0 || muscl>1 || (dt!=.005 && dt!=.0025) || faults<0 || faults>1)return 2;
    int n[3]={32,32,8*gap},lo[3]={8,8,gap},hi[3]={24,24,5*gap},knots[1]={0};
    double length[3]={.5,.5,.5},pressure[2]={0},ambient[2]={300,300};
    double target[1]={direction*.25},strength[1]={1};
    CfdMemoryBudget budget={.limit_bytes=512*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s={0};double *zero=NULL;unsigned char *before=NULL,*after=NULL;int status=1;
    if(!cfd_open_atmosphere3d_init(&s,n,length,1.2,1.8e-5,1005,300,.026,0,dt,
                                  pressure,ambient,pressure,true,9.81,1./300,.1))goto done;
    s.ground=true;s.cache_projection=true;s.muscl=muscl;s.scalar_mc=true;s.scalar_rk2=true;
    s.max_scalar_cycles=4;s.viscous_wall_curvature=candidate;
    if(!cfd_open_atmosphere3d_lateral(&s) || !cfd_open_atmosphere3d_box(&s,lo,hi) ||
       !cfd_open_weather_configure(&s,&s.weather,.25,.1,false,1,knots,target,strength))goto done;
    s.wind.quiescent_backflow=true;
    zero=cfd_memory_calloc((size_t)s.grid.count,sizeof(double));
    size_t snapshot_bytes=((size_t)s.velocity_count+3*s.grid.count+4*s.boundary_faces+64)*sizeof(double);
    before=cfd_memory_malloc(snapshot_bytes);after=cfd_memory_malloc(snapshot_bytes);
    if(!zero || !before || !after)goto done;
    int seeded=0;
    for(int k=0;k<gap;k++)for(int j=8;j<24;j++)for(int i=6;i<8;i++) {
        int x=direction<0?31-i:i,q=(k*32+j)*32+x;
        s.smoke[q]=1;s.energy[q]=direction?1:0;seeded++;
    }
    for(int q=0;q<s.grid.count;q++) {
        s.smoke[q]*=1e-6/seeded;s.energy[q]*=.02/seeded;
        s.initial_kg+=s.smoke[q];s.initial_j+=s.energy[q];
    }
    /* Audit every selected wall stencil before numerical evolution. */
    unsigned curved_stencils=0;
    if(candidate)for(int a=0;a<3;a++)for(int k=0;k<n[2]+(a==2);k++)
    for(int j=0;j<32+(a==1);j++)for(int i=0;i<32+(a==0);i++) {
        if(cfd_open_atmosphere3d_blocked(&s,a,i,j,k) || (a==2 && k==0))continue;
        int p[3]={i,j,k};
        for(int b=0;b<3;b++)for(int side=-1;side<=1;side+=2) {
            double value=0;bool curved=false;
            if(!viscous_wall_sample(&s,a,b,p,side,&value,&curved)) {
                fprintf(stderr,"stencil refused at a=%d i=%d j=%d k=%d b=%d side=%d\n",a,i,j,k,b,side);goto done;
            }
            curved_stencils+=curved;
        }
    }
    double max_div=0,max_courant=0,max_heat_defect=0,max_smoke_defect=0,max_kinetic_defect=0,max_speed=0;
    int steps=(int)lround(.1/dt);
    for(int step=0;step<steps;step++) {
        if(!cfd_open_atmosphere3d_step(&s,zero,zero)) {
            fprintf(stderr,"step %d refused: %s\n",step,s.step_rejection);goto done;
        }
        if(!cfd_open_atmosphere3d_valid(&s))goto done;
        double sums[2]={0},in[2]={0},out[2]={0};
        for(int q=0;q<s.grid.count;q++){sums[0]+=s.energy[q];sums[1]+=s.smoke[q];}
        for(int q=0;q<s.boundary_faces;q++)for(int a=0;a<2;a++) {
            in[a]+=s.flux[2*a*s.boundary_faces+q];out[a]+=s.flux[(2*a+1)*s.boundary_faces+q];
        }
        double heat=fabs(sums[0]-s.initial_j+out[0]-in[0]);
        double smoke=fabs(sums[1]-s.initial_kg+out[1]-in[1]);
        double ledger=s.weather_state.external_work_j;
        for(int a=0;a<10;a++)ledger+=s.kinetic_ledger[a];
        double kinetic=fabs(cfd_open_kinetic(&s,s.velocity)-ledger);
        if(heat>1e-10 || smoke>1e-18 || kinetic>1e-10 || s.divergence>1e-8 || s.residual>1e-11 ||
           s.momentum_courant>(muscl?.4:.8) || s.kinetic_ledger[KE_VISCOSITY]>1e-12) {
            fprintf(stderr,"invariant step %d: heat=%g smoke=%g kinetic=%g div=%g res=%g CFL=%g viscous=%g\n",
                    step,heat,smoke,kinetic,s.divergence,s.residual,s.momentum_courant,s.kinetic_ledger[KE_VISCOSITY]);
            goto done;
        }
        for(int q=0;q<s.velocity_count;q++)max_speed=fmax(max_speed,fabs(s.velocity[q]));
        max_div=fmax(max_div,s.divergence);max_courant=fmax(max_courant,s.momentum_courant);
        max_heat_defect=fmax(max_heat_defect,heat);max_smoke_defect=fmax(max_smoke_defect,smoke);
        max_kinetic_defect=fmax(max_kinetic_defect,kinetic);
    }
    double entry=gap_flow(&s,8),exit=gap_flow(&s,24),kinetic=cfd_open_kinetic(&s,s.velocity);
    if(direction==0 && (max_speed!=0 || entry!=0 || exit!=0))goto done;
    size_t bytes=accepted_copy(&s,before);
    if(bytes>snapshot_bytes)goto done;
    unsigned rejected=0;
    if(faults) {
        zero[0]=NAN;
        if(cfd_open_atmosphere3d_step(&s,zero,zero))goto done;
        zero[0]=0;accepted_copy(&s,after);if(memcmp(before,after,bytes))goto done;rejected++;
        unsigned long long cap=s.max_scalar_work_cells;s.max_scalar_work_cells=s.scalar_work_cells;
        if(cfd_open_atmosphere3d_step(&s,zero,zero))goto done;
        s.max_scalar_work_cells=cap;accepted_copy(&s,after);if(memcmp(before,after,bytes))goto done;rejected++;
        if(direction) {
            double old=s.weather.target_m_s[0];s.weather.target_m_s[0]=10;
            if(cfd_open_atmosphere3d_step(&s,zero,zero))goto done;
            s.weather.target_m_s[0]=old;accepted_copy(&s,after);if(memcmp(before,after,bytes))goto done;rejected++;
        }
    }
    if(!cfd_open_atmosphere3d_step(&s,zero,zero))goto done;
    accepted_copy(&s,after);
    printf("{\"gap_layers\":%d,\"candidate\":%d,\"direction\":%d,\"muscl\":%d,\"dt_s\":%.17g,"
           "\"time_s\":0.1,\"curved_stencils\":%u,\"entry_m3_s\":%.17g,\"exit_m3_s\":%.17g,"
           "\"kinetic_j\":%.17g,\"max_speed_m_s\":%.17g,\"max_divergence\":%.17g,\"max_courant\":%.17g,"
           "\"heat_defect_j\":%.17g,\"smoke_defect_kg\":%.17g,\"kinetic_defect_j\":%.17g,"
           "\"rejected_steps\":%u,\"rollback_exact\":true,\"continued_steps\":%d,\"continued_checksum\":\"%016llx\","
           "\"peak_allocated_bytes\":%zu}\n",
           gap,candidate,direction,muscl,dt,curved_stencils,entry,exit,kinetic,max_speed,max_div,max_courant,
           max_heat_defect,max_smoke_defect,max_kinetic_defect,rejected,s.steps,
           (unsigned long long)checksum(after,bytes),budget.peak_bytes);status=0;
done:
    cfd_memory_free(zero);cfd_memory_free(before);cfd_memory_free(after);cfd_open_atmosphere3d_destroy(&s);
    if(budget.live_bytes)status=1;cfd_memory_scope(prior);return status;
}
