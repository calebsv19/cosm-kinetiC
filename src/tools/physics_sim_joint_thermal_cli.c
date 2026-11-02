/* Local headless joint runtime. Checkpoint is authoritative; observation/Q
 * exports are derived and accepted only with a successful terminal receipt. */
#include "physics_sim_joint_thermal.h"
#ifdef PHYSICS_SIM_LIVE_RECEIVER
#include "physics_sim_receiver_client.h"
#endif
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void observe(FILE *out,const PhysicsSimJointState *j) {
    const PhysicsSimThermalState *s=&j->thermal;double e[2]={0},q=0,gas=0;
    for(int x=0;x<4;x++){q+=s->r[x].received_q_j;for(int d=0;d<2;d++)e[d]+=s->r[x].solid.native.energy_j[d];}
    for(int i=0;i<512;i++)gas+=s->v[i]*(1206*.0625*.0625/512);
    fprintf(out,"{\"time_s\":%.17g,\"q_j\":%.17g,\"gas_mean_k\":%.17g,\"solid0_k\":%.17g,\"solid1_k\":%.17g,\"source_j\":%.17g,\"profile_k\":[",s->tick/(double)TPS,q,gas/4.7109375,e[0]/50,e[1]/50,s->source);
    for(int i=0;i<256;i++)fprintf(out,"%s%.17g",i?",":"",s->v[2*i]/2+s->v[2*i+1]/2);
    fprintf(out,"],\"native_profile_k\":[");for(int i=0;i<512;i++)fprintf(out,"%s%.17g",i?",":"",s->v[i]);fprintf(out,"]}\n");
}
int main(int argc,char **argv) {
    if((argc!=8&&argc!=9)||(strcmp(argv[1],"run")&&strcmp(argv[1],"resume"))||strcmp(argv[6],JOINT_PROFILE))return 2;
    char *tail;errno=0;long parsed=strtol(argv[2],&tail,10);if(errno||!*argv[2]||*tail||parsed<0||parsed>2)return 2;int mode=(int)parsed;
    errno=0;parsed=strtol(argv[7],&tail,10);if(errno||!*argv[7]||*tail||parsed<=0||parsed>END||parsed%MICRO)return 2;int end=(int)parsed;
#ifndef PHYSICS_SIM_LIVE_RECEIVER
    if(argc==9)return 2;
#else
    PhysicsSimReceiverClient client={0};
#endif
    CfdMemoryBudget budget={.limit_bytes=16*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    PhysicsSimJointRuntime rt={0};PhysicsSimJointState base,working;int rc=1;
    if(!physics_sim_joint_init(&rt,mode,&base))goto finish;
    if(!strcmp(argv[1],"run")){if(physics_sim_joint_publish(argv[3],&rt,NULL,&base,0)!=JOINT_OK)goto finish;}
    else if(!physics_sim_joint_load(argv[3],&rt,&base))goto finish;
    if(end<=base.thermal.tick)goto finish;
    FILE *observations=fopen(argv[4],"wx");if(!observations)goto finish;
    FILE *packets=fopen(argv[5],"wx");if(!packets){fclose(observations);goto finish;}
#ifdef PHYSICS_SIM_LIVE_RECEIVER
    if(argc==9){if(!physics_sim_receiver_client_open(&client,argv[8],mode)){fclose(observations);fclose(packets);goto finish;}rt.receiver_stage=physics_sim_receiver_client_stage;rt.receiver_context=&client;}
#endif
    working=base;
    if(!working.thermal.tick)observe(observations,&working);
    while(working.thermal.tick<end) {
        PhysicsSimJointCandidate next;
        if(!physics_sim_joint_prepare(&rt,&working,working.thermal.tick,working.thermal.tick+MICRO,0,&next)||!physics_sim_joint_accept(&rt,&working,&next)){fclose(observations);fclose(packets);goto finish;}
        const PhysicsSimThermalTrace *t=&next.trace;
        fprintf(packets,"%d %d %a",working.thermal.tick,t->micro,t->q);
        for(int i=0;i<t->micro;i++)fprintf(packets," %a %a %a",t->amounts[i],t->energy[i][0],t->energy[i][1]);fputc('\n',packets);
        if(!(working.thermal.tick%OBS))observe(observations,&working);
    }
    bool exports=!ferror(observations)&&!ferror(packets);if(fclose(observations))exports=false;if(fclose(packets))exports=false;if(!exports)goto finish;
    PhysicsSimJointStatus status=physics_sim_joint_publish(argv[3],&rt,&base,&working,0);
    if(status==JOINT_PUBLISHED_RELOAD){PhysicsSimJointState recovered;if(!physics_sim_joint_load(argv[3],&rt,&recovered)||physics_sim_joint_digest(&recovered)!=physics_sim_joint_digest(&working))goto finish;working=recovered;}
    else if(status!=JOINT_OK)goto finish;
    PhysicsSimJointState readback;if(!physics_sim_joint_load(argv[3],&rt,&readback)||physics_sim_joint_digest(&readback)!=physics_sim_joint_digest(&working))goto finish;
    printf("{\"accepted\":true,\"profile\":\"%s\",\"mode\":%d,\"begin_tick\":%d,\"end_tick\":%d,\"generation\":%llu,\"checkpoint_digest\":\"%016llx\"}\n",JOINT_PROFILE,mode,base.thermal.tick,working.thermal.tick,(unsigned long long)working.generation,(unsigned long long)physics_sim_joint_digest(&working));rc=0;
finish:
#ifdef PHYSICS_SIM_LIVE_RECEIVER
    if(client.open){fprintf(stderr,"receiver: %llu accepted stages; %llu requests\n",(unsigned long long)client.stages,(unsigned long long)client.transactions);physics_sim_receiver_client_close(&client);}
#endif
    physics_sim_joint_destroy(&rt);cfd_memory_scope(prior);if(budget.live_bytes)return 3;return rc;
}
