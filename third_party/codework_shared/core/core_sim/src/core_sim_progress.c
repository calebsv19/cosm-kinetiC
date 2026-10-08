#include "core_sim_progress.h"

static int same_time(CoreSimTimePoint a, CoreSimTimePoint b) {
    return a.timebase.domain == b.timebase.domain &&
        a.timebase.ticks_per_second == b.timebase.ticks_per_second && a.ticks == b.ticks;
}
static size_t participant_index(const CoreSimJointPlan *p, uint64_t id) {
    size_t i;
    for (i = 0; i < p->schedule->participant_count; ++i)
        if (p->schedule->participants[i].participant_id == id) return i;
    return p->schedule->participant_count;
}
static size_t phase_index(const CoreSimJointPlan *p, uint64_t id) {
    size_t i;
    for (i = 0; i < p->phase_count; ++i) if (p->phase_order[i] == id) return i;
    return p->phase_count;
}

CoreSimJointStatus core_sim_joint_plan_validate(const CoreSimJointPlan *p) {
    size_t i, j;
    if (!p) return CORE_SIM_JOINT_INVALID_ARGUMENT;
    if (p->semantic_version != CORE_SIM_JOINT_SEMANTIC_VERSION || p->required_features)
        return CORE_SIM_JOINT_UNSUPPORTED;
    if (!p->plan_ref || !p->phase_order || (p->channel_count && !p->channels))
        return CORE_SIM_JOINT_INVALID_ARGUMENT;
    if (p->channel_count > CORE_SIM_JOINT_MAX_CHANNELS ||
        p->phase_count > CORE_SIM_SCHEDULE_MAX_PARTICIPANTS) return CORE_SIM_JOINT_CAPACITY;
    if (core_sim_schedule_validate(p->schedule) != CORE_SIM_SCHEDULE_OK)
        return CORE_SIM_JOINT_INVALID_ARGUMENT;
    if (p->phase_count != p->schedule->participant_count) return CORE_SIM_JOINT_INCOMPLETE;
    for (i = 0; i < p->phase_count; ++i) {
        if (participant_index(p, p->phase_order[i]) == p->phase_count)
            return CORE_SIM_JOINT_IDENTITY_MISMATCH;
        for (j = 0; j < i; ++j)
            if (p->phase_order[i] == p->phase_order[j]) return CORE_SIM_JOINT_IDENTITY_MISMATCH;
    }
    for (i = 0; i < p->channel_count; ++i) {
        const CoreSimChannelDesc *c = &p->channels[i];
        if (core_sim_channel_validate(p->schedule, c) != CORE_SIM_EXCHANGE_OK)
            return CORE_SIM_JOINT_INVALID_ARGUMENT;
        for (j = 0; j < i; ++j)
            if (c->channel_id == p->channels[j].channel_id) return CORE_SIM_JOINT_IDENTITY_MISMATCH;
        if (c->kind == CORE_SIM_TEMPORAL_INTEGRATED &&
            phase_index(p,c->producer_id) >= phase_index(p,c->consumer_id))
            return CORE_SIM_JOINT_CAUSALITY;
    }
    return CORE_SIM_JOINT_OK;
}

static CoreSimJointStatus checkpoints_valid(const CoreSimJointPlan *p,
    const CoreSimCheckpoint *cp, size_t count, CoreSimTimePoint expected) {
    size_t i,j;
    if (count != p->schedule->participant_count) return CORE_SIM_JOINT_INCOMPLETE;
    if (!cp) return CORE_SIM_JOINT_INVALID_ARGUMENT;
    for (i = 0; i < count; ++i) {
        if (cp[i].participant_id != p->schedule->participants[i].participant_id)
            return CORE_SIM_JOINT_IDENTITY_MISMATCH;
        if (!cp[i].checkpoint_ref || !cp[i].validation_ref) return CORE_SIM_JOINT_INCOMPLETE;
        if (!same_time(cp[i].time,expected)) return CORE_SIM_JOINT_WRONG_TIME;
        for (j = 0; j < i; ++j)
            if (cp[i].checkpoint_ref == cp[j].checkpoint_ref) return CORE_SIM_JOINT_IDENTITY_MISMATCH;
    }
    return CORE_SIM_JOINT_OK;
}

static CoreSimJointStatus candidate_header(const CoreSimJointPlan *p, const CoreSimJointCandidate *c) {
    if (!c) return CORE_SIM_JOINT_INVALID_ARGUMENT;
    if (c->semantic_version != CORE_SIM_JOINT_SEMANTIC_VERSION || c->required_features)
        return CORE_SIM_JOINT_UNSUPPORTED;
    if (c->checkpoint_count > CORE_SIM_SCHEDULE_MAX_PARTICIPANTS ||
        c->transfer_count > CORE_SIM_JOINT_MAX_CHANNELS) return CORE_SIM_JOINT_CAPACITY;
    if (c->plan_id != p->schedule->plan_id || c->plan_ref != p->plan_ref)
        return CORE_SIM_JOINT_IDENTITY_MISMATCH;
    if (!c->candidate_id || !c->validation_ref) return CORE_SIM_JOINT_INCOMPLETE;
    return CORE_SIM_JOINT_OK;
}

static int record_checkpoint(const CoreSimExchangeRecord *r, const CoreSimCheckpoint *cp) {
    return r->checkpoint_ref == cp->checkpoint_ref && same_time(r->checkpoint_time,cp->time);
}

static CoreSimJointStatus candidate_body(const CoreSimJointPlan *p,
    const CoreSimJointCandidate *c, const CoreSimCheckpoint *prior) {
    CoreSimTimeInterval window;
    CoreSimJointStatus status;
    size_t i;
    if (core_sim_schedule_window(p->schedule,c->window_index,&window) != CORE_SIM_SCHEDULE_OK)
        return CORE_SIM_JOINT_WRONG_TIME;
    if (!c->expected_predecessor_id || c->expected_predecessor_id == c->candidate_id)
        return CORE_SIM_JOINT_IDENTITY_MISMATCH;
    status = checkpoints_valid(p,prior,p->schedule->participant_count,window.begin);
    if (status != CORE_SIM_JOINT_OK) return status;
    status = checkpoints_valid(p,c->checkpoints,c->checkpoint_count,window.end);
    if (status != CORE_SIM_JOINT_OK) return status;
    for (i = 0; i < c->checkpoint_count; ++i) {
        size_t j;
        for (j = 0; j < c->checkpoint_count; ++j)
            if (c->checkpoints[i].checkpoint_ref == prior[j].checkpoint_ref)
                return CORE_SIM_JOINT_IDENTITY_MISMATCH;
    }
    if (c->transfer_count != p->channel_count) return CORE_SIM_JOINT_INCOMPLETE;
    for (i = 0; i < p->channel_count; ++i) {
        const CoreSimChannelDesc *channel = &p->channels[i];
        const CoreSimJointTransfer *t = &c->transfers[i];
        size_t producer = participant_index(p,channel->producer_id);
        size_t consumer = participant_index(p,channel->consumer_id);
        int staged_available = phase_index(p,channel->producer_id) < phase_index(p,channel->consumer_id);
        const CoreSimCheckpoint *frontier = staged_available ? &c->checkpoints[producer] : &prior[producer];
        const CoreSimExchangeRecord *records[2] = {&t->lower,&t->upper};
        size_t j;
        if (!t->validation_ref || !t->consumer_checkpoint_ref) return CORE_SIM_JOINT_INCOMPLETE;
        if (t->consumer_checkpoint_ref != c->checkpoints[consumer].checkpoint_ref)
            return CORE_SIM_JOINT_IDENTITY_MISMATCH;
        if (t->has_upper != 0 && t->has_upper != 1) return CORE_SIM_JOINT_INVALID_ARGUMENT;
        for (j = 0; j < (size_t)(1+t->has_upper); ++j) {
            const CoreSimExchangeRecord *r = records[j];
            if (r->window_index != c->window_index) return CORE_SIM_JOINT_WRONG_TIME;
            if (!record_checkpoint(r,&prior[producer]) &&
                !(staged_available && record_checkpoint(r,&c->checkpoints[producer])))
                return CORE_SIM_JOINT_CAUSALITY;
            if (core_sim_exchange_record_admit(p->schedule,channel,r,frontier->time) != CORE_SIM_EXCHANGE_OK)
                return CORE_SIM_JOINT_IDENTITY_MISMATCH;
        }
        if (channel->kind == CORE_SIM_TEMPORAL_INTEGRATED) {
            if (t->has_upper || !same_time(t->request,window.begin) ||
                !same_time(t->lower.support.interval.begin,window.begin) ||
                !same_time(t->lower.support.interval.end,window.end)) return CORE_SIM_JOINT_WRONG_TIME;
        } else {
            CoreSimExchangeSample sample;
            if (core_sim_exchange_sample(p->schedule,channel,t->request,frontier->time,
                &t->lower,t->has_upper ? &t->upper : NULL,&sample) != CORE_SIM_EXCHANGE_OK)
                return CORE_SIM_JOINT_CAUSALITY;
        }
    }
    return CORE_SIM_JOINT_OK;
}

CoreSimJointStatus core_sim_joint_init(const CoreSimJointPlan *p,
    uint64_t initial_id, uint64_t validation_ref,
    const CoreSimCheckpoint *checkpoints, size_t count, CoreSimJointProgress *out) {
    CoreSimJointProgress result = {0};
    CoreSimJointStatus status;
    size_t i;
    if (!out) return CORE_SIM_JOINT_INVALID_ARGUMENT;
    status = core_sim_joint_plan_validate(p);
    if (status != CORE_SIM_JOINT_OK) return status;
    if (count > CORE_SIM_SCHEDULE_MAX_PARTICIPANTS) return CORE_SIM_JOINT_CAPACITY;
    if (!initial_id || !validation_ref) return CORE_SIM_JOINT_INCOMPLETE;
    status = checkpoints_valid(p,checkpoints,count,p->schedule->horizon.begin);
    if (status != CORE_SIM_JOINT_OK) return status;
    result.accepted.semantic_version = CORE_SIM_JOINT_SEMANTIC_VERSION;
    result.accepted.plan_id = p->schedule->plan_id;
    result.accepted.plan_ref = p->plan_ref;
    result.accepted.candidate_id = initial_id;
    result.accepted.validation_ref = validation_ref;
    result.accepted.checkpoint_count = count;
    for (i = 0; i < count; ++i) result.accepted.checkpoints[i] = checkpoints[i];
    *out = result;
    return CORE_SIM_JOINT_OK;
}

CoreSimJointStatus core_sim_joint_progress_validate(const CoreSimJointPlan *p,
    const CoreSimJointProgress *progress) {
    CoreSimJointStatus status = core_sim_joint_plan_validate(p);
    uint64_t windows;
    if (status != CORE_SIM_JOINT_OK) return status;
    if (!progress) return CORE_SIM_JOINT_INVALID_ARGUMENT;
    status = candidate_header(p,&progress->accepted);
    if (status != CORE_SIM_JOINT_OK) return status;
    (void)core_sim_schedule_window_count(p->schedule,&windows);
    if (progress->completed_windows > windows) return CORE_SIM_JOINT_WRONG_TIME;
    if (!progress->completed_windows) {
        if (progress->accepted.expected_predecessor_id || progress->accepted.transfer_count ||
            progress->accepted.window_index) return CORE_SIM_JOINT_IDENTITY_MISMATCH;
        return checkpoints_valid(p,progress->accepted.checkpoints,progress->accepted.checkpoint_count,
            p->schedule->horizon.begin);
    }
    if (progress->accepted.window_index != progress->completed_windows-1) return CORE_SIM_JOINT_WRONG_TIME;
    return candidate_body(p,&progress->accepted,progress->previous_checkpoints);
}

static int candidate_equal(const CoreSimJointPlan *p, const CoreSimJointCandidate *a,
    const CoreSimJointCandidate *b) {
    size_t i;
    if (a->expected_predecessor_id != b->expected_predecessor_id || a->window_index != b->window_index ||
        a->validation_ref != b->validation_ref) return 0;
    for (i = 0; i < a->checkpoint_count; ++i) {
        const CoreSimCheckpoint *x = &a->checkpoints[i], *y = &b->checkpoints[i];
        if (x->checkpoint_ref != y->checkpoint_ref || x->validation_ref != y->validation_ref) return 0;
    }
    for (i = 0; i < a->transfer_count; ++i) {
        const CoreSimJointTransfer *x = &a->transfers[i], *y = &b->transfers[i];
        CoreSimExchangeRelation relation;
        if (x->has_upper != y->has_upper || !same_time(x->request,y->request) ||
            x->consumer_checkpoint_ref != y->consumer_checkpoint_ref || x->validation_ref != y->validation_ref)
            return 0;
        if (core_sim_exchange_record_compare(p->schedule,&p->channels[i],&x->lower,&y->lower,&relation) !=
            CORE_SIM_EXCHANGE_OK || relation != CORE_SIM_EXCHANGE_REPLAY) return 0;
        if (x->has_upper && (core_sim_exchange_record_compare(p->schedule,&p->channels[i],&x->upper,&y->upper,&relation) !=
            CORE_SIM_EXCHANGE_OK || relation != CORE_SIM_EXCHANGE_REPLAY)) return 0;
    }
    return 1;
}

CoreSimJointStatus core_sim_joint_candidate_validate(const CoreSimJointPlan *p,
    const CoreSimJointProgress *current, const CoreSimJointCandidate *c) {
    CoreSimJointStatus status = core_sim_joint_progress_validate(p,current);
    uint64_t windows;
    if (status != CORE_SIM_JOINT_OK) return status;
    status = candidate_header(p,c);
    if (status != CORE_SIM_JOINT_OK) return status;
    if (c->candidate_id == current->accepted.candidate_id ||
        (current->completed_windows && c->window_index == current->accepted.window_index &&
         c->expected_predecessor_id == current->accepted.expected_predecessor_id)) {
        if (!current->completed_windows) return CORE_SIM_JOINT_CONFLICT;
        status = candidate_body(p,c,current->previous_checkpoints);
        if (status != CORE_SIM_JOINT_OK) return status;
        return c->candidate_id == current->accepted.candidate_id && candidate_equal(p,c,&current->accepted) ?
            CORE_SIM_JOINT_REPLAY : CORE_SIM_JOINT_CONFLICT;
    }
    if (c->expected_predecessor_id != current->accepted.candidate_id)
        return CORE_SIM_JOINT_STALE_PREDECESSOR;
    (void)core_sim_schedule_window_count(p->schedule,&windows);
    if (current->completed_windows == windows) return CORE_SIM_JOINT_COMPLETE;
    if (c->window_index != current->completed_windows) return CORE_SIM_JOINT_WRONG_TIME;
    return candidate_body(p,c,current->accepted.checkpoints);
}

CoreSimJointStatus core_sim_joint_accept(const CoreSimJointPlan *p,
    const CoreSimJointProgress *current, const CoreSimJointCandidate *c, CoreSimJointProgress *out) {
    CoreSimJointProgress result = {0};
    CoreSimJointStatus status;
    size_t i;
    if (!out) return CORE_SIM_JOINT_INVALID_ARGUMENT;
    status = core_sim_joint_candidate_validate(p,current,c);
    if (status != CORE_SIM_JOINT_OK) return status;
    /* current < window_count <= UINT64_MAX, so increment cannot overflow. */
    result.completed_windows = current->completed_windows+1;
    result.accepted = *c;
    for (i = 0; i < current->accepted.checkpoint_count; ++i)
        result.previous_checkpoints[i] = current->accepted.checkpoints[i];
    *out = result;
    return CORE_SIM_JOINT_OK;
}
