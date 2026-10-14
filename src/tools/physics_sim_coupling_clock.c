/* Application adapter for core_sim. Hosts authenticate all referenced contents. */
#include "core_sim_progress.h"
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static struct json_object *get(struct json_object *o, const char *key) {
    struct json_object *v=NULL; if (o) json_object_object_get_ex(o,key,&v); return v;
}
static int integer(struct json_object *v, uint64_t *out) {
    if (!v || !json_object_is_type(v,json_type_int)) return 0;
    int64_t n=json_object_get_int64(v);
    if (n<0 || n>9007199254740991LL) return 0;
    *out=(uint64_t)n; return 1;
}
static int member(struct json_object *o, const char *key, uint64_t *out) {
    return integer(get(o,key),out);
}
static void add(struct json_object *o,const char *key,uint64_t n) {
    json_object_object_add(o,key,json_object_new_int64((int64_t)n));
}
static int ratio(struct json_object *v, CoreSimTimeRatio *r) {
    return v && json_object_is_type(v,json_type_array) && json_object_array_length(v)==2 &&
        integer(json_object_array_get_idx(v,0),&r->numerator) && r->numerator &&
        integer(json_object_array_get_idx(v,1),&r->denominator) && r->denominator;
}
static void checkpoint(CoreSimCheckpoint *cp,uint64_t id,uint64_t ref,CoreSimTimePoint t,uint64_t validation) {
    *cp=(CoreSimCheckpoint){.participant_id=id,.checkpoint_ref=ref,.time=t,.validation_ref=validation};
}
int main(int argc,char **argv) {
    struct stat st;
    if (argc!=2 || stat(argv[1],&st) || st.st_size<=0 || st.st_size>1024*1024) return 2;
    struct json_object *r=json_object_from_file(argv[1]), *out=NULL;
    CoreSimTimeRatio periods[3]; CoreSimTimebase base; CoreSimDuration duration[3],total;
    CoreSimTimePoint begin,end; CoreSimTimeInterval horizon;
    CoreSimParticipantSchedule participants[2]; CoreSimSchedule schedule;
    CoreSimChannelDesc channels[3]={0}; uint64_t phases[2]={1,2};
    CoreSimJointPlan joint; CoreSimJointProgress *progress=calloc(1,sizeof(*progress));
    CoreSimJointProgress *next=calloc(1,sizeof(*next));
    CoreSimJointCandidate *candidate=calloc(1,sizeof(*candidate));
    uint64_t domain,plan_id,plan_ref,windows,initial_id,validation,refs[2],held=0; int status=1;
    struct json_object *p=get(r,"periods"), *init=get(r,"initial_checkpoints"), *history=get(r,"history");
    if (!r || !progress || !next || !candidate || !p || !json_object_is_type(p,json_type_array) ||
        json_object_array_length(p)!=3 || !member(r,"domain",&domain) || !domain ||
        !member(r,"plan_id",&plan_id) || !plan_id || !member(r,"plan_ref",&plan_ref) || !plan_ref ||
        !member(r,"windows",&windows) || !windows || windows>32 ||
        !member(r,"initial_id",&initial_id) || !initial_id || !member(r,"initial_validation",&validation) || !validation ||
        !init || !json_object_is_type(init,json_type_array) || json_object_array_length(init)!=2 ||
        !integer(json_object_array_get_idx(init,0),&refs[0]) || !refs[0] ||
        !integer(json_object_array_get_idx(init,1),&refs[1]) || !refs[1] ||
        !history || !json_object_is_type(history,json_type_array) || json_object_array_length(history)>windows) goto done;
    if (get(r,"held_return") && (!member(r,"held_return",&held) || held>1)) goto done;
    for (size_t i=0;i<3;i++) if (!ratio(json_object_array_get_idx(p,i),&periods[i])) goto done;
    if (core_sim_timebase_from_periods(domain,periods,3,1000000000,&base)!=CORE_SIM_TIME_OK) goto done;
    for (size_t i=0;i<3;i++) if (core_sim_duration_from_seconds(base,periods[i],&duration[i])!=CORE_SIM_TIME_OK) goto done;
    if (core_sim_duration_scale(duration[2],(CoreSimTimeRatio){windows,1},&total)!=CORE_SIM_TIME_OK ||
        core_sim_time_point_make(base,0,&begin)!=CORE_SIM_TIME_OK ||
        core_sim_time_point_add(begin,total,&end)!=CORE_SIM_TIME_OK ||
        core_sim_time_interval_make(begin,end,&horizon)!=CORE_SIM_TIME_OK) goto done;
    if (end.ticks>9007199254740991ULL) goto done; /* host JSON identity bound */
    participants[0]=(CoreSimParticipantSchedule){1,duration[0]};
    participants[1]=(CoreSimParticipantSchedule){2,duration[1]};
    if (core_sim_schedule_init(plan_id,horizon,duration[2],participants,2,&schedule)!=CORE_SIM_SCHEDULE_OK) goto done;
    for (size_t i=0;i<2;i++) channels[i]=(CoreSimChannelDesc){
        .semantic_version=1,.plan_id=plan_id,.channel_id=i+1,.producer_id=1,.consumer_id=2,
        .quantity_id=i+1,.unit_id=i+1,.spatial_support_id=1,.adapter_id=1,.profile_id=1,
        .kind=CORE_SIM_TEMPORAL_INTEGRATED,.policy=CORE_SIM_POLICY_UNIFORM_RATE,.max_age={base,0}};
    if (held) channels[2]=(CoreSimChannelDesc){.semantic_version=1,.plan_id=plan_id,
        .channel_id=3,.producer_id=2,.consumer_id=1,.quantity_id=3,.unit_id=3,
        .spatial_support_id=1,.adapter_id=2,.profile_id=2,
        .kind=CORE_SIM_TEMPORAL_POINT,.policy=CORE_SIM_POLICY_HOLD,.max_age=duration[2]};
    joint=(CoreSimJointPlan){.semantic_version=1,.plan_ref=plan_ref,.schedule=&schedule,
        .channels=channels,.channel_count=2+held,.phase_order=phases,.phase_count=2};
    CoreSimCheckpoint cps[2];
    for (size_t i=0;i<2;i++) checkpoint(&cps[i],i+1,refs[i],begin,validation);
    if (core_sim_joint_init(&joint,initial_id,validation,cps,2,progress)!=CORE_SIM_JOINT_OK) goto done;
    out=json_object_new_object(); add(out,"rate",base.ticks_per_second);
    struct json_object *ws=json_object_new_array(); json_object_object_add(out,"schedule",ws);
    for (uint64_t w=0;w<windows;w++) {
        CoreSimTimeInterval window; CoreSimWindowSteps fire,fluid;
        if (core_sim_schedule_window(&schedule,w,&window)!=CORE_SIM_SCHEDULE_OK ||
            core_sim_schedule_window_steps(&schedule,1,w,&fire)!=CORE_SIM_SCHEDULE_OK ||
            core_sim_schedule_window_steps(&schedule,2,w,&fluid)!=CORE_SIM_SCHEDULE_OK ||
            fire.step_count>256 || fluid.step_count>256) goto done;
        struct json_object *row=json_object_new_object(), *steps=json_object_new_array();
        add(row,"begin",window.begin.ticks);add(row,"end",window.end.ticks);
        add(row,"fire_steps",fire.step_count);add(row,"fluid_steps",fluid.step_count);
        json_object_object_add(row,"steps",steps);json_object_array_add(ws,row);
        CoreSimExchangeRecord record={.semantic_version=1,.plan_id=plan_id,.channel_id=1,
            .window_index=w,.sequence=1,.checkpoint_ref=1,.payload_ref=1,
            .kind=CORE_SIM_TEMPORAL_INTEGRATED,.checkpoint_time=window.end,.support.interval=window};
        for (uint64_t i=0;i<fluid.step_count;i++) {
            CoreSimTimeInterval target;CoreSimExchangeOverlap left={0},right;
            if (core_sim_schedule_step(&schedule,2,fluid.first_step_index+i,&target)!=CORE_SIM_SCHEDULE_OK) goto done;
            if (i) {
                CoreSimTimeInterval prefix={window.begin,target.begin};
                if (core_sim_exchange_overlap(&schedule,&channels[0],&record,window.end,prefix,&left)!=CORE_SIM_EXCHANGE_OK) goto done;
            } else left.source_fraction=(CoreSimTimeRatio){0,1};
            CoreSimTimeInterval prefix={window.begin,target.end};
            if (core_sim_exchange_overlap(&schedule,&channels[0],&record,window.end,prefix,&right)!=CORE_SIM_EXCHANGE_OK) goto done;
            struct json_object *s=json_object_new_object();add(s,"begin",target.begin.ticks);add(s,"end",target.end.ticks);
            add(s,"left_n",left.source_fraction.numerator);add(s,"left_d",left.source_fraction.denominator);
            add(s,"right_n",right.source_fraction.numerator);add(s,"right_d",right.source_fraction.denominator);
            json_object_array_add(steps,s);
        }
    }
    for (size_t w=0;w<json_object_array_length(history);w++) {
        struct json_object *row=json_object_array_get_idx(history,w),*cp=get(row,"checkpoints");
        uint64_t id,pred,index,payload;
        if (!member(row,"id",&id) || !id || !member(row,"predecessor",&pred) || !pred ||
            !member(row,"window",&index) || index!=w || !member(row,"validation",&validation) || !validation ||
            !member(row,"payload",&payload) || !payload || !cp || !json_object_is_type(cp,json_type_array) ||
            json_object_array_length(cp)!=2 || !integer(json_object_array_get_idx(cp,0),&refs[0]) || !refs[0] ||
            !integer(json_object_array_get_idx(cp,1),&refs[1]) || !refs[1]) goto done;
        CoreSimTimeInterval window;
        if (core_sim_schedule_window(&schedule,w,&window)!=CORE_SIM_SCHEDULE_OK) goto done;
        *candidate=(CoreSimJointCandidate){.semantic_version=1,.plan_id=plan_id,.plan_ref=plan_ref,
            .candidate_id=id,.expected_predecessor_id=pred,.window_index=w,.validation_ref=validation,
            .checkpoint_count=2,.transfer_count=2+held};
        for (size_t i=0;i<2;i++) {
            checkpoint(&candidate->checkpoints[i],i+1,refs[i],window.end,validation);
            candidate->transfers[i]=(CoreSimJointTransfer){.lower={.semantic_version=1,.plan_id=plan_id,
                .channel_id=i+1,.window_index=w,.sequence=1,.checkpoint_ref=refs[0],.payload_ref=payload,
                .kind=CORE_SIM_TEMPORAL_INTEGRATED,.checkpoint_time=window.end,.support.interval=window},
                .request=window.begin,.consumer_checkpoint_ref=refs[1],.validation_ref=validation};
        }
        if (held) {
            uint64_t return_ref,return_payload,return_tick,request_tick;
            CoreSimTimePoint sample,request;
            if (!member(row,"return_checkpoint",&return_ref) || !return_ref ||
                !member(row,"return_payload",&return_payload) || !return_payload ||
                !member(row,"return_tick",&return_tick) || !member(row,"return_request",&request_tick) ||
                core_sim_time_point_make(base,return_tick,&sample)!=CORE_SIM_TIME_OK ||
                core_sim_time_point_make(base,request_tick,&request)!=CORE_SIM_TIME_OK) goto done;
            candidate->transfers[2]=(CoreSimJointTransfer){.lower={.semantic_version=1,
                .plan_id=plan_id,.channel_id=3,.window_index=w,.sequence=1,
                .checkpoint_ref=return_ref,.payload_ref=return_payload,.kind=CORE_SIM_TEMPORAL_POINT,
                .checkpoint_time=sample,.support.point=sample},.request=request,
                .consumer_checkpoint_ref=refs[0],.validation_ref=validation};
        }
        if (core_sim_joint_accept(&joint,progress,candidate,next)!=CORE_SIM_JOINT_OK) goto done;
        *progress=*next;
        /* The exact last candidate must be idempotent, without another advance. */
        if (core_sim_joint_accept(&joint,progress,candidate,next)!=CORE_SIM_JOINT_REPLAY) goto done;
    }
    add(out,"completed_windows",progress->completed_windows);add(out,"accepted_id",progress->accepted.candidate_id);
    puts(json_object_to_json_string_ext(out,JSON_C_TO_STRING_PLAIN));status=0;
done:
    if (status) fputs("core_sim exact schedule or joint transition rejected\n",stderr);
    if (out) json_object_put(out);
    if (r) json_object_put(r);
    free(candidate);free(next);free(progress);return status;
}
