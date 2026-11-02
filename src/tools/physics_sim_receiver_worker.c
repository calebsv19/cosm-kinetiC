/* PhysicsSim-owned process adapter. Only unchanged Thermal numerical sources
 * are linked. Every request stages from the supplied accepted state; this
 * process never commits heat, owns no checkpoint and performs no gas solve. */
#include <stdbool.h>
#include "physics_sim_receiver_fixture.h"
#include "physics_sim_shear_receiver_fixture.h"
#include "physics_sim_receiver_wire.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
    if(argc!=2)return 2;
    int steps=physics_sim_shear_receiver_steps(argv[1]);bool warm=strlen(argv[1])==1&&argv[1][0]>='0'&&argv[1][0]<='2';
    if(!steps&&!warm)return 2;unsigned columns=warm?4u:128u;int code=0;
    ThermalReceiver *seed=calloc(columns,sizeof *seed);if(!seed)return 1;
    if(warm){if(!physics_sim_receiver_fixture(argv[1][0]-'0',seed)){free(seed);return 2;}}
    else for(unsigned x=0;x<columns;x++)if(!physics_sim_shear_receiver_init((int)x,steps,&seed[x])){free(seed);return 2;}
    for(;;){PhysicsSimReceiverWire w;if(!psrw_receive_wait(STDIN_FILENO,&w,60000))break;
        uint64_t hash=psrw_hash(w.bytes,w.n),version=psrw_u(&w,0),transaction=psrw_u(&w,0),column=psrw_u(&w,0);
        if(version!=1||column>=columns){code=2;break;}
        ThermalReceiver accepted=seed[column],candidate;ThermalReceiverExchange packet={0};ThermalReceiverReceipt receipt;CoreSimTimePoint available={0};
        psrw_state(&w,&accepted);psrw_packet(&w,&packet);psrw_time(&w,&available);if(!w.ok||w.at!=w.n){code=2;break;}
        bool ok=thermal_receiver_stage(&accepted,&packet,available,&candidate,&receipt);
        w=psrw_new();psrw_u(&w,1);psrw_u(&w,transaction);psrw_u(&w,hash);psrw_u(&w,ok?1:0);
        if(ok){psrw_state(&w,&candidate);psrw_time(&w,&receipt.interval.begin);psrw_time(&w,&receipt.interval.end);psrw_u(&w,receipt.source_checkpoint_ref);psrw_u(&w,receipt.payload_hash);psrw_d(&w,receipt.q_j);psrw_d(&w,receipt.allocated_q_j);psrw_d(&w,receipt.time_residual_j);}
        if(!psrw_send(STDOUT_FILENO,&w)){code=1;break;}
    }
    free(seed);return code;
}
