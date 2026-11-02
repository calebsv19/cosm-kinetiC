/* Authored five-second uniform-transverse warm-gas/solid demonstrator.
 * wall-refined selects a512normal-cell solve and one-step wall coupling.
 * Accepted state is State; gas and matrix are scratch. No live session/default
 * solver adoption. Native wall batches feed exact signed-J microstep packets. */
#include "receiver.h"
#include "inert.h"
#include "physics_sim_heated_face.h"
#include "physics_sim_local_matrix.h"
#include "physics_sim_normal_conservative.h"
#include "physics_sim_thermal_step.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int normal_cells=256;
typedef PhysicsSimThermalState State;
typedef PhysicsSimThermalTrace Trace;
static double inventory(const State *s) {
    double e=0; for(int k=0;k<normal_cells;k++) e+=s->v[k]*(1206*.0625*.0625/normal_cells); return e;
}
static double solid_energy(const State *s) {
    double e=0; for(int x=0;x<4;x++)for(int d=0;d<2;d++)e+=s->r[x].solid.native.energy_j[d]; return e;
}
static void observe(const State *s) {
    double e[2]={0},q=0;for(int x=0;x<4;x++) {q+=s->r[x].received_q_j;for(int d=0;d<2;d++)e[d]+=s->r[x].solid.native.energy_j[d];}
    printf("{\"time_s\":%.17g,\"q_j\":%.17g,\"gas_mean_k\":%.17g,\"solid0_k\":%.17g,\"solid1_k\":%.17g,\"source_j\":%.17g,\"profile_k\":[",s->tick/(double)TPS,q,inventory(s)/4.7109375,e[0]/50,e[1]/50,s->source);
    /* Preserve original physical cell-average supports for all profile gates. */
    for(int k=0;k<256;k++){double value=0;for(int j=k*normal_cells/256;j<(k+1)*normal_cells/256;j++)value+=s->v[j]/(normal_cells/256);printf("%s%.17g",k?",":"",value);}
    if(normal_cells==512){printf("],\"native_profile_k\":[");for(int k=0;k<normal_cells;k++)printf("%s%.17g",k?",":"",s->v[k]);}
    puts("]}");
}
static bool stage(const State *s,State *out,CfdOpenAtmosphere3d *gas,
                  PhysicsSimLocalMatrix *matrix,int ticks,int scalar_factor,int mode,int fail,Trace *trace) {
    return physics_sim_thermal_step(s,out,gas,matrix,ticks,scalar_factor,mode,normal_cells,fail,trace);
}

int main(int argc,char **argv) {
    if(argc!=3&&argc!=4&&argc!=5&&argc!=6)return 2;
    int mode=atoi(argv[1]);if(mode<0||mode>2)return 2;
    /* Optional fixed-grid diagnostic only: reversal to20ms. Three-argument
     * default retains the approved five-second fixture and its original clocks. */
    int scalar_factor=1,window=WINDOW,end=END;
    if(argc==4) {
        if(strcmp(argv[3],"wall-refined"))return 2;
        normal_cells=512;window=MICRO;
    }
    if(argc==5||argc==6) {
        scalar_factor=atoi(argv[3]);int receiver_steps=atoi(argv[4]);
        if(mode!=2||(scalar_factor!=1&&scalar_factor!=2&&scalar_factor!=4)||
           (receiver_steps!=1&&receiver_steps!=2&&receiver_steps!=5))return 2;
        window=receiver_steps*MICRO;end=TPS/50;
        if(argc==6){normal_cells=atoi(argv[5]);if(normal_cells!=256&&normal_cells!=512)return 2;}
    }
    FILE *trace=fopen(argv[2],"wx");assert(trace);setvbuf(stdout,NULL,_IOLBF,0);
    CfdMemoryBudget budget={.limit_bytes=16*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d gas={0};int cells[3]={4,4,normal_cells};double lengths[3]={.25,.25,.0625},zero[2]={0,0},temp[2]={300,300};
    assert(cfd_open_atmosphere3d_init(&gas,cells,lengths,1.2,1.8e-5,1005,300,.026,0,1./6400,zero,temp,zero,false,0,0,.1));gas.ground=true;
    for(int i=0;i<gas.grid.count;i++)gas.velocity[i]=.05*(i/16+.5)/normal_cells;
    double width[MAX_NORMAL];for(int k=0;k<normal_cells;k++)width[k]=.0625/normal_cells;
    PhysicsSimLocalMatrix matrix={0};assert(physics_sim_local_matrix_init(&matrix,normal_cells,width,0,.026/1206,(normal_cells==512?.5:1)/(TPS*scalar_factor),false));
    State s={0},next={0};for(int k=0;k<normal_cells;k++)s.v[k]=mode==2?0:20;
    ThermalReceiverConfig c={0};c.version=c.quantity=c.unit=c.sign=1;c.cells=2;c.reference_k=300;c.max_k=321;c.area_m2=.0625/4;
    strcpy(c.fixture_sha256,COUPLED_FIXTURE_ID);strcpy(c.producer_sha256,COUPLED_SOURCE_ID);strcpy(c.support_sha256,COUPLED_SUPPORT_ID);
    c.face_hi_m[1]=.25;c.timebase=(CoreSimTimebase){717,6400};c.total_steps=32000;c.window_steps=1;
    for(int d=0;d<2;d++){c.width_m[d]=.005;c.cv_j_m3_k[d]=160000;c.conductivity_w_m_k[d]=.16;c.initial_k[d]=mode==2?320:300;}
    for(int x=0;x<4;x++){c.face_lo_m[0]=x*.25/4;c.face_hi_m[0]=(x+1)*.25/4;assert(thermal_receiver_init(&c,&s.r[x]));}
    const double initial=inventory(&s)+solid_energy(&s);double worst=0,debit_error=0,solid_error=0,allocation_error=0,min_q=INFINITY,max_q=-INFINITY;
    int refusals=0,windows=0,partials=0;observe(&s);
    while(s.tick<end) {
        int ticks=window,remaining=OBS-s.tick%OBS;if(ticks>remaining)ticks=remaining;assert(ticks%MICRO==0);
        Trace tr;
        if(s.tick==0||s.tick==TPS||s.tick==4*TPS) {
            State saved=s,untouched=next;
            for(int fail=1;fail<=2;fail++) {assert(!stage(&s,&next,&gas,&matrix,ticks,scalar_factor,mode,fail,&tr));assert(!memcmp(&s,&saved,sizeof s));assert(!memcmp(&next,&untouched,sizeof next));refusals++;}
            ThermalReceiverExchange bad;ThermalReceiverReceipt receipt={0},unchanged=receipt;ThermalReceiver rejected=s.r[0],saved_r=rejected;
            CoreSimTimeInterval interval={s.r[0].solid.time,{s.r[0].config.timebase,s.r[0].solid.time.ticks+1}};
            assert(thermal_receiver_exchange_make(&s.r[0],interval,1,0,&bad));bad.q_j=NAN;
            assert(!thermal_receiver_stage(&s.r[0],&bad,interval.end,&rejected,&receipt));assert(!memcmp(&rejected,&saved_r,sizeof rejected)&&!memcmp(&receipt,&unchanged,sizeof receipt));refusals++;
        }
        assert(stage(&s,&next,&gas,&matrix,ticks,scalar_factor,mode,0,&tr));
        debit_error=fmax(debit_error,fabs(tr.debit-tr.q));solid_error=fmax(solid_error,fabs(tr.solid-tr.q));allocation_error=fmax(allocation_error,fabs(tr.allocation-tr.q));
        assert(fabs(tr.debit-tr.q)<=128*DBL_EPSILON*(1+fabs(tr.q)));
        assert(fabs(tr.solid-tr.q)<=128*DBL_EPSILON*(1+solid_energy(&s)+fabs(tr.q)));
        assert(fabs(tr.allocation-tr.q)<=32*DBL_EPSILON*fabs(tr.q));
        min_q=fmin(min_q,tr.q);max_q=fmax(max_q,tr.q);
        fprintf(trace,"%d %d %a",next.tick,tr.micro,tr.q);
        for(int m=0;m<tr.micro;m++)fprintf(trace," %a %a %a",tr.amounts[m],tr.energy[m][0],tr.energy[m][1]);fputc('\n',trace);
        s=next;windows++;if(ticks!=window)partials++;
        worst=fmax(worst,fabs(inventory(&s)+solid_energy(&s)-initial-s.source));assert(worst/(initial+4.7109375)<=1e-8);
        if(!(s.tick%OBS))observe(&s);
    }
    double velocity=0;for(int a=0;a<3;a++)for(int j=0;j<gas.grid.count+(a==2?gas.plane:0);j++)velocity=fmax(velocity,fabs(gas.velocity[a*gas.grid.count+j]-(a==0?.05*(j/16+.5)/normal_cells:0)));assert(velocity==0);
    for(int x=0;x<4;x++)assert(s.r[x].solid.completed_steps==(uint64_t)end/MICRO);
    physics_sim_local_matrix_destroy(&matrix);cfd_open_atmosphere3d_destroy(&gas);cfd_memory_scope(prior);assert(!budget.live_bytes);assert(fclose(trace)==0);
    printf("{\"terminal\":true,\"mode\":%d,\"normal_cells\":%d,\"scalar_steps\":%d,\"receiver_steps\":%d,\"windows\":%d,\"event_split_windows\":%d,\"refusals\":%d,\"worst_balance_j\":%.17g,\"debit_error_j\":%.17g,\"solid_error_j\":%.17g,\"allocation_error_j\":%.17g,\"min_window_q_j\":%.17g,\"max_window_q_j\":%.17g,\"velocity_error\":%.17g,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu}\n",mode,normal_cells,end*scalar_factor,end/MICRO,windows,partials,refusals,worst,debit_error,solid_error,allocation_error,min_q,max_q,velocity,budget.peak_bytes,budget.live_bytes);
    return 0;
}
