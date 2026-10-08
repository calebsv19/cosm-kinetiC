#ifndef REFERENCE_HOST_H
#define REFERENCE_HOST_H
#include "core_sim_progress.h"

/* Private POSIX host example; none of this header is a library API/wire ABI. */
typedef struct RefNative {
    uint64_t emitted, received, source_steps, receiver_steps, signal;
} RefNative;
typedef struct RefState {
    uint64_t mode, windows, trajectory;
    RefNative native, previous;
    CoreSimJointProgress progress;
} RefState;
typedef struct RefHost {
    CoreSimParticipantSchedule participants[2];
    CoreSimSchedule schedule;
    CoreSimChannelDesc channels[2];
    uint64_t phases[2];
    CoreSimJointPlan plan;
} RefHost;
typedef struct RefObservations {
    uint64_t count, hash, fractional_brackets;
} RefObservations;

#define REF_HASH_INIT UINT64_C(14695981039346656037)
uint64_t ref_mix(uint64_t hash, uint64_t value);
int ref_host_init(RefHost *host, uint64_t mode);
int ref_restore_metadata(const RefHost *host, RefState *state);
/* fault: 0=none, 1=after source, 2=after receiver. A fault preserves outputs. */
int ref_window(const RefHost *host, const RefState *current, unsigned fault,
    uint64_t fps, RefState *out, RefObservations *observations);
int ref_native_valid(const RefState *state);
int ref_save(const char *path, const RefState *state, int fresh);
int ref_load(const char *path, const RefHost *host, uint64_t mode, RefState *out);
#endif
