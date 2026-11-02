/* Frozen native boxed-plume gas as initial data, then thermal-only evolution.
 * This is neither a new fluid run nor a live worker/session mutation. */
#include "physics_sim_heated_face.h"
#include "app/cfd_passive3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv) {
    if(argc!=4 && argc!=5)return 2;
    char *end;long side=strtol(argv[1],&end,10);if(*end || (side!=32 && side!=64))return 2;
    double dt=strtod(argv[3],&end);if(*end || (dt!=.1 && dt!=.05 && dt!=.025))return 2;
    double depth=1./side;
    if(argc==5){depth=strtod(argv[4],&end);if(*end || depth!=1./32)return 2;}
    int layers=(int)llround(depth*side);
    CfdMemoryBudget budget={.limit_bytes=256*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s;int n[3]={(int)side,(int)side,(int)side};
    double l[3]={1,1,1},p[2]={0,0},t[2]={300,300};
    if(!cfd_open_atmosphere3d_init(&s,n,l,1.2,.000018,1005,300,.026,0,.0025,p,t,p,false,9.81,1./300,.1))return 3;
    s.ground=true;int lo[3]={3*side/8,3*side/8,3*side/8},hi[3]={5*side/8,5*side/8,5*side/8};
    if(!cfd_open_atmosphere3d_box(&s,lo,hi))return 3;
    if(!strcmp(argv[2],"uniform")) {
        for(int q=0;q<s.grid.count;q++)if(!cfd_open_atmosphere3d_solid(&s,q))
            s.energy[q]=10*1206*s.grid.volume;
    } else {
        FILE *input=fopen(argv[2],"rb");if(!input)return 4;
        if(fread(s.energy,sizeof(double),(size_t)s.grid.count,input)!=(size_t)s.grid.count)return 4;
        fclose(input);
    }
    if(!cfd_open_atmosphere3d_valid(&s))return 5;
    double before=cfd_passive3d_total(s.energy,s.grid.count);
    PhysicsSimHeatedFace face={2,-1,300,100,10,0,0};
    double rise;if(!physics_sim_heated_face_sample(&s,&face,depth,&rise))return 5;
    printf("{\"grid\":%ld,\"dt_s\":%.17g,\"duration_s\":3.2,\"support_depth_m\":%.17g,\"initial_mean_rise_k\":%.17g,\"initial_gas_k\":[",side,dt,depth,rise);
    int emitted=0;
    for(int layer=0;layer<layers;layer++)for(int j=lo[1];j<hi[1];j++)for(int i=lo[0];i<hi[0];i++) {
        int q=((lo[2]-1-layer)*side+j)*side+i;
        printf("%s%.17g",emitted++?",":"",300+s.energy[q]/(1206*s.grid.volume));
    }
    printf("],\"surface_k\":[300");
    for(int step=0;step<(int)llround(3.2/dt);step++) {
        if(!physics_sim_heated_face_exchange_depth(&s,&face,dt,depth))return 6;
        printf(",%.17g",face.temperature_k);
    }
    double after=cfd_passive3d_total(s.energy,s.grid.count);
    printf("],\"final_gas_k\":[");emitted=0;
    for(int layer=0;layer<layers;layer++)for(int j=lo[1];j<hi[1];j++)for(int i=lo[0];i<hi[0];i++) {
        int q=((lo[2]-1-layer)*side+j)*side+i;
        printf("%s%.17g",emitted++?",":"",300+s.energy[q]/(1206*s.grid.volume));
    }
    printf("],\"gas_to_surface_j\":%.17g,\"gas_debit_j\":%.17g,\"stored_change_j\":%.17g,\"energy_residual_j\":%.17g,\"transport_frozen\":true,\"burning_enabled\":false}\n",face.gas_to_surface_j,before-after,100*((face.temperature_k-300)+face.temperature_correction_k),after+face.gas_to_surface_j-before);
    cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    return budget.live_bytes?7:0;
}
