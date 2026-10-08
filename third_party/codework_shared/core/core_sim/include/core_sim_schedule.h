#ifndef CORE_SIM_SCHEDULE_H
#define CORE_SIM_SCHEDULE_H

#include "core_sim_time.h"

#define CORE_SIM_SCHEDULE_MAX_PARTICIPANTS 256u

typedef enum CoreSimScheduleStatus {
    CORE_SIM_SCHEDULE_OK = 0,
    CORE_SIM_SCHEDULE_INVALID_ARGUMENT,
    CORE_SIM_SCHEDULE_DOMAIN_MISMATCH,
    CORE_SIM_SCHEDULE_UNALIGNED,
    CORE_SIM_SCHEDULE_CAPACITY,
    CORE_SIM_SCHEDULE_OUT_OF_RANGE,
    CORE_SIM_SCHEDULE_PARTICIPANT_NOT_FOUND
} CoreSimScheduleStatus;

typedef struct CoreSimParticipantSchedule {
    uint64_t participant_id;
    CoreSimDuration step;
} CoreSimParticipantSchedule;

/* Fixed-step v1: all participants start at horizon.begin; exchange windows
 * and the terminal horizon must align with every participant's step.
 * plan_id is a nonzero caller-assigned identity, not a content hash.
 * Participant storage is borrowed and must remain alive and immutable during
 * use. Every query revalidates it; there is no state/history to detect a change
 * between calls. Use a new identity for a changed plan.
 * No allocation, callbacks, clock reads, field interpolation or advancement.
 * Queries are index-based, so huge runs do not allocate huge event arrays.
 * All errors preserve outputs. Outputs must not overlap borrowed plan storage. */
typedef struct CoreSimSchedule {
    uint64_t plan_id;
    CoreSimTimeInterval horizon;
    CoreSimDuration exchange;
    const CoreSimParticipantSchedule *participants;
    size_t participant_count;
} CoreSimSchedule;

typedef struct CoreSimWindowSteps {
    CoreSimTimeInterval window;
    uint64_t first_step_index;
    uint64_t step_count;
} CoreSimWindowSteps;

/* Planning brackets only: does not assert that snapshots exist or permit
 * consuming a future state. T3 channel admission owns those decisions.
 * Indices count completed steps from horizon.begin (initial state index 0).
 * weight is the reduced dimensionless fraction from lower to upper.
 * At an exact state, both points/indices agree and weight is 0/1. */
typedef struct CoreSimObservationBracket {
    CoreSimTimePoint lower;
    CoreSimTimePoint upper;
    uint64_t lower_step_index;
    uint64_t upper_step_index;
    CoreSimTimeRatio weight;
} CoreSimObservationBracket;

CoreSimScheduleStatus core_sim_schedule_validate(const CoreSimSchedule *plan);
CoreSimScheduleStatus core_sim_schedule_init(uint64_t plan_id,
    CoreSimTimeInterval horizon, CoreSimDuration exchange,
    const CoreSimParticipantSchedule *participants, size_t participant_count,
    CoreSimSchedule *out);
CoreSimScheduleStatus core_sim_schedule_window_count(const CoreSimSchedule *plan,
    uint64_t *out);
CoreSimScheduleStatus core_sim_schedule_step_count(const CoreSimSchedule *plan,
    uint64_t participant_id, uint64_t *out);
/* Windows and step intervals use zero-based indices and are half-open. */
CoreSimScheduleStatus core_sim_schedule_window(const CoreSimSchedule *plan,
    uint64_t window_index, CoreSimTimeInterval *out);
CoreSimScheduleStatus core_sim_schedule_step(const CoreSimSchedule *plan,
    uint64_t participant_id, uint64_t step_index, CoreSimTimeInterval *out);
CoreSimScheduleStatus core_sim_schedule_window_steps(const CoreSimSchedule *plan,
    uint64_t participant_id, uint64_t window_index, CoreSimWindowSteps *out);
/* Observation timestamps include both horizon endpoints. */
CoreSimScheduleStatus core_sim_schedule_observation_bracket(const CoreSimSchedule *plan,
    uint64_t participant_id, CoreSimTimePoint observation,
    CoreSimObservationBracket *out);

#endif
