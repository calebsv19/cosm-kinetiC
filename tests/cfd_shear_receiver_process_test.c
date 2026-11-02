#include "physics_sim_receiver_client.h"
#include "physics_sim_shear_receiver_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
int main(int argc,char **argv){
    assert(argc==3);FILE *f=fopen(argv[2],"r");assert(f);char header[512];assert(fgets(header,sizeof header,f));
    int nx,nz,window,steps,rate;CHECK(sscanf(header,"columns_solid_v2 %d %d %d %d %d",&nx,&nz,&window,&steps,&rate)==5);CHECK(nx==128&&nz==256&&window==5&&steps==3200&&rate==6400);
    ThermalReceiver *accepted=calloc(128,sizeof *accepted);assert(accepted);
    for(int x=0;x<128;x++){CHECK(physics_sim_shear_receiver_init(x,steps,&accepted[x]));CHECK(accepted[x].config.area_m2==.0625/128);CHECK(accepted[x].config.face_lo_m[0]==x*.25/128);CHECK(accepted[x].config.face_hi_m[0]==(x+1)*.25/128);}
    PhysicsSimReceiverClient client={0};CHECK(!physics_sim_receiver_client_open_profile(&client,argv[1],"shear256-319"));CHECK(physics_sim_receiver_client_open_profile(&client,argv[1],"shear256-3200"));
    /* Spatial gas work can exceed the old five-second request-idle budget. */
    sleep(6);
    unsigned cold=0;for(int row=0;row<640;row++){
        unsigned long long tick;CHECK(fscanf(f,"%llu",&tick)==1);CHECK(tick==(unsigned long long)(row+1)*5);
        for(int x=0;x<128;x++){
            double value[10];for(int j=0;j<10;j++)CHECK(fscanf(f,"%la",&value[j])==1);
            ThermalReceiver *base=&accepted[x],next,reference;ThermalReceiverReceipt receipt,oracle;
            CoreSimTimeInterval interval={base->solid.time,{base->config.timebase,tick}};ThermalReceiverExchange packet;
            CHECK(thermal_receiver_exchange_make(base,interval,tick-4,value[0],&packet));
            CHECK(physics_sim_receiver_client_stage(&client,x,base,&packet,interval.end,&next,&receipt));
            CHECK(thermal_receiver_stage(base,&packet,interval.end,&reference,&oracle));
            CHECK(!memcmp(&next,&reference,sizeof next));CHECK(!memcmp(&receipt,&oracle,sizeof receipt));
            CHECK(next.solid.native.energy_j[0]==value[3]&&next.solid.native.energy_j[1]==value[4]);
            CHECK(next.received_q_j-base->received_q_j==value[2]);CHECK(receipt.allocated_q_j==value[0]);
            if(value[0]>0&&value[0]<1e-100)cold++;
            *base=next;
        }
    }
    CHECK(fscanf(f,"%s",header)==EOF);fclose(f);CHECK(client.stages==81920);CHECK(cold>0);physics_sim_receiver_client_close(&client);free(accepted);
    printf("shear receiver: %u checks; 81920 windows; %u faint positive packets; exact native state/receipt parity\n",checks,cold);return 0;
}
