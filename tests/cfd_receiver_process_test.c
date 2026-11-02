#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "physics_sim_joint_thermal.h"
#include "physics_sim_receiver_client.h"
#include "physics_sim_receiver_wire.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static bool reject_stage(void *context,int column,const ThermalReceiver *base,const ThermalReceiverExchange *packet,CoreSimTimePoint time,ThermalReceiver *next,ThermalReceiverReceipt *receipt){
    ThermalReceiverExchange wrong=*packet;wrong.unit=99;
    return physics_sim_receiver_client_stage(context,column,base,&wrong,time,next,receipt);
}
int main(int argc,char **argv){
    assert(argc==2);CfdMemoryBudget budget={.limit_bytes=16*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    PhysicsSimJointRuntime rt={0};PhysicsSimJointState s,old;PhysicsSimJointCandidate c,local,sentinel;
    PhysicsSimReceiverClient client={0};CHECK(physics_sim_joint_init(&rt,0,&s));old=s;
    CHECK(physics_sim_receiver_client_open(&client,argv[1],0));
    const ThermalReceiver *base=&s.thermal.r[0];CoreSimTimeInterval interval={base->solid.time,{base->config.timebase,1}};
    ThermalReceiverExchange input;CHECK(thermal_receiver_exchange_make(base,interval,1,.001,&input));
    ThermalReceiver expected,actual,saved;ThermalReceiverReceipt er,ar,untouched;
    CHECK(thermal_receiver_stage(base,&input,interval.end,&expected,&er));
    memset(&actual,0x55,sizeof actual);saved=actual;memset(&ar,0x55,sizeof ar);untouched=ar;
    for(int fault=0;fault<10;fault++){
        ThermalReceiverExchange wrong=input;CoreSimTimePoint available=interval.end;
        switch(fault){case 0:wrong.unit=99;break;case 1:wrong.sequence++;break;case 2:wrong.source_checkpoint_ref=0;break;case 3:wrong.binding++;break;case 4:wrong.q_j+=1;break;case 5:wrong.producer_sha256[0]^=1;break;case 6:available.ticks++;break;case 7:wrong.interval.end.ticks++;break;case 8:wrong.sign=2;break;case 9:wrong.payload_hash++;break;}
        CHECK(!physics_sim_receiver_client_stage(&client,0,base,&wrong,available,&actual,&ar));CHECK(!memcmp(&actual,&saved,sizeof saved));CHECK(!memcmp(&ar,&untouched,sizeof ar));CHECK(!memcmp(&s,&old,sizeof s));
    }
    for(int retry=0;retry<2;retry++){CHECK(physics_sim_receiver_client_stage(&client,0,base,&input,interval.end,&actual,&ar));CHECK(!memcmp(&actual,&expected,sizeof actual));CHECK(!memcmp(&ar,&er,sizeof ar));}
    saved=actual;CHECK(!physics_sim_receiver_client_stage(&client,0,&expected,&input,interval.end,&actual,&ar));CHECK(!memcmp(&actual,&saved,sizeof actual));
    /* Joint refusal after already staging through the separate process remains
       retryable: worker candidates were never independently committed. */
    memset(&c,0x44,sizeof c);sentinel=c;rt.receiver_context=&client;rt.receiver_stage=reject_stage;
    CHECK(!physics_sim_joint_prepare(&rt,&s,0,8,0,&c));CHECK(!memcmp(&c,&sentinel,sizeof c));CHECK(!memcmp(&s,&old,sizeof s));
    rt.receiver_stage=physics_sim_receiver_client_stage;
    CHECK(!physics_sim_joint_prepare(&rt,&s,0,8,1,&c));CHECK(!memcmp(&s,&old,sizeof s));CHECK(!memcmp(&c,&sentinel,sizeof c));
    CHECK(!physics_sim_joint_prepare(&rt,&s,0,8,2,&c));CHECK(!memcmp(&s,&old,sizeof s));
    CHECK(physics_sim_joint_prepare(&rt,&s,0,8,0,&c));
    rt.receiver_stage=NULL;CHECK(physics_sim_joint_prepare(&rt,&s,0,8,0,&local));CHECK(c.state_hash==local.state_hash);
    CHECK(physics_sim_joint_accept(&rt,&s,&c));CHECK(!physics_sim_joint_accept(&rt,&s,&c));
    rt.receiver_stage=physics_sim_receiver_client_stage;old=s;kill(client.pid,SIGKILL);
    CHECK(!physics_sim_joint_prepare(&rt,&s,8,16,0,&c));CHECK(!memcmp(&s,&old,sizeof s));physics_sim_receiver_client_close(&client);
    CHECK(physics_sim_receiver_client_open(&client,argv[1],0));CHECK(physics_sim_joint_prepare(&rt,&s,8,16,0,&c));
    rt.receiver_stage=NULL;CHECK(physics_sim_joint_prepare(&rt,&s,8,16,0,&local));CHECK(c.state_hash==local.state_hash);
    /* Deliberately lose a complete response after worker stage, then restart
       and retry the same accepted base and packet without an extra heat credit. */
    base=&s.thermal.r[0];interval=(CoreSimTimeInterval){base->solid.time,{base->config.timebase,base->solid.time.ticks+1}};
    CHECK(thermal_receiver_exchange_make(base,interval,9,.001,&input));ThermalReceiver copy=*base;
    PhysicsSimReceiverWire w=psrw_new();psrw_u(&w,1);psrw_u(&w,777);psrw_u(&w,0);psrw_state(&w,&copy);psrw_packet(&w,&input);psrw_time(&w,&interval.end);
    CHECK(psrw_send(client.input,&w));CHECK(psrw_receive(client.output,&w));CHECK(!memcmp(&s,&old,sizeof s));physics_sim_receiver_client_close(&client);
    CHECK(physics_sim_receiver_client_open(&client,argv[1],0));CHECK(physics_sim_receiver_client_stage(&client,0,base,&input,interval.end,&actual,&ar));
    CHECK(thermal_receiver_stage(base,&input,interval.end,&expected,&er));CHECK(!memcmp(&actual,&expected,sizeof actual));CHECK(!memcmp(&ar,&er,sizeof ar));
    /* Framing corruption causes refusal/EOF, never state adoption. */
    w=psrw_new();psrw_u(&w,9);CHECK(psrw_send(client.input,&w));CHECK(!psrw_receive(client.output,&w));CHECK(!memcmp(&s,&old,sizeof s));physics_sim_receiver_client_close(&client);
    physics_sim_joint_destroy(&rt);cfd_memory_scope(prior);CHECK(budget.live_bytes==0);printf("receiver process: %u checks; tracked peak %zu bytes\n",checks,budget.peak_bytes);return 0;
}
