#include "core_sim_time.h"

static uint64_t gcd(uint64_t a, uint64_t b) {
    while (b) {
        uint64_t r = a % b;
        a = b;
        b = r;
    }
    return a;
}

static int base_valid(CoreSimTimebase base) {
    return base.domain != 0 && base.ticks_per_second != 0;
}

static CoreSimTimeStatus bases_match(CoreSimTimebase a, CoreSimTimebase b) {
    if (!base_valid(a) || !base_valid(b)) return CORE_SIM_TIME_INVALID_ARGUMENT;
    if (a.domain != b.domain || a.ticks_per_second != b.ticks_per_second)
        return CORE_SIM_TIME_DOMAIN_MISMATCH;
    return CORE_SIM_TIME_OK;
}

CoreSimTimeStatus core_sim_time_ratio_make(uint64_t numerator,
    uint64_t denominator, CoreSimTimeRatio *out) {
    uint64_t divisor;
    CoreSimTimeRatio result;
    if (!out || !denominator) return CORE_SIM_TIME_INVALID_ARGUMENT;
    divisor = gcd(numerator, denominator);
    result.numerator = numerator / divisor;
    result.denominator = denominator / divisor;
    *out = result;
    return CORE_SIM_TIME_OK;
}

CoreSimTimeStatus core_sim_timebase_make(uint64_t domain,
    uint64_t ticks_per_second, CoreSimTimebase *out) {
    CoreSimTimebase result = {domain, ticks_per_second};
    if (!out || !base_valid(result)) return CORE_SIM_TIME_INVALID_ARGUMENT;
    *out = result;
    return CORE_SIM_TIME_OK;
}

CoreSimTimeStatus core_sim_timebase_from_periods(uint64_t domain,
    const CoreSimTimeRatio *periods, size_t count,
    uint64_t max_ticks_per_second, CoreSimTimebase *out) {
    uint64_t rate = 1;
    size_t i;
    if (!domain || !periods || !count || !max_ticks_per_second || !out)
        return CORE_SIM_TIME_INVALID_ARGUMENT;
    for (i = 0; i < count; ++i) {
        CoreSimTimeRatio period;
        uint64_t factor;
        if (!periods[i].numerator || core_sim_time_ratio_make(
            periods[i].numerator, periods[i].denominator, &period) != CORE_SIM_TIME_OK)
            return CORE_SIM_TIME_INVALID_ARGUMENT;
        factor = period.denominator / gcd(rate, period.denominator);
        if (rate > UINT64_MAX / factor) return CORE_SIM_TIME_OVERFLOW;
        rate *= factor;
        if (rate > max_ticks_per_second) return CORE_SIM_TIME_RESOLUTION_LIMIT;
    }
    for (i = 0; i < count; ++i) {
        CoreSimTimeRatio period;
        (void)core_sim_time_ratio_make(periods[i].numerator,
            periods[i].denominator, &period);
        if (period.numerator > UINT64_MAX / (rate / period.denominator))
            return CORE_SIM_TIME_OVERFLOW;
    }
    return core_sim_timebase_make(domain, rate, out);
}

CoreSimTimeStatus core_sim_time_point_make(CoreSimTimebase base,
    uint64_t ticks, CoreSimTimePoint *out) {
    CoreSimTimePoint result = {base, ticks};
    if (!out || !base_valid(base)) return CORE_SIM_TIME_INVALID_ARGUMENT;
    *out = result;
    return CORE_SIM_TIME_OK;
}

CoreSimTimeStatus core_sim_duration_make(CoreSimTimebase base,
    uint64_t ticks, CoreSimDuration *out) {
    CoreSimDuration result = {base, ticks};
    if (!out || !base_valid(base)) return CORE_SIM_TIME_INVALID_ARGUMENT;
    *out = result;
    return CORE_SIM_TIME_OK;
}

static CoreSimTimeStatus seconds_ticks(CoreSimTimebase base,
    CoreSimTimeRatio seconds, uint64_t *out) {
    CoreSimTimeRatio ratio;
    uint64_t scale;
    if (!base_valid(base) || core_sim_time_ratio_make(seconds.numerator,
        seconds.denominator, &ratio) != CORE_SIM_TIME_OK)
        return CORE_SIM_TIME_INVALID_ARGUMENT;
    if (base.ticks_per_second % ratio.denominator)
        return CORE_SIM_TIME_UNREPRESENTABLE;
    scale = base.ticks_per_second / ratio.denominator;
    if (ratio.numerator > UINT64_MAX / scale) return CORE_SIM_TIME_OVERFLOW;
    *out = ratio.numerator * scale;
    return CORE_SIM_TIME_OK;
}

CoreSimTimeStatus core_sim_time_point_from_seconds(CoreSimTimebase base,
    CoreSimTimeRatio seconds, CoreSimTimePoint *out) {
    uint64_t ticks;
    CoreSimTimeStatus status;
    if (!out) return CORE_SIM_TIME_INVALID_ARGUMENT;
    status = seconds_ticks(base, seconds, &ticks);
    if (status != CORE_SIM_TIME_OK) return status;
    return core_sim_time_point_make(base, ticks, out);
}

CoreSimTimeStatus core_sim_duration_from_seconds(CoreSimTimebase base,
    CoreSimTimeRatio seconds, CoreSimDuration *out) {
    uint64_t ticks;
    CoreSimTimeStatus status;
    if (!out) return CORE_SIM_TIME_INVALID_ARGUMENT;
    status = seconds_ticks(base, seconds, &ticks);
    if (status != CORE_SIM_TIME_OK) return status;
    return core_sim_duration_make(base, ticks, out);
}

CoreSimTimeStatus core_sim_time_point_to_seconds(CoreSimTimePoint point,
    CoreSimTimeRatio *out) {
    if (!base_valid(point.timebase)) return CORE_SIM_TIME_INVALID_ARGUMENT;
    return core_sim_time_ratio_make(point.ticks, point.timebase.ticks_per_second, out);
}

CoreSimTimeStatus core_sim_duration_to_seconds(CoreSimDuration duration,
    CoreSimTimeRatio *out) {
    if (!base_valid(duration.timebase)) return CORE_SIM_TIME_INVALID_ARGUMENT;
    return core_sim_time_ratio_make(duration.ticks, duration.timebase.ticks_per_second, out);
}

CoreSimTimeStatus core_sim_time_point_add(CoreSimTimePoint point,
    CoreSimDuration duration, CoreSimTimePoint *out) {
    CoreSimTimeStatus status = bases_match(point.timebase, duration.timebase);
    if (!out) return CORE_SIM_TIME_INVALID_ARGUMENT;
    if (status != CORE_SIM_TIME_OK) return status;
    if (point.ticks > UINT64_MAX - duration.ticks) return CORE_SIM_TIME_OVERFLOW;
    return core_sim_time_point_make(point.timebase, point.ticks + duration.ticks, out);
}

CoreSimTimeStatus core_sim_time_point_difference(CoreSimTimePoint later,
    CoreSimTimePoint earlier, CoreSimDuration *out) {
    CoreSimTimeStatus status = bases_match(later.timebase, earlier.timebase);
    if (!out) return CORE_SIM_TIME_INVALID_ARGUMENT;
    if (status != CORE_SIM_TIME_OK) return status;
    if (later.ticks < earlier.ticks) return CORE_SIM_TIME_REVERSED;
    return core_sim_duration_make(later.timebase, later.ticks - earlier.ticks, out);
}

CoreSimTimeStatus core_sim_time_point_compare(CoreSimTimePoint a,
    CoreSimTimePoint b, int *out_order) {
    CoreSimTimeStatus status = bases_match(a.timebase, b.timebase);
    if (!out_order) return CORE_SIM_TIME_INVALID_ARGUMENT;
    if (status != CORE_SIM_TIME_OK) return status;
    *out_order = (a.ticks > b.ticks) - (a.ticks < b.ticks);
    return CORE_SIM_TIME_OK;
}

CoreSimTimeStatus core_sim_duration_add(CoreSimDuration a,
    CoreSimDuration b, CoreSimDuration *out) {
    CoreSimTimeStatus status = bases_match(a.timebase, b.timebase);
    if (!out) return CORE_SIM_TIME_INVALID_ARGUMENT;
    if (status != CORE_SIM_TIME_OK) return status;
    if (a.ticks > UINT64_MAX - b.ticks) return CORE_SIM_TIME_OVERFLOW;
    return core_sim_duration_make(a.timebase, a.ticks + b.ticks, out);
}

CoreSimTimeStatus core_sim_duration_scale(CoreSimDuration duration,
    CoreSimTimeRatio factor, CoreSimDuration *out) {
    CoreSimTimeRatio reduced;
    uint64_t divisor, ticks, denominator;
    if (!out || !base_valid(duration.timebase) || core_sim_time_ratio_make(
        factor.numerator, factor.denominator, &reduced) != CORE_SIM_TIME_OK)
        return CORE_SIM_TIME_INVALID_ARGUMENT;
    divisor = gcd(duration.ticks, reduced.denominator);
    ticks = duration.ticks / divisor;
    denominator = reduced.denominator / divisor;
    if (denominator != 1) return CORE_SIM_TIME_UNREPRESENTABLE;
    if (reduced.numerator && ticks > UINT64_MAX / reduced.numerator)
        return CORE_SIM_TIME_OVERFLOW;
    return core_sim_duration_make(duration.timebase, ticks * reduced.numerator, out);
}

CoreSimTimeStatus core_sim_time_interval_make(CoreSimTimePoint begin,
    CoreSimTimePoint end, CoreSimTimeInterval *out) {
    CoreSimTimeStatus status = bases_match(begin.timebase, end.timebase);
    CoreSimTimeInterval result = {begin, end};
    if (!out) return CORE_SIM_TIME_INVALID_ARGUMENT;
    if (status != CORE_SIM_TIME_OK) return status;
    if (end.ticks < begin.ticks) return CORE_SIM_TIME_REVERSED;
    if (end.ticks == begin.ticks) return CORE_SIM_TIME_INVALID_ARGUMENT;
    *out = result;
    return CORE_SIM_TIME_OK;
}
