#include "core_sim_exchange.h"

static int same_base(CoreSimTimebase a, CoreSimTimebase b) {
    return a.domain == b.domain && a.ticks_per_second == b.ticks_per_second;
}

static CoreSimExchangeStatus schedule_status(CoreSimScheduleStatus status) {
    if (status == CORE_SIM_SCHEDULE_OK) return CORE_SIM_EXCHANGE_OK;
    if (status == CORE_SIM_SCHEDULE_DOMAIN_MISMATCH) return CORE_SIM_EXCHANGE_DOMAIN_MISMATCH;
    if (status == CORE_SIM_SCHEDULE_OUT_OF_RANGE) return CORE_SIM_EXCHANGE_OUT_OF_RANGE;
    return CORE_SIM_EXCHANGE_INVALID_ARGUMENT;
}

static CoreSimExchangeStatus point_valid(const CoreSimSchedule *plan, CoreSimTimePoint p) {
    if (!p.timebase.domain || !p.timebase.ticks_per_second) return CORE_SIM_EXCHANGE_INVALID_ARGUMENT;
    if (!same_base(p.timebase, plan->horizon.begin.timebase)) return CORE_SIM_EXCHANGE_DOMAIN_MISMATCH;
    if (p.ticks < plan->horizon.begin.ticks || p.ticks > plan->horizon.end.ticks)
        return CORE_SIM_EXCHANGE_OUT_OF_RANGE;
    return CORE_SIM_EXCHANGE_OK;
}

static CoreSimExchangeStatus interval_valid(const CoreSimSchedule *plan, CoreSimTimeInterval v) {
    CoreSimExchangeStatus status = point_valid(plan, v.begin);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    status = point_valid(plan, v.end);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    return v.begin.ticks < v.end.ticks ? CORE_SIM_EXCHANGE_OK : CORE_SIM_EXCHANGE_INVALID_ARGUMENT;
}

CoreSimExchangeStatus core_sim_channel_validate(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel) {
    CoreSimExchangeStatus status = schedule_status(core_sim_schedule_validate(plan));
    size_t i;
    int producer = 0, consumer = 0;
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    if (!channel) return CORE_SIM_EXCHANGE_INVALID_ARGUMENT;
    if (channel->semantic_version != CORE_SIM_EXCHANGE_SEMANTIC_VERSION || channel->required_features)
        return CORE_SIM_EXCHANGE_UNSUPPORTED;
    if (!channel->plan_id || !channel->channel_id || !channel->producer_id || !channel->consumer_id ||
        !channel->quantity_id || !channel->unit_id || !channel->spatial_support_id ||
        !channel->adapter_id || !channel->profile_id ||
        channel->producer_id == channel->consumer_id ||
        !channel->max_age.timebase.domain || !channel->max_age.timebase.ticks_per_second)
        return CORE_SIM_EXCHANGE_INVALID_ARGUMENT;
    if (channel->plan_id != plan->plan_id) return CORE_SIM_EXCHANGE_IDENTITY_MISMATCH;
    if (!same_base(channel->max_age.timebase, plan->horizon.begin.timebase))
        return CORE_SIM_EXCHANGE_DOMAIN_MISMATCH;
    for (i = 0; i < plan->participant_count; ++i) {
        producer |= plan->participants[i].participant_id == channel->producer_id;
        consumer |= plan->participants[i].participant_id == channel->consumer_id;
    }
    if (!producer || !consumer) return CORE_SIM_EXCHANGE_IDENTITY_MISMATCH;
    switch (channel->kind) {
    case CORE_SIM_TEMPORAL_INTEGRATED:
        if (channel->policy != CORE_SIM_POLICY_UNIFORM_RATE) return CORE_SIM_EXCHANGE_UNSUPPORTED;
        break;
    case CORE_SIM_TEMPORAL_POINT:
        if (channel->policy != CORE_SIM_POLICY_EXACT && channel->policy != CORE_SIM_POLICY_HOLD &&
            channel->policy != CORE_SIM_POLICY_LINEAR) return CORE_SIM_EXCHANGE_UNSUPPORTED;
        break;
    case CORE_SIM_TEMPORAL_DISCRETE:
        if (channel->policy != CORE_SIM_POLICY_EXACT && channel->policy != CORE_SIM_POLICY_HOLD)
            return CORE_SIM_EXCHANGE_UNSUPPORTED;
        break;
    default: return CORE_SIM_EXCHANGE_UNSUPPORTED;
    }
    if ((channel->policy == CORE_SIM_POLICY_HOLD) != (channel->max_age.ticks != 0))
        return CORE_SIM_EXCHANGE_INVALID_ARGUMENT;
    return CORE_SIM_EXCHANGE_OK;
}

CoreSimExchangeStatus core_sim_exchange_record_validate(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel, const CoreSimExchangeRecord *record) {
    CoreSimTimeInterval window;
    CoreSimExchangeStatus status = core_sim_channel_validate(plan, channel);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    if (!record) return CORE_SIM_EXCHANGE_INVALID_ARGUMENT;
    if (record->semantic_version != CORE_SIM_EXCHANGE_SEMANTIC_VERSION || record->required_features)
        return CORE_SIM_EXCHANGE_UNSUPPORTED;
    if (!record->sequence || !record->checkpoint_ref || !record->payload_ref)
        return CORE_SIM_EXCHANGE_INVALID_ARGUMENT;
    if (record->plan_id != plan->plan_id || record->channel_id != channel->channel_id ||
        record->kind != channel->kind) return CORE_SIM_EXCHANGE_IDENTITY_MISMATCH;
    status = schedule_status(core_sim_schedule_window(plan, record->window_index, &window));
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    status = point_valid(plan, record->checkpoint_time);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    if (record->kind == CORE_SIM_TEMPORAL_INTEGRATED) {
        status = interval_valid(plan, record->support.interval);
        if (status != CORE_SIM_EXCHANGE_OK) return status;
        if (record->support.interval.begin.ticks < window.begin.ticks ||
            record->support.interval.end.ticks > window.end.ticks ||
            record->checkpoint_time.ticks != record->support.interval.end.ticks)
            return CORE_SIM_EXCHANGE_OUT_OF_RANGE;
    } else {
        status = point_valid(plan, record->support.point);
        if (status != CORE_SIM_EXCHANGE_OK) return status;
        if (record->support.point.ticks > window.end.ticks ||
            record->support.point.ticks != record->checkpoint_time.ticks)
            return CORE_SIM_EXCHANGE_OUT_OF_RANGE;
    }
    return CORE_SIM_EXCHANGE_OK;
}

CoreSimExchangeStatus core_sim_exchange_record_admit(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel, const CoreSimExchangeRecord *record,
    CoreSimTimePoint available_through) {
    CoreSimExchangeStatus status = core_sim_exchange_record_validate(plan, channel, record);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    status = point_valid(plan, available_through);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    if (record->checkpoint_time.ticks > available_through.ticks) return CORE_SIM_EXCHANGE_FUTURE;
    return CORE_SIM_EXCHANGE_OK;
}

CoreSimExchangeStatus core_sim_exchange_record_compare(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel, const CoreSimExchangeRecord *a,
    const CoreSimExchangeRecord *b, CoreSimExchangeRelation *out) {
    CoreSimExchangeRelation result;
    CoreSimExchangeStatus status;
    int equal;
    if (!out) return CORE_SIM_EXCHANGE_INVALID_ARGUMENT;
    status = core_sim_exchange_record_validate(plan, channel, a);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    status = core_sim_exchange_record_validate(plan, channel, b);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    if (a->window_index != b->window_index || a->sequence != b->sequence)
        result = CORE_SIM_EXCHANGE_DISTINCT;
    else {
        equal = a->checkpoint_ref == b->checkpoint_ref && a->payload_ref == b->payload_ref &&
            a->checkpoint_time.ticks == b->checkpoint_time.ticks;
        if (a->kind == CORE_SIM_TEMPORAL_INTEGRATED)
            equal &= a->support.interval.begin.ticks == b->support.interval.begin.ticks &&
                a->support.interval.end.ticks == b->support.interval.end.ticks;
        else equal &= a->support.point.ticks == b->support.point.ticks;
        result = equal ? CORE_SIM_EXCHANGE_REPLAY : CORE_SIM_EXCHANGE_CONFLICT;
    }
    *out = result;
    return CORE_SIM_EXCHANGE_OK;
}

CoreSimExchangeStatus core_sim_exchange_sample(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel, CoreSimTimePoint request,
    CoreSimTimePoint available_through, const CoreSimExchangeRecord *lower,
    const CoreSimExchangeRecord *upper, CoreSimExchangeSample *out) {
    CoreSimExchangeSample result;
    CoreSimTimeInterval window;
    CoreSimExchangeStatus status;
    uint64_t lo, hi;
    if (!out) return CORE_SIM_EXCHANGE_INVALID_ARGUMENT;
    status = core_sim_exchange_record_admit(plan, channel, lower, available_through);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    if (channel->kind == CORE_SIM_TEMPORAL_INTEGRATED) return CORE_SIM_EXCHANGE_UNSUPPORTED;
    status = point_valid(plan, request);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    /* record_admit has validated this index. */
    (void)core_sim_schedule_window(plan, lower->window_index, &window);
    if (request.ticks < window.begin.ticks || request.ticks > window.end.ticks)
        return CORE_SIM_EXCHANGE_OUT_OF_RANGE;
    lo = lower->support.point.ticks;
    if (lo > request.ticks) return CORE_SIM_EXCHANGE_FUTURE;
    result.lower_sequence = result.upper_sequence = lower->sequence;
    result.weight = (CoreSimTimeRatio){0, 1};
    result.age = (CoreSimDuration){request.timebase, request.ticks - lo};
    if (channel->policy != CORE_SIM_POLICY_LINEAR && upper)
        return CORE_SIM_EXCHANGE_POLICY_UNAVAILABLE;
    if (channel->policy == CORE_SIM_POLICY_HOLD) {
        if (result.age.ticks > channel->max_age.ticks) return CORE_SIM_EXCHANGE_STALE;
    } else if (!upper) {
        if (lo != request.ticks) return CORE_SIM_EXCHANGE_POLICY_UNAVAILABLE;
    } else {
        status = core_sim_exchange_record_admit(plan, channel, upper, available_through);
        if (status != CORE_SIM_EXCHANGE_OK) return status;
        if (upper->window_index != lower->window_index) return CORE_SIM_EXCHANGE_IDENTITY_MISMATCH;
        hi = upper->support.point.ticks;
        if (hi <= lo || lower->sequence >= upper->sequence || request.ticks < lo || request.ticks > hi)
            return CORE_SIM_EXCHANGE_POLICY_UNAVAILABLE;
        result.upper_sequence = upper->sequence;
        result.age.ticks = 0;
        (void)core_sim_time_ratio_make(request.ticks - lo, hi - lo, &result.weight);
    }
    *out = result;
    return CORE_SIM_EXCHANGE_OK;
}

CoreSimExchangeStatus core_sim_exchange_overlap(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel, const CoreSimExchangeRecord *source,
    CoreSimTimePoint available_through, CoreSimTimeInterval target,
    CoreSimExchangeOverlap *out) {
    CoreSimExchangeOverlap result;
    CoreSimExchangeStatus status;
    uint64_t begin, end, ticks;
    if (!out) return CORE_SIM_EXCHANGE_INVALID_ARGUMENT;
    status = core_sim_exchange_record_admit(plan, channel, source, available_through);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    if (channel->kind != CORE_SIM_TEMPORAL_INTEGRATED) return CORE_SIM_EXCHANGE_UNSUPPORTED;
    status = interval_valid(plan, target);
    if (status != CORE_SIM_EXCHANGE_OK) return status;
    begin = source->support.interval.begin.ticks > target.begin.ticks ?
        source->support.interval.begin.ticks : target.begin.ticks;
    end = source->support.interval.end.ticks < target.end.ticks ?
        source->support.interval.end.ticks : target.end.ticks;
    ticks = end > begin ? end - begin : 0;
    result.duration = (CoreSimDuration){target.begin.timebase, ticks};
    (void)core_sim_time_ratio_make(ticks,
        source->support.interval.end.ticks - source->support.interval.begin.ticks, &result.source_fraction);
    (void)core_sim_time_ratio_make(ticks, target.end.ticks - target.begin.ticks, &result.target_fraction);
    *out = result;
    return CORE_SIM_EXCHANGE_OK;
}
