#include "reference_host.h"
#include <string.h>

static CoreSimTimePoint point(uint64_t ticks) {
    return (CoreSimTimePoint){{77,600},ticks};
}
uint64_t ref_mix(uint64_t h, uint64_t value) {
    unsigned i;
    for (i=0;i<8;++i) { h ^= value & 255; h *= UINT64_C(1099511628211); value >>= 8; }
    return h;
}
static uint64_t native_ref(const RefNative *n, uint64_t w, uint64_t participant) {
    uint64_t h=ref_mix(REF_HASH_INIT,w);
    h=ref_mix(h,participant);
    if (participant==1) { h=ref_mix(h,n->emitted); h=ref_mix(h,n->source_steps); }
    else { h=ref_mix(h,n->received); h=ref_mix(h,n->receiver_steps); h=ref_mix(h,n->signal); }
    return h ? h : 1;
}
static void checkpoints(const RefNative *n, uint64_t w, CoreSimCheckpoint *cp) {
    size_t i;
    for (i=0;i<2;++i) cp[i]=(CoreSimCheckpoint){i+1,100+2*w+i,point(120*w),native_ref(n,w,i+1)};
}
static CoreSimExchangeRecord record(const RefHost *h, uint64_t w, unsigned channel,
    const CoreSimCheckpoint *cp, uint64_t value) {
    CoreSimExchangeRecord r={0};
    r.semantic_version=1; r.plan_id=h->schedule.plan_id;
    r.channel_id=channel+1; r.window_index=w; r.sequence=1;
    r.checkpoint_ref=cp->checkpoint_ref;
    r.payload_ref=ref_mix(ref_mix(ref_mix(REF_HASH_INIT,h->plan.plan_ref),cp->checkpoint_ref),value);
    if (!r.payload_ref) r.payload_ref=1;
    r.kind=h->channels[channel].kind; r.checkpoint_time=cp->time;
    if (!channel) r.support.interval=(CoreSimTimeInterval){point(w*120),point((w+1)*120)};
    else r.support.point=cp->time;
    return r;
}
int ref_host_init(RefHost *h, uint64_t mode) {
    CoreSimTimeRatio periods[]={{1,50},{1,200},{1,5},{1,60}};
    CoreSimTimebase base;
    CoreSimDuration source,receiver,exchange;
    CoreSimTimePoint end;
    size_t i;
    /* Fixed profile includes 60-Hz observation representation even when output
     * FPS differs; representation never changes numerical solver cadence. */
    if (mode>2 || core_sim_timebase_from_periods(77,periods,4,600,&base)!=CORE_SIM_TIME_OK ||
        core_sim_duration_from_seconds(base,periods[0],&source)!=CORE_SIM_TIME_OK ||
        core_sim_duration_from_seconds(base,periods[1],&receiver)!=CORE_SIM_TIME_OK ||
        core_sim_duration_from_seconds(base,periods[2],&exchange)!=CORE_SIM_TIME_OK ||
        core_sim_time_point_from_seconds(base,(CoreSimTimeRatio){40,1},&end)!=CORE_SIM_TIME_OK ||
        base.ticks_per_second!=600 || source.ticks!=12 || receiver.ticks!=3 || exchange.ticks!=120)
        return 0;
    memset(h,0,sizeof(*h));
    h->participants[0]=(CoreSimParticipantSchedule){1,source};
    h->participants[1]=(CoreSimParticipantSchedule){2,receiver};
    h->schedule=(CoreSimSchedule){90+mode,{point(0),end},exchange,h->participants,2};
    for (i=0;i<2;++i) {
        CoreSimChannelDesc *c=&h->channels[i];
        c->semantic_version=1; c->plan_id=h->schedule.plan_id; c->channel_id=i+1;
        c->producer_id=i+1; c->consumer_id=2-i; c->quantity_id=10+i;
        c->unit_id=20+i; c->spatial_support_id=30; c->adapter_id=40; c->profile_id=50+mode;
        c->kind=i ? CORE_SIM_TEMPORAL_POINT : CORE_SIM_TEMPORAL_INTEGRATED;
        c->policy=i ? CORE_SIM_POLICY_HOLD : CORE_SIM_POLICY_UNIFORM_RATE;
        c->max_age=(CoreSimDuration){{77,600},i ? 120 : 0};
    }
    h->phases[0]=1; h->phases[1]=2;
    h->plan=(CoreSimJointPlan){1,0,UINT64_C(0x543500)+mode,&h->schedule,h->channels,mode ? 2 : 1,h->phases,2};
    return 1;
}
static void candidate(const RefHost *h, const RefState *s, CoreSimJointCandidate *c) {
    CoreSimCheckpoint prior[2];
    size_t i;
    memset(c,0,sizeof(*c));
    checkpoints(&s->previous,s->windows-1,prior);
    c->semantic_version=1; c->plan_id=h->schedule.plan_id; c->plan_ref=h->plan.plan_ref;
    c->candidate_id=s->windows+1; c->expected_predecessor_id=s->windows;
    c->window_index=s->windows-1;
    c->validation_ref=native_ref(&s->native,s->windows,1);
    c->checkpoint_count=2; c->transfer_count=h->plan.channel_count;
    checkpoints(&s->native,s->windows,c->checkpoints);
    c->transfers[0].lower=record(h,c->window_index,0,&c->checkpoints[0],s->native.emitted-s->previous.emitted);
    if (s->mode) c->transfers[1].lower=record(h,c->window_index,1,&prior[1],s->previous.signal);
    for (i=0;i<c->transfer_count;++i) {
        c->transfers[i].request=point(c->window_index*120);
        c->transfers[i].consumer_checkpoint_ref=c->checkpoints[1-i].checkpoint_ref;
        c->transfers[i].validation_ref=ref_mix(c->transfers[i].lower.payload_ref,s->native.received)+1;
        if (!c->transfers[i].validation_ref) c->transfers[i].validation_ref=1;
    }
}
int ref_native_valid(const RefState *s) {
    const RefNative *n=&s->native, *p=&s->previous;
    uint64_t pw=s->windows ? s->windows-1 : 0;
    if (s->mode>2 || s->windows>200 || n->emitted!=n->received || p->emitted!=p->received ||
        n->source_steps!=s->windows*10 || n->receiver_steps!=s->windows*40 ||
        p->source_steps!=pw*10 || p->receiver_steps!=pw*40 ||
        n->signal!=(n->received!=0) || p->signal!=(p->received!=0)) return 0;
    if (!s->windows) return n->emitted==0 && p->emitted==0;
    return n->emitted>=p->emitted && n->emitted-p->emitted==120*(1+(s->mode==2)*p->signal);
}
int ref_restore_metadata(const RefHost *h, RefState *s) {
    CoreSimCheckpoint cp[2];
    if (!ref_native_valid(s)) return 0;
    memset(&s->progress,0,sizeof(s->progress));
    if (!s->windows) {
        checkpoints(&s->native,0,cp);
        return core_sim_joint_init(&h->plan,1,1,cp,2,&s->progress)==CORE_SIM_JOINT_OK;
    }
    s->progress.completed_windows=s->windows;
    candidate(h,s,&s->progress.accepted);
    checkpoints(&s->previous,s->windows-1,s->progress.previous_checkpoints);
    return core_sim_joint_progress_validate(&h->plan,&s->progress)==CORE_SIM_JOINT_OK;
}
static int observe(const RefHost *h, uint64_t w, uint64_t fps,
    const uint64_t *source, const uint64_t *receiver, RefObservations *o) {
    uint64_t t;
    for (t=w*120+600/fps;t<=(w+1)*120;t+=600/fps) {
        unsigned p;
        o->hash=ref_mix(o->hash,t); ++o->count;
        for (p=0;p<2;++p) {
            CoreSimObservationBracket b;
            const uint64_t *values=p ? receiver : source;
            uint64_t start=w*(p ? 40 : 10), lo,hi,num,den;
            if (core_sim_schedule_observation_bracket(&h->schedule,p+1,point(t),&b)!=CORE_SIM_SCHEDULE_OK)
                return 0;
            lo=b.lower_step_index-start; hi=b.upper_step_index-start;
            if (hi>(p ? 40u : 10u)) return 0;
            if (b.lower_step_index!=b.upper_step_index) ++o->fractional_brackets;
            den=b.weight.denominator;
            num=values[lo]*(den-b.weight.numerator)+values[hi]*b.weight.numerator;
            /* Small bounded integers; normalized scalar sample, no float. */
            {
                uint64_t a=num,bden=den;
                while (bden) { uint64_t r=a%bden; a=bden; bden=r; }
                num/=a; den/=a;
            }
            o->hash=ref_mix(ref_mix(o->hash,num),den);
        }
    }
    return 1;
}
int ref_window(const RefHost *h, const RefState *current, unsigned fault,
    uint64_t fps, RefState *out, RefObservations *observations) {
    RefState staged=*current;
    CoreSimJointCandidate c, duplicate;
    CoreSimWindowSteps source_steps, receiver_steps;
    CoreSimExchangeRecord emission;
    CoreSimCheckpoint cp[2];
    CoreSimExchangeSample held;
    RefObservations seen=*observations;
    uint64_t source[11], receiver[41], i, amount, signal=0;
    if (current->windows>=200 || !ref_native_valid(current)) return 0;
    if (core_sim_joint_progress_validate(&h->plan,&current->progress)!=CORE_SIM_JOINT_OK) return 0;
    if (current->mode) {
        CoreSimExchangeRecord input=record(h,current->windows,1,&current->progress.accepted.checkpoints[1],current->native.signal);
        if (core_sim_exchange_sample(&h->schedule,&h->channels[1],point(current->windows*120),
            point(current->windows*120),&input,NULL,&held)!=CORE_SIM_EXCHANGE_OK) return 0;
        signal=current->native.signal; /* Adapter reads the payload bound above. */
    }
    staged.previous=current->native;
    if (core_sim_schedule_window_steps(&h->schedule,1,current->windows,&source_steps)!=CORE_SIM_SCHEDULE_OK ||
        core_sim_schedule_window_steps(&h->schedule,2,current->windows,&receiver_steps)!=CORE_SIM_SCHEDULE_OK)
        return 0;
    if (source_steps.step_count!=10 || receiver_steps.step_count!=40) return 0;
    source[0]=staged.native.emitted;
    for (i=0;i<source_steps.step_count;++i) {
        CoreSimTimeInterval step;
        if (core_sim_schedule_step(&h->schedule,1,source_steps.first_step_index+i,&step)!=CORE_SIM_SCHEDULE_OK ||
            step.begin.ticks!=staged.native.source_steps*12) return 0;
        staged.native.emitted+=(step.end.ticks-step.begin.ticks)*(1+(current->mode==2)*signal);
        ++staged.native.source_steps; source[i+1]=staged.native.emitted;
    }
    if (fault==1) return 75;
    amount=staged.native.emitted-current->native.emitted;
    checkpoints(&staged.native,current->windows+1,cp);
    emission=record(h,current->windows,0,&cp[0],amount);
    receiver[0]=staged.native.received;
    for (i=0;i<receiver_steps.step_count;++i) {
        CoreSimTimeInterval step;
        CoreSimExchangeOverlap overlap;
        uint64_t allocated;
        if (core_sim_schedule_step(&h->schedule,2,receiver_steps.first_step_index+i,&step)!=CORE_SIM_SCHEDULE_OK ||
            step.begin.ticks!=staged.native.receiver_steps*3 ||
            core_sim_exchange_overlap(&h->schedule,&h->channels[0],&emission,point((current->windows+1)*120),
                step,&overlap)!=CORE_SIM_EXCHANGE_OK) return 0;
        if (amount%overlap.source_fraction.denominator) return 0;
        allocated=(amount/overlap.source_fraction.denominator)*overlap.source_fraction.numerator;
        staged.native.received+=allocated; ++staged.native.receiver_steps;
        staged.native.signal=staged.native.received!=0; receiver[i+1]=staged.native.received;
    }
    if (fault==2) return 75;
    ++staged.windows;
    if (!ref_native_valid(&staged)) return 0;
    candidate(h,&staged,&c);
    if (core_sim_joint_accept(&h->plan,&current->progress,&c,&staged.progress)!=CORE_SIM_JOINT_OK) return 0;
    /* Admission probes do not execute/allocate any physical source a second time. */
    if (core_sim_joint_candidate_validate(&h->plan,&staged.progress,&c)!=CORE_SIM_JOINT_REPLAY) return 0;
    duplicate=c; duplicate.transfers[0].lower.payload_ref^=1;
    if (!duplicate.transfers[0].lower.payload_ref) duplicate.transfers[0].lower.payload_ref=2;
    if (core_sim_joint_candidate_validate(&h->plan,&staged.progress,&duplicate)!=CORE_SIM_JOINT_CONFLICT) return 0;
    staged.trajectory=ref_mix(staged.trajectory,staged.windows);
    staged.trajectory=ref_mix(staged.trajectory,staged.native.emitted);
    staged.trajectory=ref_mix(staged.trajectory,staged.native.received);
    staged.trajectory=ref_mix(staged.trajectory,staged.native.source_steps);
    staged.trajectory=ref_mix(staged.trajectory,staged.native.receiver_steps);
    staged.trajectory=ref_mix(staged.trajectory,staged.native.signal);
    if (!observe(h,current->windows,fps,source,receiver,&seen)) return 0;
    *out=staged; *observations=seen;
    return 1;
}
