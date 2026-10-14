#include "app/cfd_hotgas_domain3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    double mixtures[6][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1},
                           {3.76,1,0,0},{5.64,0,1,2}};
    double temperatures[]={200,298.15,300,499.999999,500,500.000001,
                            999.999999,1000,1000.000001,1300,1500};
    printf("[");
    for (int m=0; m<6; ++m) {
        CfdHotGasCaloric model, before; assert(cfd_hotgas_caloric_init(&model,mixtures[m]));
        before=model; double invalid[4]={-1,0,0,0};
        assert(!cfd_hotgas_caloric_init(&model,invalid)); assert(memcmp(&model,&before,sizeof(model))==0);
        double ignored=17, cp=19;
        assert(!cfd_hotgas_caloric_properties(&model,199,&ignored,&cp)); assert(ignored==17 && cp==19);
        assert(!cfd_hotgas_caloric_temperature(&model,1e10,&ignored)); assert(ignored==17);
        for (int k=0; k<11; ++k) {
            double t=temperatures[k],e,capacity,recovered;
            assert(cfd_hotgas_caloric_properties(&model,t,&e,&capacity));
            assert(cfd_hotgas_caloric_temperature(&model,e,&recovered)); assert(fabs(recovered-t)<1e-8);
            printf("%s{\"mixture\":%d,\"temperature\":%.17g,\"energy\":%.17g,\"cp\":%.17g}",
                   m || k ? "," : "",m,t,e,capacity);
        }
        for (int join=500; join<=1000; join+=500) {
            double e0,e1,c0,c1;
            assert(cfd_hotgas_caloric_properties(&model,join-1e-7,&e0,&c0));
            assert(cfd_hotgas_caloric_properties(&model,join+1e-7,&e1,&c1));
            assert(fabs(c1-c0)<1e-5);
            assert(fabs((e1-e0)/(2e-7)-(c0-model.gas_constant))<.02);
        }
    }
    printf("]\n");
    CfdHotGas3d gas; int n[3]={4,4,4}; double length[3]={1,1,1};
    assert(cfd_hotgas3d_init(&gas,n,length,1.4,287));
    assert(cfd_hotgas_caloric_init(&gas.caloric,mixtures[5]));
    gas.gas_constant_j_kg_k=gas.caloric.gas_constant;
    double e,cp; assert(cfd_hotgas_caloric_properties(&gas.caloric,298.15,&e,&cp));
    gas.gamma=cp/(cp-gas.gas_constant_j_kg_k);
    assert(cfd_hotgas_caloric_properties(&gas.caloric,300,&e,&cp));
    double source[320]={0},accepted[320];
    for (int q=0; q<64; ++q) {gas.state[5*q]=1;gas.state[5*q+4]=e;source[5*q+4]=10000;}
    CfdHotGasDomain d={0};d.contact_flux=3;
    for (int f=0; f<6; ++f) d.face[f].kind=CFD_HOTGAS_SLIP_WALL;
    CfdHotGasDomainBudget budget;
    assert(cfd_hotgas_domain3d_step(&gas,&d,source,1e-6,&budget));
    for (int q=0; q<64; ++q) assert(fabs(gas.state[5*q+4]-e-10000)<1e-8);
    memcpy(accepted,gas.state,sizeof(accepted));
    CfdHotGasDomainBudget before;memset(&budget,0xa5,sizeof(budget));memcpy(&before,&budget,sizeof(budget));
    for (int q=0; q<64; ++q) source[5*q+4]=1e9;
    assert(!cfd_hotgas_domain3d_step(&gas,&d,source,1e-6,&budget));
    assert(memcmp(accepted,gas.state,sizeof(accepted))==0 && memcmp(&before,&budget,sizeof(budget))==0);
    cfd_hotgas3d_destroy(&gas);return 0;
}
