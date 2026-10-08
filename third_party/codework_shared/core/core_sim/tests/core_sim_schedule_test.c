#include "core_sim_schedule.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(c) do { ++checks; if (!(c)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); return 1; \
} } while (0)
#define OK(c) CHECK((c) == CORE_SIM_SCHEDULE_OK)
#define ERROR_UNCHANGED(call, output, expected) do { \
    unsigned char saved[sizeof(output)]; memcpy(saved, &(output), sizeof(output)); \
    CHECK((call) == (expected)); CHECK(memcmp(saved, &(output), sizeof(output)) == 0); \
} while (0)

static CoreSimSchedule plan_for(CoreSimParticipantSchedule *p,
    uint64_t rate, uint64_t start, uint64_t end, uint64_t exchange) {
    CoreSimTimebase base = {7, rate};
    CoreSimSchedule result = {11, {{base, start}, {base, end}}, {base, exchange}, p, 2};
    return result;
}

static int test_episode_and_fps(void) {
    CoreSimTimebase base = {7, 600};
    CoreSimParticipantSchedule p[] = {{1, {base, 12}}, {2, {base, 3}}};
    CoreSimSchedule plan = plan_for(p, 600, 0, 24000, 120), initialized;
    uint64_t count, w, id, step, fps;
    CoreSimTimeInterval previous = plan.horizon, current;
    CoreSimWindowSteps range;
    CoreSimObservationBracket bracket;
    OK(core_sim_schedule_init(plan.plan_id, plan.horizon, plan.exchange, p, 2, &initialized));
    CHECK(initialized.plan_id == 11 && initialized.participants == p);
    OK(core_sim_schedule_window_count(&plan, &count)); CHECK(count == 200);
    OK(core_sim_schedule_step_count(&plan, 1, &count)); CHECK(count == 2000);
    OK(core_sim_schedule_step_count(&plan, 2, &count)); CHECK(count == 8000);
    for (w = 0; w < 200; ++w) {
        OK(core_sim_schedule_window(&plan, w, &current));
        CHECK(current.begin.ticks == w * 120 && current.end.ticks == (w+1)*120);
        if (w) CHECK(previous.end.ticks == current.begin.ticks);
        previous = current;
        for (id = 1; id <= 2; ++id) {
            uint64_t dt = id == 1 ? 12 : 3;
            OK(core_sim_schedule_window_steps(&plan, id, w, &range));
            CHECK(range.first_step_index == w*(120/dt) && range.step_count == 120/dt);
            CHECK(range.window.begin.ticks == current.begin.ticks && range.window.end.ticks == current.end.ticks);
            for (step = 0; step < range.step_count; ++step) {
                CoreSimTimeInterval s;
                OK(core_sim_schedule_step(&plan, id, range.first_step_index+step, &s));
                CHECK(s.begin.ticks == current.begin.ticks+step*dt && s.end.ticks == s.begin.ticks+dt);
                CHECK(s.end.ticks <= current.end.ticks);
            }
        }
    }
    /* Observation cadence is an external query sequence, never plan state. */
    for (fps = 5; fps <= 60; fps = fps == 5 ? 25 : 60) {
        uint64_t frame;
        for (frame = 1; frame <= 40*fps; ++frame) {
            CoreSimTimePoint t = {base, frame*(600/fps)};
            OK(core_sim_schedule_observation_bracket(&plan, 1, t, &bracket));
            CHECK(bracket.lower.ticks <= t.ticks && bracket.upper.ticks >= t.ticks);
            CHECK(bracket.lower_step_index == t.ticks/12);
            CHECK(bracket.upper_step_index == t.ticks/12+(t.ticks%12 != 0));
        }
        OK(core_sim_schedule_window_count(&plan, &count)); CHECK(count == 200);
        OK(core_sim_schedule_step_count(&plan, 1, &count)); CHECK(count == 2000);
        OK(core_sim_schedule_step_count(&plan, 2, &count)); CHECK(count == 8000);
        if (fps == 60) break;
    }
    /* Swap which participant is faster; no Fire/Fluid-specific policy. */
    p[0].step.ticks = 3; p[1].step.ticks = 12;
    plan.plan_id = 12;
    OK(core_sim_schedule_step_count(&plan, 1, &count)); CHECK(count == 8000);
    OK(core_sim_schedule_step_count(&plan, 2, &count)); CHECK(count == 2000);
    /* Same physical schedule at a different exact timebase. */
    p[0].step = (CoreSimDuration){{7, 200}, 1};
    p[1].step = (CoreSimDuration){{7, 200}, 4};
    plan = plan_for(p, 200, 0, 8000, 40);
    OK(core_sim_schedule_window_count(&plan, &count)); CHECK(count == 200);
    OK(core_sim_schedule_step_count(&plan, 1, &count)); CHECK(count == 8000);
    OK(core_sim_schedule_step_count(&plan, 2, &count)); CHECK(count == 2000);
    return 0;
}

static int test_small_bracket_oracle(void) {
    uint64_t start, stride, exchange, t;
    CoreSimTimebase base = {7, 60};
    /* Walk known state positions independently instead of using floor/ceil. */
    for (start = 0; start < 4; ++start) for (stride = 1; stride <= 8; ++stride)
        for (exchange = stride; exchange <= 24; exchange += stride) {
            CoreSimParticipantSchedule p[] = {{1, {base, stride}}, {2, {base, 1}}};
            CoreSimSchedule plan = plan_for(p, 60, start, start+3*exchange, exchange);
            for (t = start; t <= plan.horizon.end.ticks; ++t) {
                uint64_t lower = start, lower_index = 0, upper;
                CoreSimObservationBracket b;
                while (lower+stride <= t) { lower += stride; ++lower_index; }
                upper = lower == t ? lower : lower+stride;
                OK(core_sim_schedule_observation_bracket(&plan, 1, (CoreSimTimePoint){base, t}, &b));
                CHECK(b.lower.ticks == lower && b.upper.ticks == upper);
                CHECK(b.lower_step_index == lower_index);
                CHECK(b.upper_step_index == lower_index+(lower != t));
                CHECK(b.weight.denominator > 0);
                if (lower == upper) CHECK(b.weight.numerator == 0 && b.weight.denominator == 1);
                else CHECK(b.weight.numerator*(upper-lower) == (t-lower)*b.weight.denominator);
            }
        }
    return 0;
}

static int test_failures(void) {
    CoreSimTimebase base = {7, 200};
    CoreSimParticipantSchedule p[] = {{1, {base, 4}}, {2, {base, 1}}};
    CoreSimSchedule plan = plan_for(p, 200, 10, 8010, 40), out = plan, bad;
    CoreSimTimeInterval interval = plan.horizon;
    CoreSimObservationBracket b;
    CoreSimWindowSteps range;
    uint64_t count = 987;
    memset(&b, 0x57, sizeof(b)); memset(&range, 0x63, sizeof(range));
    CHECK(core_sim_schedule_validate(NULL) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_schedule_init(0, plan.horizon, plan.exchange, p, 2, &out), out, CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_schedule_window(NULL, 0, &interval), interval, CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    ERROR_UNCHANGED(core_sim_schedule_window_count(NULL, &count), count, CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    bad = plan; bad.participants = NULL;
    ERROR_UNCHANGED(core_sim_schedule_step_count(&bad, 1, &count), count, CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    bad = plan; bad.participant_count = 0;
    CHECK(core_sim_schedule_validate(&bad) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    bad = plan; bad.participant_count = CORE_SIM_SCHEDULE_MAX_PARTICIPANTS+1;
    CHECK(core_sim_schedule_validate(&bad) == CORE_SIM_SCHEDULE_CAPACITY);
    bad = plan; bad.exchange.ticks = 0;
    ERROR_UNCHANGED(core_sim_schedule_window(&bad, 0, &interval), interval, CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    bad = plan; bad.horizon.end = bad.horizon.begin;
    CHECK(core_sim_schedule_validate(&bad) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    bad = plan; bad.horizon.end.ticks = 9;
    CHECK(core_sim_schedule_validate(&bad) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    bad = plan; bad.horizon.end.timebase.domain = 8;
    CHECK(core_sim_schedule_validate(&bad) == CORE_SIM_SCHEDULE_DOMAIN_MISMATCH);
    bad = plan; bad.exchange.timebase.ticks_per_second = 600;
    CHECK(core_sim_schedule_validate(&bad) == CORE_SIM_SCHEDULE_DOMAIN_MISMATCH);
    bad = plan; ++bad.horizon.end.ticks;
    CHECK(core_sim_schedule_validate(&bad) == CORE_SIM_SCHEDULE_UNALIGNED);
    p[1].participant_id = 1;
    ERROR_UNCHANGED(core_sim_schedule_window_count(&plan, &count), count, CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    p[1].participant_id = 0;
    CHECK(core_sim_schedule_validate(&plan) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    p[1].participant_id = 2; p[1].step.ticks = 0;
    CHECK(core_sim_schedule_validate(&plan) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    p[1].step.ticks = 3;
    ERROR_UNCHANGED(core_sim_schedule_window_steps(&plan, 1, 0, &range), range, CORE_SIM_SCHEDULE_UNALIGNED);
    p[1].step.ticks = 80;
    CHECK(core_sim_schedule_validate(&plan) == CORE_SIM_SCHEDULE_UNALIGNED);
    p[1].step.ticks = 1; p[1].step.timebase.domain = 8;
    CHECK(core_sim_schedule_validate(&plan) == CORE_SIM_SCHEDULE_DOMAIN_MISMATCH);
    p[1].step.timebase = base;
    ERROR_UNCHANGED(core_sim_schedule_window(&plan, 200, &interval), interval, CORE_SIM_SCHEDULE_OUT_OF_RANGE);
    ERROR_UNCHANGED(core_sim_schedule_step(&plan, 1, 2000, &interval), interval, CORE_SIM_SCHEDULE_OUT_OF_RANGE);
    ERROR_UNCHANGED(core_sim_schedule_step(&plan, 3, 0, &interval), interval, CORE_SIM_SCHEDULE_PARTICIPANT_NOT_FOUND);
    ERROR_UNCHANGED(core_sim_schedule_step_count(&plan, 0, &count), count, CORE_SIM_SCHEDULE_PARTICIPANT_NOT_FOUND);
    ERROR_UNCHANGED(core_sim_schedule_window_steps(&plan, 3, 0, &range), range, CORE_SIM_SCHEDULE_PARTICIPANT_NOT_FOUND);
    ERROR_UNCHANGED(core_sim_schedule_observation_bracket(&plan, 1, (CoreSimTimePoint){base, 9}, &b), b, CORE_SIM_SCHEDULE_OUT_OF_RANGE);
    ERROR_UNCHANGED(core_sim_schedule_observation_bracket(&plan, 1, (CoreSimTimePoint){base, 8011}, &b), b, CORE_SIM_SCHEDULE_OUT_OF_RANGE);
    ERROR_UNCHANGED(core_sim_schedule_observation_bracket(&plan, 3, plan.horizon.begin, &b), b, CORE_SIM_SCHEDULE_PARTICIPANT_NOT_FOUND);
    ERROR_UNCHANGED(core_sim_schedule_observation_bracket(&plan, 1, (CoreSimTimePoint){{8, 200}, 10}, &b), b, CORE_SIM_SCHEDULE_DOMAIN_MISMATCH);
    ERROR_UNCHANGED(core_sim_schedule_observation_bracket(&plan, 1, (CoreSimTimePoint){{0, 200}, 10}, &b), b, CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    CHECK(core_sim_schedule_init(1, plan.horizon, plan.exchange, p, 2, NULL) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    CHECK(core_sim_schedule_window_count(&plan, NULL) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    CHECK(core_sim_schedule_step_count(&plan, 1, NULL) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    CHECK(core_sim_schedule_window(&plan, 0, NULL) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    CHECK(core_sim_schedule_step(&plan, 1, 0, NULL) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    CHECK(core_sim_schedule_window_steps(&plan, 1, 0, NULL) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    CHECK(core_sim_schedule_observation_bracket(&plan, 1, plan.horizon.begin, NULL) == CORE_SIM_SCHEDULE_INVALID_ARGUMENT);
    return 0;
}

static int test_large_run(void) {
    CoreSimTimebase base = {7, 1};
    CoreSimParticipantSchedule p[] = {{1, {base, 1}}, {2, {base, 1}}};
    CoreSimSchedule plan = plan_for(p, 1, 0, UINT64_MAX, 1);
    CoreSimTimeInterval interval;
    CoreSimWindowSteps range;
    CoreSimObservationBracket b;
    uint64_t count;
    OK(core_sim_schedule_window_count(&plan, &count)); CHECK(count == UINT64_MAX);
    OK(core_sim_schedule_window(&plan, UINT64_MAX-1, &interval));
    CHECK(interval.begin.ticks == UINT64_MAX-1 && interval.end.ticks == UINT64_MAX);
    OK(core_sim_schedule_step(&plan, 1, UINT64_MAX-1, &interval)); CHECK(interval.end.ticks == UINT64_MAX);
    OK(core_sim_schedule_window_steps(&plan, 2, UINT64_MAX-1, &range));
    CHECK(range.first_step_index == UINT64_MAX-1 && range.step_count == 1);
    OK(core_sim_schedule_observation_bracket(&plan, 1, plan.horizon.end, &b));
    CHECK(b.lower_step_index == UINT64_MAX && b.upper_step_index == UINT64_MAX);
    ERROR_UNCHANGED(core_sim_schedule_window(&plan, UINT64_MAX, &interval), interval, CORE_SIM_SCHEDULE_OUT_OF_RANGE);
    p[0].step.ticks = 2; p[1].step.ticks = 10;
    plan = plan_for(p, 1, UINT64_MAX-100, UINT64_MAX, 10);
    OK(core_sim_schedule_window(&plan, 9, &interval)); CHECK(interval.end.ticks == UINT64_MAX);
    OK(core_sim_schedule_observation_bracket(&plan, 1, (CoreSimTimePoint){base, UINT64_MAX-1}, &b));
    CHECK(b.lower.ticks == UINT64_MAX-2 && b.upper.ticks == UINT64_MAX);
    CHECK(b.weight.numerator == 1 && b.weight.denominator == 2);
    return 0;
}

int main(void) {
    if (test_episode_and_fps() || test_small_bracket_oracle() || test_failures() || test_large_run()) return 1;
    printf("core_sim_schedule_test: PASS (%u checks)\n", checks);
    return 0;
}
