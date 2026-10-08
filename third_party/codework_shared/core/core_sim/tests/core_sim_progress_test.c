#include "core_sim_progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long checks;
#define CHECK(v) do { ++checks; if (!(v)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v); exit(1); } } while (0)
#define OK(v) CHECK((v) == CORE_SIM_JOINT_OK)
#define UNCHANGED(call, output, expected) do { \
    unsigned char saved[sizeof(output)]; memcpy(saved,&(output),sizeof(output)); \
    CHECK((call)==(expected)); CHECK(memcmp(saved,&(output),sizeof(output))==0); \
} while (0)

typedef struct Fixture {
    CoreSimParticipantSchedule participants[2];
    CoreSimSchedule schedule;
    CoreSimChannelDesc channels[2];
    uint64_t order[2];
    CoreSimJointPlan plan;
} Fixture;
static CoreSimTimebase base = {91,200};
static CoreSimTimePoint point(uint64_t ticks) { return (CoreSimTimePoint){base,ticks}; }
static void fixture(Fixture *f, uint64_t begin, uint64_t end) {
    size_t i;
    memset(f,0,sizeof(*f));
    f->participants[0] = (CoreSimParticipantSchedule){1,{base,4}};
    f->participants[1] = (CoreSimParticipantSchedule){2,{base,1}};
    f->schedule = (CoreSimSchedule){17,{point(begin),point(end)},{base,40},f->participants,2};
    for (i=0;i<2;++i) {
        CoreSimChannelDesc *c = &f->channels[i];
        c->semantic_version=1; c->plan_id=17; c->channel_id=i+1;
        c->producer_id=i+1; c->consumer_id=2-i;
        c->quantity_id=10+i; c->unit_id=20+i; c->spatial_support_id=30;
        c->adapter_id=40; c->profile_id=50;
        c->kind=i ? CORE_SIM_TEMPORAL_POINT : CORE_SIM_TEMPORAL_INTEGRATED;
        c->policy=i ? CORE_SIM_POLICY_HOLD : CORE_SIM_POLICY_UNIFORM_RATE;
        c->max_age=(CoreSimDuration){base,i ? 8 : 0};
    }
    f->order[0]=1; f->order[1]=2;
    f->plan=(CoreSimJointPlan){1,0,55,&f->schedule,f->channels,2,f->order,2};
}
static void initial(const Fixture *f, CoreSimJointProgress *out) {
    CoreSimCheckpoint cp[2] = {
        {1,100,{{91,200},0},900}, {2,200,{{91,200},0},901}
    };
    cp[0].time=cp[1].time=f->schedule.horizon.begin;
    OK(core_sim_joint_init(&f->plan,1,888,cp,2,out));
}
static CoreSimExchangeRecord record(const CoreSimChannelDesc *c, uint64_t w,
    const CoreSimCheckpoint *cp) {
    CoreSimExchangeRecord r = {0};
    r.semantic_version=1; r.plan_id=17; r.channel_id=c->channel_id;
    r.window_index=w; r.sequence=1; r.kind=c->kind;
    r.checkpoint_ref=cp->checkpoint_ref; r.payload_ref=1000+w*2+c->channel_id;
    r.checkpoint_time=cp->time; r.support.point=cp->time;
    return r;
}
static void next(const Fixture *f, const CoreSimJointProgress *current, CoreSimJointCandidate *c) {
    size_t i;
    uint64_t w=current->completed_windows;
    uint64_t begin=f->schedule.horizon.begin.ticks+w*40;
    memset(c,0,sizeof(*c));
    c->semantic_version=1; c->plan_id=17; c->plan_ref=55;
    c->candidate_id=w+2; c->expected_predecessor_id=current->accepted.candidate_id;
    c->window_index=w; c->validation_ref=5000+w;
    c->checkpoint_count=c->transfer_count=2;
    for (i=0;i<2;++i)
        c->checkpoints[i]=(CoreSimCheckpoint){i+1,10000+w*2+i,point(begin+40),6000+w*2+i};
    c->transfers[0].lower=record(&f->channels[0],w,&c->checkpoints[0]);
    c->transfers[0].lower.support.interval=(CoreSimTimeInterval){point(begin),point(begin+40)};
    c->transfers[1].lower=record(&f->channels[1],w,&current->accepted.checkpoints[1]);
    for (i=0;i<2;++i) {
        c->transfers[i].request=point(begin);
        c->transfers[i].consumer_checkpoint_ref=c->checkpoints[1-i].checkpoint_ref;
        c->transfers[i].validation_ref=7000+w*2+i;
    }
}

static void transitions(void) {
    Fixture f;
    CoreSimJointProgress current, output, restarted;
    CoreSimJointCandidate c, bad, first;
    uint64_t w;
    fixture(&f,7,8007); initial(&f,&current);
    memset(&output,0xA5,sizeof(output));
    for (w=0;w<200;++w) {
        next(&f,&current,&c);
        CHECK(c.window_index==w && c.checkpoints[0].time.ticks==7+(w+1)*40);
        OK(core_sim_joint_candidate_validate(&f.plan,&current,&c));
        bad=c; bad.checkpoint_count=1;
        UNCHANGED(core_sim_joint_accept(&f.plan,&current,&bad,&output),output,CORE_SIM_JOINT_INCOMPLETE);
        CHECK(current.completed_windows==w && current.accepted.checkpoints[1].time.ticks==7+w*40);
        /* An unpublished proposal does not change the accepted source object. */
        OK(core_sim_joint_accept(&f.plan,&current,&c,&output));
        CHECK(current.completed_windows==w && output.completed_windows==w+1);
        restarted=output; /* Host deserialization is represented by a value copy. */
        OK(core_sim_joint_progress_validate(&f.plan,&restarted));
        CHECK(restarted.accepted.candidate_id==w+2);
        current=restarted;
        UNCHANGED(core_sim_joint_accept(&f.plan,&current,&c,&output),output,CORE_SIM_JOINT_REPLAY);
        bad=c; bad.transfers[0].lower.payload_ref++;
        UNCHANGED(core_sim_joint_accept(&f.plan,&current,&bad,&output),output,CORE_SIM_JOINT_CONFLICT);
        bad=c; bad.candidate_id+=1000;
        UNCHANGED(core_sim_joint_accept(&f.plan,&current,&bad,&output),output,CORE_SIM_JOINT_CONFLICT);
        if (!w) first=c;
    }
    CHECK(current.completed_windows==200 && current.accepted.checkpoints[0].time.ticks==8007);
    next(&f,&current,&c);
    UNCHANGED(core_sim_joint_accept(&f.plan,&current,&c,&output),output,CORE_SIM_JOINT_COMPLETE);
    UNCHANGED(core_sim_joint_accept(&f.plan,&current,&first,&output),output,CORE_SIM_JOINT_STALE_PREDECESSOR);
    /* In-place transition has the same contract; the host still owns durability. */
    fixture(&f,0,120); initial(&f,&current); next(&f,&current,&c);
    OK(core_sim_joint_accept(&f.plan,&current,&c,&current));
    CHECK(current.completed_windows==1);
    bad=c; bad.checkpoint_count=1;
    UNCHANGED(core_sim_joint_accept(&f.plan,&current,&bad,&current),current,CORE_SIM_JOINT_INCOMPLETE);
    bad=c; bad.transfers[0].validation_ref++;
    UNCHANGED(core_sim_joint_accept(&f.plan,&current,&bad,&output),output,CORE_SIM_JOINT_CONFLICT);
    bad=c; bad.transfers[0].upper.payload_ref=999; /* inactive bytes have no semantics */
    UNCHANGED(core_sim_joint_accept(&f.plan,&current,&bad,&output),output,CORE_SIM_JOINT_REPLAY);
    UNCHANGED(core_sim_joint_accept(&f.plan,&current,&c,&current),current,CORE_SIM_JOINT_REPLAY);
}

static void failures(void) {
    Fixture f;
    CoreSimJointProgress current, output;
    CoreSimJointCandidate good, bad;
    unsigned i;
    static const CoreSimJointStatus expected[] = {
        CORE_SIM_JOINT_UNSUPPORTED,CORE_SIM_JOINT_UNSUPPORTED,
        CORE_SIM_JOINT_CAPACITY,CORE_SIM_JOINT_CAPACITY,
        CORE_SIM_JOINT_IDENTITY_MISMATCH,CORE_SIM_JOINT_IDENTITY_MISMATCH,
        CORE_SIM_JOINT_INCOMPLETE,CORE_SIM_JOINT_INCOMPLETE,
        CORE_SIM_JOINT_STALE_PREDECESSOR,CORE_SIM_JOINT_WRONG_TIME,
        CORE_SIM_JOINT_INCOMPLETE,CORE_SIM_JOINT_WRONG_TIME,
        CORE_SIM_JOINT_IDENTITY_MISMATCH,CORE_SIM_JOINT_INCOMPLETE,
        CORE_SIM_JOINT_IDENTITY_MISMATCH,CORE_SIM_JOINT_INCOMPLETE,
        CORE_SIM_JOINT_IDENTITY_MISMATCH,CORE_SIM_JOINT_WRONG_TIME,
        CORE_SIM_JOINT_CAUSALITY,CORE_SIM_JOINT_IDENTITY_MISMATCH,
        CORE_SIM_JOINT_INVALID_ARGUMENT,CORE_SIM_JOINT_CAUSALITY,
        CORE_SIM_JOINT_WRONG_TIME,CORE_SIM_JOINT_IDENTITY_MISMATCH,
        CORE_SIM_JOINT_IDENTITY_MISMATCH,CORE_SIM_JOINT_CAUSALITY
    };
    fixture(&f,0,120); initial(&f,&current); next(&f,&current,&good);
    memset(&output,0x5A,sizeof(output));
    for (i=0;i<sizeof(expected)/sizeof(expected[0]);++i) {
        bad=good;
        switch (i) {
        case 0: bad.semantic_version++; break;
        case 1: bad.required_features=1; break;
        case 2: bad.checkpoint_count=257; break;
        case 3: bad.transfer_count=65; break;
        case 4: bad.plan_id++; break;
        case 5: bad.plan_ref++; break;
        case 6: bad.candidate_id=0; break;
        case 7: bad.validation_ref=0; break;
        case 8: bad.expected_predecessor_id=999; break;
        case 9: bad.window_index=1; break;
        case 10: bad.checkpoint_count=1; break;
        case 11: bad.checkpoints[1].time.ticks--; break;
        case 12: bad.checkpoints[1].participant_id=1; break;
        case 13: bad.checkpoints[0].validation_ref=0; break;
        case 14: bad.checkpoints[0].checkpoint_ref=current.accepted.checkpoints[0].checkpoint_ref; break;
        case 15: bad.transfer_count=1; break;
        case 16: bad.transfers[0].consumer_checkpoint_ref++; break;
        case 17: bad.transfers[0].lower.window_index=1; break;
        case 18: bad.transfers[1].lower.checkpoint_ref=good.checkpoints[1].checkpoint_ref;
            bad.transfers[1].lower.checkpoint_time=bad.transfers[1].lower.support.point=point(40); break;
        case 19: bad.transfers[0].lower.payload_ref=0; break;
        case 20: bad.transfers[0].has_upper=2; break;
        case 21: bad.transfers[1].request=point(9); break;
        case 22: bad.transfers[0].lower.support.interval.begin=point(1); break;
        case 23: bad.checkpoints[0].checkpoint_ref=bad.checkpoints[1].checkpoint_ref; break;
        case 24: bad.transfers[0].lower.channel_id=2; break;
        default: bad.transfers[0].lower.checkpoint_ref=current.accepted.checkpoints[1].checkpoint_ref; break;
        }
        UNCHANGED(core_sim_joint_accept(&f.plan,&current,&bad,&output),output,expected[i]);
        CHECK(current.completed_windows==0 && current.accepted.candidate_id==1);
    }
    CHECK(core_sim_joint_plan_validate(NULL)==CORE_SIM_JOINT_INVALID_ARGUMENT);
    CHECK(core_sim_joint_progress_validate(&f.plan,NULL)==CORE_SIM_JOINT_INVALID_ARGUMENT);
    CHECK(core_sim_joint_candidate_validate(&f.plan,&current,NULL)==CORE_SIM_JOINT_INVALID_ARGUMENT);
    CHECK(core_sim_joint_accept(&f.plan,&current,&good,NULL)==CORE_SIM_JOINT_INVALID_ARGUMENT);
    UNCHANGED(core_sim_joint_init(&f.plan,0,5,current.accepted.checkpoints,2,&output),output,CORE_SIM_JOINT_INCOMPLETE);
    UNCHANGED(core_sim_joint_init(&f.plan,5,0,current.accepted.checkpoints,2,&output),output,CORE_SIM_JOINT_INCOMPLETE);
    UNCHANGED(core_sim_joint_init(&f.plan,5,5,NULL,2,&output),output,CORE_SIM_JOINT_INVALID_ARGUMENT);
    UNCHANGED(core_sim_joint_init(&f.plan,5,5,current.accepted.checkpoints,1,&output),output,CORE_SIM_JOINT_INCOMPLETE);
    UNCHANGED(core_sim_joint_init(&f.plan,5,5,current.accepted.checkpoints,257,&output),output,CORE_SIM_JOINT_CAPACITY);
}

static void restart_and_plan(void) {
    Fixture f;
    CoreSimJointProgress current, broken;
    CoreSimJointCandidate c;
    CoreSimJointPlan bad;
    unsigned i;
    fixture(&f,0,120); initial(&f,&current); next(&f,&current,&c);
    OK(core_sim_joint_accept(&f.plan,&current,&c,&current));
    for (i=0;i<9;++i) {
        broken=current;
        switch (i) {
        case 0: broken.completed_windows=4; break;
        case 1: broken.accepted.plan_ref++; break;
        case 2: broken.accepted.checkpoints[0].time.timebase.domain++; break;
        case 3: broken.previous_checkpoints[1].time.ticks++; break;
        case 4: broken.accepted.expected_predecessor_id=0; break;
        case 5: broken.accepted.transfer_count=0; break;
        case 6: broken.accepted.window_index=1; break;
        case 7: broken.accepted.transfers[1].lower.checkpoint_ref++; break;
        default: broken.accepted.checkpoints[1].validation_ref=0; break;
        }
        CHECK(core_sim_joint_progress_validate(&f.plan,&broken)!=CORE_SIM_JOINT_OK);
    }
    bad=f.plan; bad.plan_ref++;
    CHECK(core_sim_joint_progress_validate(&bad,&current)==CORE_SIM_JOINT_IDENTITY_MISMATCH);
    bad=f.plan; bad.channel_count=65;
    CHECK(core_sim_joint_plan_validate(&bad)==CORE_SIM_JOINT_CAPACITY);
    bad=f.plan; bad.phase_count=257;
    CHECK(core_sim_joint_plan_validate(&bad)==CORE_SIM_JOINT_CAPACITY);
    bad=f.plan; bad.phase_count=1;
    CHECK(core_sim_joint_plan_validate(&bad)==CORE_SIM_JOINT_INCOMPLETE);
    bad=f.plan; bad.semantic_version++;
    CHECK(core_sim_joint_plan_validate(&bad)==CORE_SIM_JOINT_UNSUPPORTED);
    bad=f.plan; bad.required_features=1;
    CHECK(core_sim_joint_plan_validate(&bad)==CORE_SIM_JOINT_UNSUPPORTED);
    bad=f.plan; bad.schedule=NULL;
    CHECK(core_sim_joint_plan_validate(&bad)==CORE_SIM_JOINT_INVALID_ARGUMENT);
    f.order[1]=1;
    CHECK(core_sim_joint_plan_validate(&f.plan)==CORE_SIM_JOINT_IDENTITY_MISMATCH);
    f.order[1]=99;
    CHECK(core_sim_joint_plan_validate(&f.plan)==CORE_SIM_JOINT_IDENTITY_MISMATCH);
    f.order[0]=2; f.order[1]=1;
    CHECK(core_sim_joint_plan_validate(&f.plan)==CORE_SIM_JOINT_CAUSALITY);
    f.order[0]=1; f.order[1]=2; f.channels[1].channel_id=1;
    CHECK(core_sim_joint_plan_validate(&f.plan)==CORE_SIM_JOINT_IDENTITY_MISMATCH);
}

static void brackets_and_limits(void) {
    Fixture f;
    CoreSimJointProgress current, out;
    CoreSimJointCandidate c;
    fixture(&f,0,120);
    f.channels[0].kind=CORE_SIM_TEMPORAL_POINT; f.channels[0].policy=CORE_SIM_POLICY_LINEAR;
    initial(&f,&current);
    next(&f,&current,&c);
    c.transfers[0].lower=record(&f.channels[0],0,&current.accepted.checkpoints[0]);
    c.transfers[0].upper=record(&f.channels[0],0,&c.checkpoints[0]);
    c.transfers[0].upper.sequence=2; c.transfers[0].has_upper=1; c.transfers[0].request=point(20);
    OK(core_sim_joint_accept(&f.plan,&current,&c,&out));
    c.transfers[0].upper.sequence=1;
    UNCHANGED(core_sim_joint_accept(&f.plan,&current,&c,&out),out,CORE_SIM_JOINT_CAUSALITY);
    /* No channel is required for independent participants, but all checkpoints are. */
    fixture(&f,UINT64_MAX-40,UINT64_MAX); f.plan.channel_count=0; f.plan.channels=NULL;
    initial(&f,&current); next(&f,&current,&c); c.transfer_count=0;
    OK(core_sim_joint_accept(&f.plan,&current,&c,&out));
    CHECK(out.accepted.checkpoints[0].time.ticks==UINT64_MAX && out.completed_windows==1);
    OK(core_sim_joint_progress_validate(&f.plan,&out));
}

static void advertised_bounds(void) {
    Fixture f;
    CoreSimJointProgress current, out;
    CoreSimJointCandidate c;
    CoreSimChannelDesc channels[CORE_SIM_JOINT_MAX_CHANNELS];
    CoreSimParticipantSchedule participants[CORE_SIM_SCHEDULE_MAX_PARTICIPANTS];
    uint64_t order[CORE_SIM_SCHEDULE_MAX_PARTICIPANTS];
    CoreSimCheckpoint cp[CORE_SIM_SCHEDULE_MAX_PARTICIPANTS];
    size_t i;
    fixture(&f,0,120);
    for (i=0;i<CORE_SIM_JOINT_MAX_CHANNELS;++i) {
        channels[i]=f.channels[0]; channels[i].channel_id=i+1;
    }
    f.plan.channels=channels; f.plan.channel_count=CORE_SIM_JOINT_MAX_CHANNELS;
    initial(&f,&current); next(&f,&current,&c);
    c.transfer_count=CORE_SIM_JOINT_MAX_CHANNELS;
    for (i=1;i<c.transfer_count;++i) c.transfers[i]=c.transfers[0];
    for (i=0;i<c.transfer_count;++i) {
        c.transfers[i].lower.channel_id=i+1; c.transfers[i].lower.payload_ref=9000+i;
    }
    OK(core_sim_joint_accept(&f.plan,&current,&c,&out));
    OK(core_sim_joint_progress_validate(&f.plan,&out));
    /* Upper participant capacity with no channels. */
    for (i=0;i<CORE_SIM_SCHEDULE_MAX_PARTICIPANTS;++i) {
        participants[i]=(CoreSimParticipantSchedule){i+1,{base,1}}; order[i]=i+1;
        cp[i]=(CoreSimCheckpoint){i+1,1000+i,point(0),2000+i};
    }
    f.schedule.participants=participants; f.schedule.participant_count=CORE_SIM_SCHEDULE_MAX_PARTICIPANTS;
    f.plan.phase_order=order; f.plan.phase_count=CORE_SIM_SCHEDULE_MAX_PARTICIPANTS;
    f.plan.channel_count=0; f.plan.channels=NULL;
    OK(core_sim_joint_init(&f.plan,1,1,cp,CORE_SIM_SCHEDULE_MAX_PARTICIPANTS,&current));
    memset(&c,0,sizeof(c)); c.semantic_version=1; c.plan_id=17; c.plan_ref=55;
    c.candidate_id=2; c.expected_predecessor_id=1; c.validation_ref=2;
    c.checkpoint_count=CORE_SIM_SCHEDULE_MAX_PARTICIPANTS;
    for (i=0;i<c.checkpoint_count;++i) c.checkpoints[i]=(CoreSimCheckpoint){i+1,3000+i,point(40),4000+i};
    OK(core_sim_joint_accept(&f.plan,&current,&c,&out));
    OK(core_sim_joint_progress_validate(&f.plan,&out));
    /* Restored near-terminal metadata for UINT64_MAX unit windows. No giant
     * ledger/event array is required; history authentication remains host-owned. */
    fixture(&f,0,UINT64_MAX); f.schedule.exchange.ticks=1;
    f.participants[0].step.ticks=1; f.plan.channel_count=0;
    initial(&f,&current);
    current.completed_windows=UINT64_MAX-1;
    current.accepted.candidate_id=101; current.accepted.expected_predecessor_id=100;
    current.accepted.window_index=UINT64_MAX-2;
    for (i=0;i<2;++i) {
        current.previous_checkpoints[i]=(CoreSimCheckpoint){i+1,500+i,point(UINT64_MAX-2),800+i};
        current.accepted.checkpoints[i]=(CoreSimCheckpoint){i+1,600+i,point(UINT64_MAX-1),900+i};
    }
    OK(core_sim_joint_progress_validate(&f.plan,&current));
    c=current.accepted; c.candidate_id=102; c.expected_predecessor_id=101;
    c.window_index=UINT64_MAX-1;
    for (i=0;i<2;++i) c.checkpoints[i]=(CoreSimCheckpoint){i+1,700+i,point(UINT64_MAX),1000+i};
    OK(core_sim_joint_accept(&f.plan,&current,&c,&out));
    CHECK(out.completed_windows==UINT64_MAX);
    OK(core_sim_joint_progress_validate(&f.plan,&out));
    c.expected_predecessor_id=102; c.candidate_id=103;
    UNCHANGED(core_sim_joint_accept(&f.plan,&out,&c,&current),current,CORE_SIM_JOINT_COMPLETE);
}

int main(void) {
    transitions(); failures(); restart_and_plan(); brackets_and_limits(); advertised_bounds();
    printf("core_sim_progress_test: PASS (%lu checks)\n",checks);
    return 0;
}
