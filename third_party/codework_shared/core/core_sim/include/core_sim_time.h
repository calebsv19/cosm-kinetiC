#ifndef CORE_SIM_TIME_H
#define CORE_SIM_TIME_H

#include <stddef.h>
#include <stdint.h>

/* Model time only. No wall clock, global state, allocation, or solver policy.
 * All outputs are unchanged on error. Value arguments permit output aliasing.
 * A domain is a caller-assigned nonzero run/branch identity; callers must not
 * reuse it for an unrelated origin. The complete (domain, rate) pair must match.
 * Structs are values, not a binary persistence format. */
typedef enum CoreSimTimeStatus {
    CORE_SIM_TIME_OK = 0,
    CORE_SIM_TIME_INVALID_ARGUMENT,
    CORE_SIM_TIME_OVERFLOW,
    CORE_SIM_TIME_RESOLUTION_LIMIT,
    CORE_SIM_TIME_UNREPRESENTABLE,
    CORE_SIM_TIME_DOMAIN_MISMATCH,
    CORE_SIM_TIME_REVERSED
} CoreSimTimeStatus;

typedef struct CoreSimTimeRatio {
    uint64_t numerator;
    uint64_t denominator;
} CoreSimTimeRatio;

typedef struct CoreSimTimebase {
    uint64_t domain;
    uint64_t ticks_per_second;
} CoreSimTimebase;

typedef struct CoreSimTimePoint {
    CoreSimTimebase timebase;
    uint64_t ticks;
} CoreSimTimePoint;

typedef struct CoreSimDuration {
    CoreSimTimebase timebase;
    uint64_t ticks;
} CoreSimDuration;

/* Nonempty half-open model-time interval [begin, end). */
typedef struct CoreSimTimeInterval {
    CoreSimTimePoint begin;
    CoreSimTimePoint end;
} CoreSimTimeInterval;

/* Ratios are nonnegative seconds. Accept unreduced input; normalize output.
 * Zero seconds normalizes to 0/1. Denominator zero always rejects. */
CoreSimTimeStatus core_sim_time_ratio_make(uint64_t numerator,
    uint64_t denominator, CoreSimTimeRatio *out);
CoreSimTimeStatus core_sim_timebase_make(uint64_t domain,
    uint64_t ticks_per_second, CoreSimTimebase *out);
/* Derive the least integer rate representing every positive rational period.
 * max_ticks_per_second is a required positive bound. No float rationalization. */
CoreSimTimeStatus core_sim_timebase_from_periods(uint64_t domain,
    const CoreSimTimeRatio *periods, size_t count,
    uint64_t max_ticks_per_second, CoreSimTimebase *out);
CoreSimTimeStatus core_sim_time_point_make(CoreSimTimebase base,
    uint64_t ticks, CoreSimTimePoint *out);
/* Zero duration is valid arithmetic (e.g. difference of equal points).
 * A positive solver cadence is a separate schedule admission rule. */
CoreSimTimeStatus core_sim_duration_make(CoreSimTimebase base,
    uint64_t ticks, CoreSimDuration *out);
CoreSimTimeStatus core_sim_time_point_from_seconds(CoreSimTimebase base,
    CoreSimTimeRatio seconds, CoreSimTimePoint *out);
CoreSimTimeStatus core_sim_duration_from_seconds(CoreSimTimebase base,
    CoreSimTimeRatio seconds, CoreSimDuration *out);
CoreSimTimeStatus core_sim_time_point_to_seconds(CoreSimTimePoint point,
    CoreSimTimeRatio *out);
CoreSimTimeStatus core_sim_duration_to_seconds(CoreSimDuration duration,
    CoreSimTimeRatio *out);
CoreSimTimeStatus core_sim_time_point_add(CoreSimTimePoint point,
    CoreSimDuration duration, CoreSimTimePoint *out);
CoreSimTimeStatus core_sim_time_point_difference(CoreSimTimePoint later,
    CoreSimTimePoint earlier, CoreSimDuration *out);
CoreSimTimeStatus core_sim_time_point_compare(CoreSimTimePoint a,
    CoreSimTimePoint b, int *out_order);
CoreSimTimeStatus core_sim_duration_add(CoreSimDuration a,
    CoreSimDuration b, CoreSimDuration *out);
/* Exact dimensionless scaling. Reduces before multiplication; rejects a
 * fractional tick or overflow rather than rounding or saturating. */
CoreSimTimeStatus core_sim_duration_scale(CoreSimDuration duration,
    CoreSimTimeRatio factor, CoreSimDuration *out);
CoreSimTimeStatus core_sim_time_interval_make(CoreSimTimePoint begin,
    CoreSimTimePoint end, CoreSimTimeInterval *out);

#endif
