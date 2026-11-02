#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "physics_sim_receiver_client.h"
#include "physics_sim_receiver_wire.h"
#include "physics_sim_shear_receiver_fixture.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
static struct sigaction previous_pipe;
static bool active_client;
bool physics_sim_receiver_client_open_profile(PhysicsSimReceiverClient *out,const char *path,const char *profile){
    if(!out||!path||!profile||active_client)return false;
    bool warm=strlen(profile)==1&&profile[0]>='0'&&profile[0]<='2';
    if(!warm&&!physics_sim_shear_receiver_steps(profile))return false;
    int request[2],response[2];if(pipe(request))return false;if(pipe(response)){close(request[0]);close(request[1]);return false;}
    struct sigaction ignore={0};ignore.sa_handler=SIG_IGN;sigemptyset(&ignore.sa_mask);
    if(sigaction(SIGPIPE,&ignore,&previous_pipe)){for(int i=0;i<2;i++){close(request[i]);close(response[i]);}return false;}
    pid_t pid=fork();
    if(!pid){if(dup2(request[0],STDIN_FILENO)<0||dup2(response[1],STDOUT_FILENO)<0)_exit(126);
        for(int i=0;i<2;i++){close(request[i]);close(response[i]);}execl(path,path,profile,(char *)NULL);_exit(127);}
    close(request[0]);close(response[1]);if(pid<0){close(request[1]);close(response[0]);sigaction(SIGPIPE,&previous_pipe,NULL);return false;}
    *out=(PhysicsSimReceiverClient){.input=request[1],.output=response[0],.pid=pid,.open=true,.healthy=true,.columns=warm?4u:128u};active_client=true;return true;
}
bool physics_sim_receiver_client_open(PhysicsSimReceiverClient *out,const char *path,int mode){
    if(mode<0||mode>2)return false;char text[2]={(char)('0'+mode),0};
    return physics_sim_receiver_client_open_profile(out,path,text);
}
void physics_sim_receiver_client_close(PhysicsSimReceiverClient *c){
    if(!c||!c->open)return;close(c->input);close(c->output);kill(c->pid,SIGTERM);
    while(waitpid(c->pid,NULL,0)<0&&errno==EINTR){}sigaction(SIGPIPE,&previous_pipe,NULL);active_client=false;c->open=c->healthy=false;
}
bool physics_sim_receiver_client_stage(void *context,int column,const ThermalReceiver *accepted,
    const ThermalReceiverExchange *packet,CoreSimTimePoint available,ThermalReceiver *out,ThermalReceiverReceipt *receipt){
    PhysicsSimReceiverClient *c=context;if(!c||!c->open||!c->healthy||!accepted||!packet||!out||!receipt||out==accepted||column<0||(unsigned)column>=c->columns)return false;
    ThermalReceiver base=*accepted;ThermalReceiverExchange input=*packet;PhysicsSimReceiverWire w=psrw_new();
    uint64_t transaction=++c->transactions;psrw_u(&w,1);psrw_u(&w,transaction);psrw_u(&w,(uint64_t)column);psrw_state(&w,&base);psrw_packet(&w,&input);psrw_time(&w,&available);
    uint64_t hash=psrw_hash(w.bytes,w.at);if(!psrw_send(c->input,&w)||!psrw_receive(c->output,&w)){c->healthy=false;return false;}
    uint64_t version=psrw_u(&w,0),echo=psrw_u(&w,0),request=psrw_u(&w,0),status=psrw_u(&w,0);
    if(version!=1||echo!=transaction||request!=hash||status>1){c->healthy=false;return false;}
    if(!status){if(!w.ok||w.at!=w.n)c->healthy=false;return false;}
    ThermalReceiver next=*accepted;ThermalReceiverReceipt r={0};psrw_state(&w,&next);
    psrw_time(&w,&r.interval.begin);psrw_time(&w,&r.interval.end);r.source_checkpoint_ref=psrw_u(&w,0);r.payload_hash=psrw_u(&w,0);r.q_j=psrw_d(&w,0);r.allocated_q_j=psrw_d(&w,0);r.time_residual_j=psrw_d(&w,0);
    if(!w.ok||w.at!=w.n||r.interval.begin.timebase.domain!=packet->interval.begin.timebase.domain||
       r.interval.begin.timebase.ticks_per_second!=packet->interval.begin.timebase.ticks_per_second||r.interval.begin.ticks!=packet->interval.begin.ticks||
       r.interval.end.timebase.domain!=packet->interval.end.timebase.domain||r.interval.end.timebase.ticks_per_second!=packet->interval.end.timebase.ticks_per_second||r.interval.end.ticks!=packet->interval.end.ticks||r.source_checkpoint_ref!=packet->source_checkpoint_ref||r.payload_hash!=packet->payload_hash||
       r.q_j!=packet->q_j||r.allocated_q_j!=packet->q_j||r.time_residual_j!=0||next.binding!=accepted->binding||
       next.windows!=accepted->windows+1||next.solid.time.timebase.domain!=available.timebase.domain||
       next.solid.time.timebase.ticks_per_second!=available.timebase.ticks_per_second||next.solid.time.ticks!=available.ticks||
       !thermal_receiver_observe(&next,&r.accepted_candidate)||!isfinite(next.received_q_j)||next.received_q_j!=accepted->received_q_j+packet->q_j){c->healthy=false;return false;}
    *out=next;*receipt=r;c->stages++;return true;
}
