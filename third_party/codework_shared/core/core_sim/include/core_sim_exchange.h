#ifndef CORE_SIM_EXCHANGE_H
#define CORE_SIM_EXCHANGE_H

#include "core_sim_schedule.h"

#define CORE_SIM_EXCHANGE_SEMANTIC_VERSION 1u

typedef enum CoreSimExchangeStatus {
    CORE_SIM_EXCHANGE_OK = 0,
    CORE_SIM_EXCHANGE_INVALID_ARGUMENT,
    CORE_SIM_EXCHANGE_UNSUPPORTED,
    CORE_SIM_EXCHANGE_DOMAIN_MISMATCH,
    CORE_SIM_EXCHANGE_IDENTITY_MISMATCH,
    CORE_SIM_EXCHANGE_OUT_OF_RANGE,
    CORE_SIM_EXCHANGE_POLICY_UNAVAILABLE,
    CORE_SIM_EXCHANGE_STALE,
    CORE_SIM_EXCHANGE_FUTURE
} CoreSimExchangeStatus;

typedef enum CoreSimTemporalKind {
    CORE_SIM_TEMPORAL_INTEGRATED = 1,
    CORE_SIM_TEMPORAL_POINT,
    CORE_SIM_TEMPORAL_DISCRETE
} CoreSimTemporalKind;

typedef enum CoreSimTemporalPolicy {
    CORE_SIM_POLICY_UNIFORM_RATE = 1, /* integrated amounts only */
    CORE_SIM_POLICY_EXACT,
    CORE_SIM_POLICY_HOLD,
    CORE_SIM_POLICY_LINEAR /* point observations only */
} CoreSimTemporalPolicy;

/* Value metadata, not a serialization format. IDs are nonzero opaque host
 * identities, not units, coordinates, content hashes or ownership proofs.
 * The host freezes/verifies plan/channel identity, adapter/profile meaning and
 * referenced checkpoints/payloads before calling these structural helpers.
 * required_features must be zero in v1. Unknown versions/features reject.
 * max_age uses the plan timebase; positive only for HOLD, zero otherwise.
 * No core dependencies on field storage, unit conversion or transport. */
typedef struct CoreSimChannelDesc {
    uint32_t semantic_version;
    uint64_t required_features;
    uint64_t plan_id, channel_id;
    uint64_t producer_id, consumer_id;
    uint64_t quantity_id, unit_id, spatial_support_id;
    uint64_t adapter_id, profile_id;
    CoreSimTemporalKind kind;
    CoreSimTemporalPolicy policy;
    CoreSimDuration max_age;
} CoreSimChannelDesc;

/* Sequence is a nonzero channel-local order within a window, not a timestamp.
 * Stable key: (plan/domain/rate, channel, window_index, sequence).
 * Same key with changed checkpoint, support or payload is a conflict.
 * Host must use new identities when referenced content changes.
 * Point/discrete samples are at checkpoint_time. Integrated support is inside
 * its source window and ends at checkpoint_time. Native sample/checkpoint
 * cadence and physical validity remain adapter-owned (dense output is allowed).
 * Prior-window point samples may be explicitly held in a later window.
 * Only the union member selected by kind is read. */
typedef struct CoreSimExchangeRecord {
    uint32_t semantic_version;
    uint64_t required_features;
    uint64_t plan_id, channel_id, window_index, sequence;
    uint64_t checkpoint_ref, payload_ref;
    CoreSimTemporalKind kind;
    CoreSimTimePoint checkpoint_time;
    union {
        CoreSimTimePoint point;
        CoreSimTimeInterval interval;
    } support;
} CoreSimExchangeRecord;

typedef enum CoreSimExchangeRelation {
    CORE_SIM_EXCHANGE_DISTINCT = 0,
    CORE_SIM_EXCHANGE_REPLAY,
    CORE_SIM_EXCHANGE_CONFLICT
} CoreSimExchangeRelation;

/* Metadata weights only: evaluate (1-weight)*lower + weight*upper in a
 * meaningful channel adapter. HOLD/EXACT return equal sequences, weight 0/1.
 * age is request minus held/exact sample, or zero for a linear bracket. */
typedef struct CoreSimExchangeSample {
    uint64_t lower_sequence, upper_sequence;
    CoreSimTimeRatio weight;
    CoreSimDuration age;
} CoreSimExchangeSample;

typedef struct CoreSimExchangeOverlap {
    CoreSimDuration duration;
    CoreSimTimeRatio source_fraction; /* overlap / source support duration */
    CoreSimTimeRatio target_fraction; /* overlap / target duration */
} CoreSimExchangeOverlap;

/* All outputs unchanged on failure; outputs must not overlap input storage.
 * Borrowed inputs must remain immutable for the call. No pointers are retained,
 * allocation, callbacks, interpolation, ledger mutation or advancement. */
CoreSimExchangeStatus core_sim_channel_validate(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel);
/* Structural metadata validation only. admit additionally checks a host-supplied
 * producer availability frontier. A frontier is not proof of payload contents. */
CoreSimExchangeStatus core_sim_exchange_record_validate(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel, const CoreSimExchangeRecord *record);
CoreSimExchangeStatus core_sim_exchange_record_admit(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel, const CoreSimExchangeRecord *record,
    CoreSimTimePoint available_through);
/* Pairwise identity classification only; T4/host owns the accepted ledger. */
CoreSimExchangeStatus core_sim_exchange_record_compare(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel, const CoreSimExchangeRecord *a,
    const CoreSimExchangeRecord *b, CoreSimExchangeRelation *out);
/* Request is within the selected record window (end included for observations).
 * EXACT/HOLD take lower only (upper must be NULL). LINEAR takes either an exact
 * lower only or two strictly ordered, available records whose sequences increase.
 * Upper can be later than request only when already available through frontier.
 * Future-only/absent brackets reject; no extrapolation or implicit fallback. */
CoreSimExchangeStatus core_sim_exchange_sample(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel, CoreSimTimePoint request,
    CoreSimTimePoint available_through, const CoreSimExchangeRecord *lower,
    const CoreSimExchangeRecord *upper, CoreSimExchangeSample *out);
/* Explicit uniform-rate reconstruction of an integrated amount only. Target
 * is a nonempty interval inside the plan horizon; disjoint/touching gives 0/1.
 * To conserve an amount, adapters must tile source support exactly once using
 * disjoint targets; this stateless query does not track allocations or budgets. */
CoreSimExchangeStatus core_sim_exchange_overlap(const CoreSimSchedule *plan,
    const CoreSimChannelDesc *channel, const CoreSimExchangeRecord *source,
    CoreSimTimePoint available_through, CoreSimTimeInterval target,
    CoreSimExchangeOverlap *out);

#endif
