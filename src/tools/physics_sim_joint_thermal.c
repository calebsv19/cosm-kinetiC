#include "physics_sim_joint_thermal.h"
#include "physics_sim_receiver_fixture.h"
#include <float.h>
#include <math.h>
#include <string.h>
static double inventory(const PhysicsSimThermalState *s){double e=0;for(int i=0;i<512;i++)e+=s->v[i]*(1206*.0625*.0625/512);return e;}
bool physics_sim_joint_seed(int mode,PhysicsSimJointState *out) {
    if(!out||mode<0||mode>2)return false;
    PhysicsSimJointState s={0};s.version=1;s.mode=(unsigned)mode;
    for(int i=0;i<512;i++)s.thermal.v[i]=mode==2?0:20;
    if(!physics_sim_receiver_fixture(mode,s.thermal.r))return false;
    *out=s;return true;
}
bool physics_sim_joint_init(PhysicsSimJointRuntime *out,int mode,PhysicsSimJointState *state) {
    if(!out||!state||mode<0||mode>2)return false;
    PhysicsSimJointRuntime rt={0};PhysicsSimJointState s={0};rt.mode=mode;
    int cells[3]={4,4,512};double length[3]={.25,.25,.0625},zero[2]={0},temp[2]={300,300},width[512];
    if(!cfd_open_atmosphere3d_init(&rt.gas,cells,length,1.2,1.8e-5,1005,300,.026,0,1./6400,zero,temp,zero,false,0,0,.1))return false;
    rt.gas.ground=true;for(int i=0;i<rt.gas.grid.count;i++)rt.gas.velocity[i]=.05*(i/16+.5)/512;
    for(int i=0;i<512;i++){width[i]=.0625/512;s.thermal.v[i]=mode==2?0:20;}
    if(!physics_sim_local_matrix_init(&rt.matrix,512,width,0,.026/1206,.5/TPS,false)){cfd_open_atmosphere3d_destroy(&rt.gas);return false;}
    if(!physics_sim_joint_seed(mode,&s)){physics_sim_local_matrix_destroy(&rt.matrix);cfd_open_atmosphere3d_destroy(&rt.gas);return false;}
    s.version=1;s.mode=(unsigned)mode;rt.initialized=true;for(int x=0;x<4;x++)rt.receiver_binding[x]=s.thermal.r[x].binding;*out=rt;*state=s;return true;
}
void physics_sim_joint_destroy(PhysicsSimJointRuntime *rt) {
    if(rt&&rt->initialized){physics_sim_local_matrix_destroy(&rt->matrix);cfd_open_atmosphere3d_destroy(&rt->gas);memset(rt,0,sizeof *rt);}
}
bool physics_sim_joint_valid(const PhysicsSimJointRuntime *rt,const PhysicsSimJointState *s) {
    if(!rt||!rt->initialized||!s||s->version!=1||s->mode!=(unsigned)rt->mode||
       s->thermal.tick<0||s->thermal.tick>END||s->thermal.tick%MICRO||s->generation!=(uint64_t)s->thermal.tick/MICRO||!isfinite(s->last_q))return false;
    const PhysicsSimThermalState *t=&s->thermal;
    for(int i=0;i<512;i++)if(!isfinite(t->v[i])||t->v[i]<0||t->v[i]>21+64*DBL_EPSILON*21||
       !isfinite(t->correction[i])||fabs(t->correction[i])>64*DBL_EPSILON*fmax(1,t->v[i]))return false;
    double source=4.7109375*fmin(1,t->tick/(double)TPS);
    if(!isfinite(t->source)||fabs(t->source-source)>1e-11)return false;
    for(int x=0;x<4;x++) {
        const ThermalReceiver *r=&t->r[x];ThermalReceiverObservation obs;
        if(r->binding!=rt->receiver_binding[x]||r->config.timebase.domain!=717||r->config.timebase.ticks_per_second!=6400||r->config.window_steps!=1||r->config.total_steps!=32000||
           r->config.initial_k[0]!=(s->mode==2?320:300)||r->config.initial_k[1]!=r->config.initial_k[0]||
           strcmp(r->config.fixture_sha256,COUPLED_FIXTURE_ID)||strcmp(r->config.producer_sha256,COUPLED_SOURCE_ID)||
           strcmp(r->config.support_sha256,COUPLED_SUPPORT_ID)||r->solid.completed_steps!=s->generation||
           r->solid.time.ticks!=s->generation||r->windows!=s->generation||!thermal_receiver_observe(r,&obs))return false;
    }
    double q=0;for(int x=0;x<4;x++)q+=t->r[x].received_q_j;
    double initial=s->mode==2?2000:94.21875;
    double residual=inventory(t)+physics_sim_thermal_solid_energy(t)-initial-t->source;
    if(!isfinite(residual)||fabs(residual)/(initial+4.7109375)>1e-8)return false;
    if(s->generation&&(s->mode==0?s->last_q<=0:s->mode==1?s->last_q!=0:s->last_q>=0))return false;
    if(fabs(physics_sim_thermal_solid_energy(t)-(s->mode==2?2000:0)-q)>1e-8*(initial+4.7109375))return false;
    return true;
}
bool physics_sim_joint_prepare(PhysicsSimJointRuntime *rt,const PhysicsSimJointState *s,
    int expected,int available,int fail,PhysicsSimJointCandidate *out) {
    if(!out||!physics_sim_joint_valid(rt,s)||s->thermal.tick!=expected||expected>END-MICRO||available!=expected+MICRO||fail<0||fail>2)return false;
    PhysicsSimJointCandidate next={0};next.state=*s;
    if(!physics_sim_thermal_step_with_receiver(&s->thermal,&next.state.thermal,&rt->gas,&rt->matrix,MICRO,1,rt->mode,512,fail,&next.trace,rt->receiver_stage,rt->receiver_context))return false;
    next.state.generation++;next.state.last_q=next.trace.q;
    if(!physics_sim_joint_valid(rt,&next.state)||
       fabs(next.trace.debit-next.trace.q)>128*DBL_EPSILON*(1+fabs(next.trace.q))||
       fabs(next.trace.solid-next.trace.q)>128*DBL_EPSILON*(1+physics_sim_thermal_solid_energy(&s->thermal)+fabs(next.trace.q))||
       next.trace.allocation!=next.trace.q)return false;
    next.base_hash=physics_sim_joint_digest(s);next.state_hash=physics_sim_joint_digest(&next.state);*out=next;return true;
}
bool physics_sim_joint_accept(PhysicsSimJointRuntime *rt,PhysicsSimJointState *accepted,const PhysicsSimJointCandidate *next) {
    if(!accepted||!next||!physics_sim_joint_valid(rt,accepted)||!physics_sim_joint_valid(rt,&next->state)||
       next->base_hash!=physics_sim_joint_digest(accepted)||next->state_hash!=physics_sim_joint_digest(&next->state)||
       next->state.generation!=accepted->generation+1||next->state.thermal.tick!=accepted->thermal.tick+MICRO)return false;
    *accepted=next->state;return true;
}
