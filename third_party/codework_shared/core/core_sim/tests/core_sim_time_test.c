#include "core_sim_time.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(c) do { ++checks; if (!(c)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); return 1; \
} } while (0)
#define ERROR_UNCHANGED(call, output, expected) do { \
    unsigned char before[sizeof(output)]; \
    memcpy(before, &(output), sizeof(output)); \
    CHECK((call) == (expected)); \
    CHECK(memcmp(before, &(output), sizeof(output)) == 0); \
} while (0)
#define OK(call) CHECK((call) == CORE_SIM_TIME_OK)

static int test_known_rates(void) {
    CoreSimTimeRatio periods[] = {{1, 50}, {1, 200}, {1, 5}};
    CoreSimTimebase base;
    CoreSimDuration fire, fluid, exchange, total;
    CoreSimTimePoint origin, end;
    CoreSimTimeRatio seconds;
    CoreSimTimeInterval interval;
    int order;
    OK(core_sim_timebase_from_periods(7, periods, 3, 10000, &base));
    CHECK(base.domain == 7 && base.ticks_per_second == 200);
    OK(core_sim_duration_from_seconds(base, periods[0], &fire));
    OK(core_sim_duration_from_seconds(base, periods[1], &fluid));
    OK(core_sim_duration_from_seconds(base, periods[2], &exchange));
    CHECK(fire.ticks == 4 && fluid.ticks == 1 && exchange.ticks == 40);
    OK(core_sim_duration_from_seconds(base, (CoreSimTimeRatio){40, 1}, &total));
    CHECK(total.ticks == 8000);
    CHECK(total.ticks / fire.ticks == 2000);
    CHECK(total.ticks / fluid.ticks == 8000);
    CHECK(total.ticks / exchange.ticks == 200);
    OK(core_sim_time_point_make(base, 0, &origin));
    OK(core_sim_time_point_add(origin, total, &end));
    OK(core_sim_time_point_compare(origin, end, &order)); CHECK(order == -1);
    OK(core_sim_time_point_compare(end, origin, &order)); CHECK(order == 1);
    OK(core_sim_time_point_compare(end, end, &order)); CHECK(order == 0);
    OK(core_sim_time_interval_make(origin, end, &interval));
    CHECK(interval.begin.ticks == 0 && interval.end.ticks == 8000);
    OK(core_sim_time_point_difference(end, origin, &total)); CHECK(total.ticks == 8000);
    OK(core_sim_time_point_to_seconds(end, &seconds));
    CHECK(seconds.numerator == 40 && seconds.denominator == 1);
    OK(core_sim_duration_to_seconds(fire, &seconds));
    CHECK(seconds.numerator == 1 && seconds.denominator == 50);
    OK(core_sim_time_point_from_seconds(base, seconds, &origin)); CHECK(origin.ticks == 4);
    /* Value inputs allow intentional aliasing; neither advances a global clock. */
    OK(core_sim_time_point_add(origin, fire, &origin)); CHECK(origin.ticks == 8);
    OK(core_sim_duration_add(fire, fire, &fire)); CHECK(fire.ticks == 8);
    OK(core_sim_duration_scale(fire, (CoreSimTimeRatio){1, 2}, &fire)); CHECK(fire.ticks == 4);
    OK(core_sim_time_point_difference(end, end, &total)); CHECK(total.ticks == 0);
    OK(core_sim_duration_scale(fire, (CoreSimTimeRatio){0, 19}, &total)); CHECK(total.ticks == 0);
    OK(core_sim_duration_to_seconds(total, &seconds));
    CHECK(seconds.numerator == 0 && seconds.denominator == 1);
    periods[0] = (CoreSimTimeRatio){1, 60};
    OK(core_sim_timebase_from_periods(7, periods, 3, 10000, &base));
    CHECK(base.ticks_per_second == 600);
    OK(core_sim_duration_from_seconds(base, periods[0], &fire)); CHECK(fire.ticks == 10);
    OK(core_sim_duration_from_seconds(base, periods[1], &fluid)); CHECK(fluid.ticks == 3);
    return 0;
}

static int test_rejections(void) {
    CoreSimTimebase base = {7, 200}, other = {8, 200}, rate = {7, 600}, invalid = {0, 200};
    CoreSimTimeRatio ratio = {99, 17}, periods[] = {{1, 3}, {1, 5}};
    CoreSimTimePoint point = {base, 11}, output = {other, 99};
    CoreSimDuration duration = {base, 3}, result = {other, 99};
    CoreSimTimeInterval interval = {{base, 1}, {base, 2}};
    CoreSimTimebase saved_base = base;
    int order = 9;
    ERROR_UNCHANGED(core_sim_time_ratio_make(1, 0, &ratio), ratio, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_timebase_make(0, 200, &base), base, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_timebase_make(7, 0, &base), base, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_timebase_from_periods(7, periods, 2, 14, &base), base, CORE_SIM_TIME_RESOLUTION_LIMIT);
    ERROR_UNCHANGED(core_sim_timebase_from_periods(0, periods, 2, 100, &base), base, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_timebase_from_periods(7, NULL, 2, 100, &base), base, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_timebase_from_periods(7, periods, 0, 100, &base), base, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_timebase_from_periods(7, periods, 2, 0, &base), base, CORE_SIM_TIME_INVALID_ARGUMENT);
    periods[0] = (CoreSimTimeRatio){0, 3};
    ERROR_UNCHANGED(core_sim_timebase_from_periods(7, periods, 2, 100, &base), base, CORE_SIM_TIME_INVALID_ARGUMENT);
    periods[0] = (CoreSimTimeRatio){1, 0};
    ERROR_UNCHANGED(core_sim_timebase_from_periods(7, periods, 2, 100, &base), base, CORE_SIM_TIME_INVALID_ARGUMENT);
    periods[0] = (CoreSimTimeRatio){1, UINT64_MAX}; periods[1] = (CoreSimTimeRatio){1, 2};
    ERROR_UNCHANGED(core_sim_timebase_from_periods(7, periods, 2, UINT64_MAX, &base), base, CORE_SIM_TIME_OVERFLOW);
    periods[0] = (CoreSimTimeRatio){UINT64_MAX, 2}; periods[1] = (CoreSimTimeRatio){1, 3};
    ERROR_UNCHANGED(core_sim_timebase_from_periods(7, periods, 2, UINT64_MAX, &base), base, CORE_SIM_TIME_OVERFLOW);
    ERROR_UNCHANGED(core_sim_time_point_make(invalid, 1, &output), output, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_duration_make(invalid, 1, &result), result, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_time_point_from_seconds(base, (CoreSimTimeRatio){1, 3}, &output), output, CORE_SIM_TIME_UNREPRESENTABLE);
    ERROR_UNCHANGED(core_sim_duration_from_seconds(base, (CoreSimTimeRatio){1, 3}, &result), result, CORE_SIM_TIME_UNREPRESENTABLE);
    ERROR_UNCHANGED(core_sim_duration_from_seconds(base, (CoreSimTimeRatio){1, 0}, &result), result, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_time_point_from_seconds(base, (CoreSimTimeRatio){UINT64_MAX, 1}, &output), output, CORE_SIM_TIME_OVERFLOW);
    ERROR_UNCHANGED(core_sim_duration_from_seconds(base, (CoreSimTimeRatio){UINT64_MAX, 1}, &result), result, CORE_SIM_TIME_OVERFLOW);
    ERROR_UNCHANGED(core_sim_time_point_add(point, (CoreSimDuration){other, 1}, &output), output, CORE_SIM_TIME_DOMAIN_MISMATCH);
    ERROR_UNCHANGED(core_sim_time_point_add(point, (CoreSimDuration){rate, 1}, &output), output, CORE_SIM_TIME_DOMAIN_MISMATCH);
    ERROR_UNCHANGED(core_sim_time_point_add(point, (CoreSimDuration){invalid, 1}, &output), output, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_time_point_add(point, (CoreSimDuration){base, UINT64_MAX}, &output), output, CORE_SIM_TIME_OVERFLOW);
    ERROR_UNCHANGED(core_sim_time_point_difference(point, (CoreSimTimePoint){other, 0}, &result), result, CORE_SIM_TIME_DOMAIN_MISMATCH);
    ERROR_UNCHANGED(core_sim_time_point_difference(point, (CoreSimTimePoint){base, 12}, &result), result, CORE_SIM_TIME_REVERSED);
    ERROR_UNCHANGED(core_sim_time_point_compare(point, (CoreSimTimePoint){rate, 11}, &order), order, CORE_SIM_TIME_DOMAIN_MISMATCH);
    ERROR_UNCHANGED(core_sim_duration_add(duration, (CoreSimDuration){other, 1}, &result), result, CORE_SIM_TIME_DOMAIN_MISMATCH);
    ERROR_UNCHANGED(core_sim_duration_add(duration, (CoreSimDuration){base, UINT64_MAX}, &result), result, CORE_SIM_TIME_OVERFLOW);
    ERROR_UNCHANGED(core_sim_duration_scale(duration, (CoreSimTimeRatio){1, 2}, &result), result, CORE_SIM_TIME_UNREPRESENTABLE);
    ERROR_UNCHANGED(core_sim_duration_scale(duration, (CoreSimTimeRatio){UINT64_MAX, 1}, &result), result, CORE_SIM_TIME_OVERFLOW);
    ERROR_UNCHANGED(core_sim_duration_scale(duration, (CoreSimTimeRatio){1, 0}, &result), result, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_time_point_to_seconds((CoreSimTimePoint){invalid, 1}, &ratio), ratio, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_duration_to_seconds((CoreSimDuration){invalid, 1}, &ratio), ratio, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_time_interval_make(point, point, &interval), interval, CORE_SIM_TIME_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_time_interval_make(point, (CoreSimTimePoint){base, 0}, &interval), interval, CORE_SIM_TIME_REVERSED);
    ERROR_UNCHANGED(core_sim_time_interval_make(point, (CoreSimTimePoint){other, 12}, &interval), interval, CORE_SIM_TIME_DOMAIN_MISMATCH);
    CHECK(base.domain == saved_base.domain && base.ticks_per_second == saved_base.ticks_per_second);
    CHECK(core_sim_time_ratio_make(1, 2, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_timebase_make(1, 2, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_timebase_from_periods(1, periods, 2, 100, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_time_point_make(base, 1, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_duration_make(base, 1, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_time_point_from_seconds(base, ratio, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_duration_from_seconds(base, ratio, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_time_point_to_seconds(point, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_duration_to_seconds(duration, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_time_point_add(point, duration, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_time_point_difference(point, point, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_time_point_compare(point, point, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_duration_add(duration, duration, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_duration_scale(duration, ratio, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    CHECK(core_sim_time_interval_make(point, point, NULL) == CORE_SIM_TIME_INVALID_ARGUMENT);
    return 0;
}

static int test_integer_limits(void) {
    CoreSimTimebase base = {1, UINT64_MAX};
    CoreSimTimeRatio ratio;
    CoreSimTimePoint point;
    CoreSimDuration duration, result;
    OK(core_sim_time_point_from_seconds(base, (CoreSimTimeRatio){1, 1}, &point));
    CHECK(point.ticks == UINT64_MAX);
    OK(core_sim_time_point_to_seconds(point, &ratio));
    CHECK(ratio.numerator == 1 && ratio.denominator == 1);
    OK(core_sim_duration_make(base, UINT64_MAX, &duration));
    OK(core_sim_duration_scale(duration, (CoreSimTimeRatio){UINT64_MAX, UINT64_MAX}, &result));
    CHECK(result.ticks == UINT64_MAX);
    OK(core_sim_duration_scale(duration, (CoreSimTimeRatio){1, UINT64_MAX}, &result)); CHECK(result.ticks == 1);
    OK(core_sim_time_ratio_make(UINT64_MAX, UINT64_MAX, &ratio));
    CHECK(ratio.numerator == 1 && ratio.denominator == 1);
    base.ticks_per_second = 1;
    OK(core_sim_time_point_from_seconds(base, (CoreSimTimeRatio){UINT64_MAX, 1}, &point));
    CHECK(point.ticks == UINT64_MAX);
    OK(core_sim_time_point_add(point, (CoreSimDuration){base, 0}, &point));
    CHECK(point.ticks == UINT64_MAX);
    return 0;
}

/* Independent small-integer oracles use cross multiplication and brute-force
 * minimum rate search, not the implementation's GCD/LCM algorithm. */
static int test_rational_oracles(void) {
    uint64_t n, d, rate, ticks, a, b;
    for (rate = 1; rate <= 32; ++rate) for (d = 1; d <= 20; ++d)
        for (n = 0; n <= 32; ++n) {
            CoreSimDuration out = {{1, rate}, 999};
            CoreSimTimeStatus status = core_sim_duration_from_seconds(
                out.timebase, (CoreSimTimeRatio){n, d}, &out);
            if ((n * rate) % d) { CHECK(status == CORE_SIM_TIME_UNREPRESENTABLE); CHECK(out.ticks == 999); }
            else {
                CoreSimTimeRatio seconds;
                CHECK(status == CORE_SIM_TIME_OK); CHECK(out.ticks * d == n * rate);
                OK(core_sim_duration_to_seconds(out, &seconds));
                CHECK(seconds.numerator * d == n * seconds.denominator);
            }
        }
    for (a = 1; a <= 12; ++a) for (b = 1; b <= 12; ++b) {
        CoreSimTimeRatio periods[] = {{2, 2*a}, {3, 3*b}};
        CoreSimTimebase base;
        for (rate = 1; rate % a || rate % b; ++rate) { }
        OK(core_sim_timebase_from_periods(99, periods, 2, 1000, &base));
        CHECK(base.ticks_per_second == rate);
    }
    for (ticks = 0; ticks <= 20; ++ticks) for (n = 0; n <= 12; ++n)
        for (d = 1; d <= 12; ++d) {
            CoreSimDuration input = {{1, 1}, ticks}, out = {{1, 1}, 999};
            CoreSimTimeStatus status = core_sim_duration_scale(input, (CoreSimTimeRatio){n, d}, &out);
            if ((ticks*n) % d) { CHECK(status == CORE_SIM_TIME_UNREPRESENTABLE); CHECK(out.ticks == 999); }
            else { CHECK(status == CORE_SIM_TIME_OK); CHECK(out.ticks*d == ticks*n); }
        }
    return 0;
}

int main(void) {
    if (test_known_rates() || test_rejections() || test_integer_limits() || test_rational_oracles()) return 1;
    printf("core_sim_time_test: PASS (%u checks)\n", checks);
    return 0;
}
