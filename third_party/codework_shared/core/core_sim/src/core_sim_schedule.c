#include "core_sim_schedule.h"

static int same_base(CoreSimTimebase a, CoreSimTimebase b) {
    return a.domain == b.domain && a.ticks_per_second == b.ticks_per_second;
}

CoreSimScheduleStatus core_sim_schedule_validate(const CoreSimSchedule *plan) {
    CoreSimTimeInterval interval;
    CoreSimTimeStatus status;
    uint64_t duration;
    size_t i, j;
    if (!plan || !plan->plan_id || !plan->participants || !plan->participant_count)
        return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
    if (plan->participant_count > CORE_SIM_SCHEDULE_MAX_PARTICIPANTS)
        return CORE_SIM_SCHEDULE_CAPACITY;
    status = core_sim_time_interval_make(plan->horizon.begin, plan->horizon.end, &interval);
    if (status == CORE_SIM_TIME_DOMAIN_MISMATCH) return CORE_SIM_SCHEDULE_DOMAIN_MISMATCH;
    if (status != CORE_SIM_TIME_OK || !plan->exchange.ticks ||
        !plan->exchange.timebase.domain || !plan->exchange.timebase.ticks_per_second)
        return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
    if (!same_base(plan->horizon.begin.timebase, plan->exchange.timebase))
        return CORE_SIM_SCHEDULE_DOMAIN_MISMATCH;
    duration = interval.end.ticks - interval.begin.ticks;
    if (duration % plan->exchange.ticks) return CORE_SIM_SCHEDULE_UNALIGNED;
    for (i = 0; i < plan->participant_count; ++i) {
        const CoreSimParticipantSchedule *p = &plan->participants[i];
        if (!p->participant_id || !p->step.ticks || !p->step.timebase.domain ||
            !p->step.timebase.ticks_per_second) return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
        if (!same_base(p->step.timebase, interval.begin.timebase))
            return CORE_SIM_SCHEDULE_DOMAIN_MISMATCH;
        if (plan->exchange.ticks % p->step.ticks) return CORE_SIM_SCHEDULE_UNALIGNED;
        for (j = 0; j < i; ++j)
            if (p->participant_id == plan->participants[j].participant_id)
                return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
    }
    return CORE_SIM_SCHEDULE_OK;
}

CoreSimScheduleStatus core_sim_schedule_init(uint64_t plan_id,
    CoreSimTimeInterval horizon, CoreSimDuration exchange,
    const CoreSimParticipantSchedule *participants, size_t participant_count,
    CoreSimSchedule *out) {
    CoreSimSchedule candidate = {plan_id, horizon, exchange, participants, participant_count};
    CoreSimScheduleStatus status;
    if (!out) return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
    status = core_sim_schedule_validate(&candidate);
    if (status == CORE_SIM_SCHEDULE_OK) *out = candidate;
    return status;
}

/* Called only after complete plan validation. */
static const CoreSimParticipantSchedule *participant(const CoreSimSchedule *plan, uint64_t id) {
    size_t i;
    for (i = 0; i < plan->participant_count; ++i)
        if (plan->participants[i].participant_id == id) return &plan->participants[i];
    return NULL;
}

CoreSimScheduleStatus core_sim_schedule_window_count(const CoreSimSchedule *plan, uint64_t *out) {
    CoreSimScheduleStatus status;
    if (!out) return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
    status = core_sim_schedule_validate(plan);
    if (status != CORE_SIM_SCHEDULE_OK) return status;
    *out = (plan->horizon.end.ticks - plan->horizon.begin.ticks) / plan->exchange.ticks;
    return CORE_SIM_SCHEDULE_OK;
}

CoreSimScheduleStatus core_sim_schedule_step_count(const CoreSimSchedule *plan,
    uint64_t participant_id, uint64_t *out) {
    const CoreSimParticipantSchedule *p;
    CoreSimScheduleStatus status;
    if (!out) return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
    status = core_sim_schedule_validate(plan);
    if (status != CORE_SIM_SCHEDULE_OK) return status;
    p = participant(plan, participant_id);
    if (!p) return CORE_SIM_SCHEDULE_PARTICIPANT_NOT_FOUND;
    *out = (plan->horizon.end.ticks - plan->horizon.begin.ticks) / p->step.ticks;
    return CORE_SIM_SCHEDULE_OK;
}

/* Arithmetic is bounded by validated horizon.end: index < duration/stride. */
static CoreSimTimeInterval indexed_interval(const CoreSimSchedule *plan,
    uint64_t index, uint64_t stride) {
    CoreSimTimeInterval interval = plan->horizon;
    interval.begin.ticks += index * stride;
    interval.end.ticks = interval.begin.ticks + stride;
    return interval;
}

CoreSimScheduleStatus core_sim_schedule_window(const CoreSimSchedule *plan,
    uint64_t window_index, CoreSimTimeInterval *out) {
    uint64_t count;
    CoreSimScheduleStatus status;
    if (!out) return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
    status = core_sim_schedule_window_count(plan, &count);
    if (status != CORE_SIM_SCHEDULE_OK) return status;
    if (window_index >= count) return CORE_SIM_SCHEDULE_OUT_OF_RANGE;
    *out = indexed_interval(plan, window_index, plan->exchange.ticks);
    return CORE_SIM_SCHEDULE_OK;
}

CoreSimScheduleStatus core_sim_schedule_step(const CoreSimSchedule *plan,
    uint64_t participant_id, uint64_t step_index, CoreSimTimeInterval *out) {
    uint64_t count;
    const CoreSimParticipantSchedule *p;
    CoreSimScheduleStatus status;
    if (!out) return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
    status = core_sim_schedule_step_count(plan, participant_id, &count);
    if (status != CORE_SIM_SCHEDULE_OK) return status;
    if (step_index >= count) return CORE_SIM_SCHEDULE_OUT_OF_RANGE;
    p = participant(plan, participant_id);
    *out = indexed_interval(plan, step_index, p->step.ticks);
    return CORE_SIM_SCHEDULE_OK;
}

CoreSimScheduleStatus core_sim_schedule_window_steps(const CoreSimSchedule *plan,
    uint64_t participant_id, uint64_t window_index, CoreSimWindowSteps *out) {
    CoreSimWindowSteps result;
    const CoreSimParticipantSchedule *p;
    CoreSimScheduleStatus status;
    if (!out) return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
    status = core_sim_schedule_window(plan, window_index, &result.window);
    if (status != CORE_SIM_SCHEDULE_OK) return status;
    p = participant(plan, participant_id);
    if (!p) return CORE_SIM_SCHEDULE_PARTICIPANT_NOT_FOUND;
    result.step_count = plan->exchange.ticks / p->step.ticks;
    result.first_step_index = window_index * result.step_count;
    *out = result;
    return CORE_SIM_SCHEDULE_OK;
}

CoreSimScheduleStatus core_sim_schedule_observation_bracket(const CoreSimSchedule *plan,
    uint64_t participant_id, CoreSimTimePoint observation,
    CoreSimObservationBracket *out) {
    CoreSimObservationBracket result;
    CoreSimScheduleStatus status;
    const CoreSimParticipantSchedule *p;
    uint64_t offset, remainder;
    if (!out) return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
    status = core_sim_schedule_validate(plan);
    if (status != CORE_SIM_SCHEDULE_OK) return status;
    if (!observation.timebase.domain || !observation.timebase.ticks_per_second)
        return CORE_SIM_SCHEDULE_INVALID_ARGUMENT;
    if (!same_base(observation.timebase, plan->horizon.begin.timebase))
        return CORE_SIM_SCHEDULE_DOMAIN_MISMATCH;
    if (observation.ticks < plan->horizon.begin.ticks || observation.ticks > plan->horizon.end.ticks)
        return CORE_SIM_SCHEDULE_OUT_OF_RANGE;
    p = participant(plan, participant_id);
    if (!p) return CORE_SIM_SCHEDULE_PARTICIPANT_NOT_FOUND;
    offset = observation.ticks - plan->horizon.begin.ticks;
    remainder = offset % p->step.ticks;
    result.lower_step_index = offset / p->step.ticks;
    result.upper_step_index = result.lower_step_index + (remainder != 0);
    result.lower = plan->horizon.begin;
    result.lower.ticks += result.lower_step_index * p->step.ticks;
    result.upper = result.lower;
    if (remainder) result.upper.ticks += p->step.ticks;
    (void)core_sim_time_ratio_make(remainder, p->step.ticks, &result.weight);
    *out = result;
    return CORE_SIM_SCHEDULE_OK;
}
