#ifndef CORE_SIM_PROGRESS_H
#define CORE_SIM_PROGRESS_H

#include "core_sim_exchange.h"

#define CORE_SIM_JOINT_SEMANTIC_VERSION 1u
#define CORE_SIM_JOINT_MAX_CHANNELS 64u

typedef enum CoreSimJointStatus {
    CORE_SIM_JOINT_OK = 0,
    CORE_SIM_JOINT_INVALID_ARGUMENT,
    CORE_SIM_JOINT_UNSUPPORTED,
    CORE_SIM_JOINT_CAPACITY,
    CORE_SIM_JOINT_IDENTITY_MISMATCH,
    CORE_SIM_JOINT_WRONG_TIME,
    CORE_SIM_JOINT_INCOMPLETE,
    CORE_SIM_JOINT_CAUSALITY,
    CORE_SIM_JOINT_STALE_PREDECESSOR,
    CORE_SIM_JOINT_REPLAY,
    CORE_SIM_JOINT_CONFLICT,
    CORE_SIM_JOINT_COMPLETE
} CoreSimJointStatus;

/* Frozen borrowed plan. plan_ref identifies host-verified immutable contents,
 * including schedule, channels and phase order; no hashing is performed here.
 * All participants occur once in phase_order, each advancing one whole window.
 * 0..64 required channels; one transfer per channel per window in channel order.
 * Physical adapters aggregate fine-grained records before this v1 boundary. */
typedef struct CoreSimJointPlan {
    uint32_t semantic_version;
    uint64_t required_features;
    uint64_t plan_ref;
    const CoreSimSchedule *schedule;
    const CoreSimChannelDesc *channels;
    size_t channel_count;
    const uint64_t *phase_order;
    size_t phase_count;
} CoreSimJointPlan;

typedef struct CoreSimCheckpoint {
    uint64_t participant_id, checkpoint_ref;
    CoreSimTimePoint time;
    uint64_t validation_ref; /* host-verified native checkpoint evidence */
} CoreSimCheckpoint;

/* Declares consumption by the staged consumer checkpoint, not just production.
 * validation_ref binds host adapter/content/budget validation evidence.
 * T4 only checks nonzero identity; it cannot prove callbacks consumed anything.
 * Integrated record covers the entire window; request is its begin, no upper.
 * Point/discrete request is inside the window, endpoints included.
 * Each record must refer to this producer's prior or staged checkpoint.
 * Staged producer records are available only if its phase precedes consumer.
 * LINEAR may use both checkpoints only under that explicit ordered strategy.
 * has_upper is exactly 0 or 1; inactive upper bytes are ignored. */
typedef struct CoreSimJointTransfer {
    CoreSimExchangeRecord lower, upper;
    int has_upper;
    CoreSimTimePoint request;
    uint64_t consumer_checkpoint_ref;
    uint64_t validation_ref;
} CoreSimJointTransfer;

/* Bounded value records, not wire formats. Checkpoint order equals schedule
 * participant order. Transfer order equals joint-plan channel order.
 * candidate_id is immutable host identity; expected_predecessor_id binds the
 * prior accepted set. validation_ref binds whole-window physical/budget checks.
 * Native outputs are staged and not publicly accepted merely by validation. */
typedef struct CoreSimJointCandidate {
    uint32_t semantic_version;
    uint64_t required_features;
    uint64_t plan_id, plan_ref;
    uint64_t candidate_id, expected_predecessor_id, window_index;
    uint64_t validation_ref;
    size_t checkpoint_count, transfer_count;
    CoreSimCheckpoint checkpoints[CORE_SIM_SCHEDULE_MAX_PARTICIPANTS];
    CoreSimJointTransfer transfers[CORE_SIM_JOINT_MAX_CHANNELS];
} CoreSimJointCandidate;

/* Compact last-window ledger. Initial accepted has no predecessor/transfers;
 * otherwise previous_checkpoints plus accepted retain the exact last transition
 * for restart validation and idempotent last-candidate replay classification.
 * Older requests reject as stale/wrong-window, not as historical replay.
 * Full history, durable identity/content verification and persistence are host
 * owned. Strict consecutive windows prevent accepting an earlier window again. */
typedef struct CoreSimJointProgress {
    uint64_t completed_windows;
    CoreSimJointCandidate accepted;
    CoreSimCheckpoint previous_checkpoints[CORE_SIM_SCHEDULE_MAX_PARTICIPANTS];
} CoreSimJointProgress;

CoreSimJointStatus core_sim_joint_plan_validate(const CoreSimJointPlan *plan);
/* Init copies initial checkpoints at horizon.begin; validation_ref and initial
 * ID bind the host's initial accepted manifest. Output unchanged on error. */
CoreSimJointStatus core_sim_joint_init(const CoreSimJointPlan *plan,
    uint64_t initial_id, uint64_t validation_ref,
    const CoreSimCheckpoint *checkpoints, size_t count, CoreSimJointProgress *out);
CoreSimJointStatus core_sim_joint_progress_validate(const CoreSimJointPlan *plan,
    const CoreSimJointProgress *progress);
CoreSimJointStatus core_sim_joint_candidate_validate(const CoreSimJointPlan *plan,
    const CoreSimJointProgress *current, const CoreSimJointCandidate *candidate);
/* Pure proposed accepted-state transition. All non-OK statuses (including
 * REPLAY) preserve output. out==current is allowed; no other input/output
 * overlap is supported. Inputs stay immutable during each call.
 * For durable use, prepare into a separate object, atomically publish through
 * host persistence, then expose/copy that state. On uncertain publication reread
 * the same identity; do not blindly retry. No IO, locking, solver rollback,
 * allocation, clock reads or automatic callbacks occur in this API. */
CoreSimJointStatus core_sim_joint_accept(const CoreSimJointPlan *plan,
    const CoreSimJointProgress *current, const CoreSimJointCandidate *candidate,
    CoreSimJointProgress *out);

#endif
