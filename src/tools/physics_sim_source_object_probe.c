/* Controlled SI heater -> resolved gas transport -> authored inert exchange.
 * No initial thermal excess or direct source deposition in exchange support.
 * This diagnostic is not native Fire or a durable thermal participant. */
#include "physics_sim_inert_coupling.h"
#include "app/cfd_passive3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static double total_outflow(const CfdOpenAtmosphere3d *s) {
    double sum=0;
    for(int i=0;i<s->boundary_faces;i++)sum+=s->flux[s->boundary_faces+i]-s->flux[i];
    return sum;
}

int main(int argc,char **argv) {
    if(argc!=5)return 2;
    char *end=NULL;
    long side=strtol(argv[1],&end,10);if(*end || (side!=16 && side!=32))return 2;
    double dt=strtod(argv[2],&end);if(*end || (dt!=.005 && dt!=.0025))return 2;
    long mode=strtol(argv[3],&end,10);if(*end || mode<0 || mode>3)return 2;
    /* 0 moving source/exchange;1 source off;2 exchange off;3 wind off. */
    const double horizon=1.2,depth=.125,power=100,capacity=10;
    int ticks=(int)llround(horizon/dt),n[3]={(int)side,(int)side,(int)side};
    int lo[3]={3*side/8,3*side/8,3*side/8};
    int hi[3]={5*side/8,5*side/8,5*side/8};
    int aperture_lo[2]={0,0},aperture_hi[2]={(int)side,(int)side},knots[1]={0};
    double lengths[3]={1,1,1},p[2]={0},ambient[2]={300,300},speed[1]={mode==3?0:.5};
    CfdMemoryBudget budget={.limit_bytes=512*1024*1024};
    CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d gas={0};PhysicsSimHeatedFace face={0,-1,300,capacity,mode==2?0:2,0,0};
    double *source=NULL,*zero=NULL;FILE *csv=NULL;int status=1;
    if(!cfd_open_atmosphere3d_init(&gas,n,lengths,1.2,1.8e-5,1005,300,.026,0,
        dt,p,ambient,p,false,9.81,1./300,.1))goto done;
    gas.ground=true;gas.cache_projection=true;
    if(!cfd_open_atmosphere3d_lateral(&gas) ||
       !cfd_open_wind_configure(&gas,0,.25,false,1,knots,speed) ||
       !cfd_open_wind_patch(&gas,aperture_lo,aperture_hi) ||
       !cfd_open_atmosphere3d_box(&gas,lo,hi))goto done;
    gas.wind.quiescent_backflow=true;
    source=cfd_memory_calloc((size_t)gas.grid.count,sizeof(double));
    zero=cfd_memory_calloc((size_t)gas.grid.count,sizeof(double));
    if(!source || !zero)goto done;
    /* Bounds are physical and aligned at both grids. Support starts at .25m;
     * source ends at .1875m, leaving .0625m without direct heating. */
    int source_lo=(int)side/8,source_hi=3*(int)side/16;
    int source_count=(source_hi-source_lo)*(hi[1]-lo[1])*(hi[2]-lo[2]);
    if(source_count<=0 || source_hi>lo[0]-(int)llround(depth*side))goto done;
    csv=fopen(argv[4],"wx");if(!csv)goto done;
    fprintf(csv,"time_s,input_j,gas_j,outflow_j,support_rise_k,surface_rise_k,gas_to_surface_j,balance_j\n");
    fprintf(csv,"0,0,0,0,0,0,0,0\n");
    double peak_support=0,max_balance=0,max_object_defect=0,first_support=-1;
    for(int step=0;step<ticks;step++) {
        double add=(mode!=1 && step<(int)llround(.4/dt))?power*dt/source_count:0;
        for(int k=lo[2];k<hi[2];k++)for(int j=lo[1];j<hi[1];j++)
            for(int i=source_lo;i<source_hi;i++) {
                int q=((k*(int)side)+j)*(int)side+i;
                if(cfd_open_atmosphere3d_solid(&gas,q))goto done;
                source[q]=add;
            }
        if(!physics_sim_inert_coupling_step(&gas,&face,source,zero,depth,false)) {
            fprintf(stderr,"source-object refused step %d: %s\n",step,gas.step_rejection);goto done;
        }
        double rise;
        if(!physics_sim_heated_face_sample(&gas,&face,depth,&rise))goto done;
        double surface=(face.temperature_k-300)+face.temperature_correction_k;
        double energy=cfd_passive3d_total(gas.energy,gas.grid.count),out=total_outflow(&gas);
        double balance=energy+capacity*surface+out-gas.input_j;
        peak_support=fmax(peak_support,rise);max_balance=fmax(max_balance,fabs(balance));
        max_object_defect=fmax(max_object_defect,fabs(capacity*surface-face.gas_to_surface_j));
        if(first_support<0 && rise>=.001)first_support=gas.time;
        fprintf(csv,"%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n",
            gas.time,gas.input_j,energy,out,rise,surface,face.gas_to_surface_j,balance);
    }
    double surface=(face.temperature_k-300)+face.temperature_correction_k;
    printf("{\"grid\":%ld,\"dt_s\":%.17g,\"mode\":%ld,\"input_j\":%.17g,"
        "\"peak_support_rise_k\":%.17g,\"surface_rise_k\":%.17g,"
        "\"gas_to_surface_j\":%.17g,\"max_balance_j\":%.17g,"
        "\"max_object_defect_j\":%.17g,\"first_support_1mk_s\":%.17g,"
        "\"source_support_gap_m\":0.0625,\"source_cells\":%d,\"peak_bytes\":%zu}\n",
        side,dt,mode,gas.input_j,peak_support,surface,face.gas_to_surface_j,
        max_balance,max_object_defect,first_support,source_count,budget.peak_bytes);
    if(max_balance>1e-7 || max_object_defect>1e-9 || fabs(gas.input_j-(mode==1?0:40))>1e-10)goto done;
    if(mode==1 && (fabs(surface)>1e-12 || fabs(peak_support)>1e-12))goto done;
    if(mode==2 && (surface!=0 || face.gas_to_surface_j!=0))goto done;
    if((mode==0 || mode==2) && peak_support<.1)goto done;
    if(mode==0 && surface<.001)goto done;
    status=0;
done:
    if(csv)fclose(csv);
    cfd_memory_free(source);cfd_memory_free(zero);cfd_open_atmosphere3d_destroy(&gas);
    if(budget.live_bytes)status=1;
    cfd_memory_scope(prior);return status;
}
