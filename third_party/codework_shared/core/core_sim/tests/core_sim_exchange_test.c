#include "core_sim_exchange.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long checks;
#define CHECK(v) do { ++checks; if (!(v)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #v); exit(1); } } while (0)
#define OK(v) CHECK((v) == CORE_SIM_EXCHANGE_OK)
#define ERROR(v, output, expected) do { \
    unsigned char saved[sizeof(output)]; memcpy(saved, &(output), sizeof(output)); \
    CHECK((v) == (expected)); CHECK(memcmp(saved, &(output), sizeof(output)) == 0); \
} while (0)

static CoreSimTimebase base = {19, 200};
static CoreSimTimePoint point(uint64_t ticks) { return (CoreSimTimePoint){base, ticks}; }
static CoreSimTimeInterval interval(uint64_t a, uint64_t b) {
    return (CoreSimTimeInterval){point(a), point(b)};
}
static CoreSimParticipantSchedule participants[] = {{1, {{19, 200}, 4}}, {2, {{19, 200}, 1}}};
static CoreSimSchedule plan(void) {
    return (CoreSimSchedule){11, interval(0, 120), {base, 40}, participants, 2};
}
static CoreSimChannelDesc channel(CoreSimTemporalKind kind, CoreSimTemporalPolicy policy) {
    CoreSimChannelDesc c = {0};
    c.semantic_version = 1;
    c.plan_id = 11; c.channel_id = 5;
    c.producer_id = 1; c.consumer_id = 2;
    c.quantity_id = 7; c.unit_id = 8; c.spatial_support_id = 9;
    c.adapter_id = 10; c.profile_id = 12;
    c.kind = kind; c.policy = policy;
    c.max_age = (CoreSimDuration){base, policy == CORE_SIM_POLICY_HOLD ? 8 : 0};
    return c;
}
static CoreSimExchangeRecord sample(CoreSimTemporalKind kind, uint64_t time, uint64_t seq) {
    CoreSimExchangeRecord r = {0};
    r.semantic_version = 1; r.plan_id = 11; r.channel_id = 5;
    r.sequence = seq; r.checkpoint_ref = 20 + seq; r.payload_ref = 30 + seq;
    r.kind = kind; r.checkpoint_time = point(time); r.support.point = point(time);
    return r;
}
static CoreSimExchangeRecord amount(uint64_t begin, uint64_t end) {
    CoreSimExchangeRecord r = sample(CORE_SIM_TEMPORAL_INTEGRATED, end, 1);
    r.support.interval = interval(begin, end);
    return r;
}

static void samples(void) {
    CoreSimSchedule p = plan();
    CoreSimChannelDesc c = channel(CORE_SIM_TEMPORAL_POINT, CORE_SIM_POLICY_EXACT);
    CoreSimExchangeRecord lo = sample(c.kind, 8, 1), hi = sample(c.kind, 20, 2);
    CoreSimExchangeSample out;
    memset(&out, 0xA5, sizeof(out));
    OK(core_sim_exchange_sample(&p, &c, point(8), point(8), &lo, NULL, &out));
    CHECK(out.lower_sequence == 1 && out.upper_sequence == 1);
    CHECK(out.weight.numerator == 0 && out.weight.denominator == 1 && out.age.ticks == 0);
    ERROR(core_sim_exchange_sample(&p, &c, point(9), point(20), &lo, NULL, &out), out,
        CORE_SIM_EXCHANGE_POLICY_UNAVAILABLE);
    ERROR(core_sim_exchange_sample(&p, &c, point(7), point(20), &lo, NULL, &out), out,
        CORE_SIM_EXCHANGE_FUTURE);
    ERROR(core_sim_exchange_sample(&p, &c, point(8), point(7), &lo, NULL, &out), out,
        CORE_SIM_EXCHANGE_FUTURE);
    ERROR(core_sim_exchange_sample(&p, &c, point(8), point(20), &lo, &hi, &out), out,
        CORE_SIM_EXCHANGE_POLICY_UNAVAILABLE);
    c.policy = CORE_SIM_POLICY_HOLD; c.max_age.ticks = 8;
    OK(core_sim_exchange_sample(&p, &c, point(16), point(8), &lo, NULL, &out));
    CHECK(out.age.ticks == 8 && out.weight.numerator == 0);
    ERROR(core_sim_exchange_sample(&p, &c, point(17), point(20), &lo, NULL, &out), out,
        CORE_SIM_EXCHANGE_STALE);
    ERROR(core_sim_exchange_sample(&p, &c, point(16), point(20), &lo, &hi, &out), out,
        CORE_SIM_EXCHANGE_POLICY_UNAVAILABLE);
    /* A later-window hold explicitly binds the older accepted sample. */
    lo.window_index = 1; c.max_age.ticks = 40;
    OK(core_sim_exchange_sample(&p, &c, point(40), point(8), &lo, NULL, &out));
    CHECK(out.age.ticks == 32);
    ERROR(core_sim_exchange_sample(&p, &c, point(39), point(8), &lo, NULL, &out), out,
        CORE_SIM_EXCHANGE_OUT_OF_RANGE);
    lo.window_index = 0;
    c.policy = CORE_SIM_POLICY_LINEAR; c.max_age.ticks = 0;
    OK(core_sim_exchange_sample(&p, &c, point(13), point(20), &lo, &hi, &out));
    CHECK(out.weight.numerator == 5 && out.weight.denominator == 12);
    CHECK(out.lower_sequence == 1 && out.upper_sequence == 2 && out.age.ticks == 0);
    ERROR(core_sim_exchange_sample(&p, &c, point(13), point(19), &lo, &hi, &out), out,
        CORE_SIM_EXCHANGE_FUTURE);
    ERROR(core_sim_exchange_sample(&p, &c, point(13), point(20), &lo, NULL, &out), out,
        CORE_SIM_EXCHANGE_POLICY_UNAVAILABLE);
    ERROR(core_sim_exchange_sample(&p, &c, point(21), point(20), &lo, &hi, &out), out,
        CORE_SIM_EXCHANGE_POLICY_UNAVAILABLE);
    ERROR(core_sim_exchange_sample(&p, &c, point(13), point(20), &hi, &lo, &out), out,
        CORE_SIM_EXCHANGE_FUTURE);
    hi.sequence = lo.sequence;
    ERROR(core_sim_exchange_sample(&p, &c, point(13), point(20), &lo, &hi, &out), out,
        CORE_SIM_EXCHANGE_POLICY_UNAVAILABLE);
    hi.sequence = 2; hi.window_index = 1;
    ERROR(core_sim_exchange_sample(&p, &c, point(13), point(20), &lo, &hi, &out), out,
        CORE_SIM_EXCHANGE_IDENTITY_MISMATCH);
    hi.window_index = 0;
    OK(core_sim_exchange_sample(&p, &c, point(8), point(8), &lo, NULL, &out));
    /* Every bracket inside the window, oracle by independent tick counting. */
    {
        uint64_t a, b, t, n;
        for (a = 0; a < 40; ++a) for (b = a+1; b <= 40; ++b) {
            lo = sample(c.kind, a, 1); hi = sample(c.kind, b, 2);
            for (t = a; t <= b; ++t) {
                uint64_t elapsed = 0;
                for (n = a; n < t; ++n) ++elapsed;
                OK(core_sim_exchange_sample(&p, &c, point(t), point(b), &lo, &hi, &out));
                CHECK(out.weight.numerator * (b-a) == elapsed * out.weight.denominator);
            }
        }
    }
    c = channel(CORE_SIM_TEMPORAL_DISCRETE, CORE_SIM_POLICY_EXACT);
    lo = sample(c.kind, 0, 1);
    OK(core_sim_exchange_sample(&p, &c, point(0), point(0), &lo, NULL, &out));
    c.policy = CORE_SIM_POLICY_HOLD; c.max_age.ticks = 8;
    OK(core_sim_exchange_sample(&p, &c, point(8), point(0), &lo, NULL, &out));
    c.policy = CORE_SIM_POLICY_LINEAR; c.max_age.ticks = 0;
    ERROR(core_sim_exchange_sample(&p, &c, point(4), point(8), &lo, NULL, &out), out,
        CORE_SIM_EXCHANGE_UNSUPPORTED);
}

static void overlaps(void) {
    CoreSimSchedule p = plan();
    CoreSimChannelDesc c = channel(CORE_SIM_TEMPORAL_INTEGRATED, CORE_SIM_POLICY_UNIFORM_RATE);
    CoreSimExchangeRecord r;
    CoreSimExchangeOverlap out;
    uint64_t a, b, x, y, t;
    memset(&out, 0xA5, sizeof(out));
    for (a = 0; a < 12; ++a) for (b = a+1; b <= 12; ++b) {
        r = amount(a,b);
        for (x = 0; x < 15; ++x) for (y = x+1; y <= 15; ++y) {
            uint64_t counted = 0;
            for (t = 0; t < 15; ++t) if (t >= a && t < b && t >= x && t < y) ++counted;
            OK(core_sim_exchange_overlap(&p, &c, &r, point(b), interval(x,y), &out));
            CHECK(out.duration.ticks == counted);
            CHECK(out.source_fraction.numerator*(b-a) == counted*out.source_fraction.denominator);
            CHECK(out.target_fraction.numerator*(y-x) == counted*out.target_fraction.denominator);
        }
        /* All two-way partitions allocate exactly the supplied integer amount
         * of b-a units: ratio conversion remains exact and never rounds. */
        for (x = a+1; x < b; ++x) {
            CoreSimDuration allocated;
            uint64_t total;
            OK(core_sim_exchange_overlap(&p, &c, &r, point(b), interval(a,x), &out));
            CHECK(core_sim_duration_scale((CoreSimDuration){base,b-a}, out.source_fraction,
                &allocated) == CORE_SIM_TIME_OK);
            total = allocated.ticks;
            OK(core_sim_exchange_overlap(&p, &c, &r, point(b), interval(x,b), &out));
            CHECK(core_sim_duration_scale((CoreSimDuration){base,b-a}, out.source_fraction,
                &allocated) == CORE_SIM_TIME_OK);
            CHECK(total + allocated.ticks == b-a);
        }
    }
    r = amount(0,40);
    ERROR(core_sim_exchange_overlap(&p, &c, &r, point(39), interval(0,1), &out), out,
        CORE_SIM_EXCHANGE_FUTURE);
    ERROR(core_sim_exchange_overlap(&p, &c, &r, point(40), interval(1,1), &out), out,
        CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    ERROR(core_sim_exchange_overlap(&p, &c, &r, point(40), interval(2,1), &out), out,
        CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    ERROR(core_sim_exchange_overlap(&p, &c, &r, point(40), interval(0,121), &out), out,
        CORE_SIM_EXCHANGE_OUT_OF_RANGE);
    {
        CoreSimTimeInterval foreign = interval(0,1); foreign.end.timebase.domain++;
        ERROR(core_sim_exchange_overlap(&p, &c, &r, point(40), foreign, &out), out,
            CORE_SIM_EXCHANGE_DOMAIN_MISMATCH);
    }
    c = channel(CORE_SIM_TEMPORAL_POINT, CORE_SIM_POLICY_EXACT); r = sample(c.kind, 0, 1);
    ERROR(core_sim_exchange_overlap(&p, &c, &r, point(0), interval(0,1), &out), out,
        CORE_SIM_EXCHANGE_UNSUPPORTED);
}

static void identities_and_validation(void) {
    CoreSimSchedule p = plan(), bad_plan;
    CoreSimChannelDesc c = channel(CORE_SIM_TEMPORAL_POINT, CORE_SIM_POLICY_EXACT), bad;
    CoreSimExchangeRecord a = sample(c.kind, 8, 1), b;
    CoreSimExchangeRelation relation = CORE_SIM_EXCHANGE_DISTINCT;
    CoreSimExchangeSample output;
    CoreSimExchangeOverlap overlap;
    unsigned i;
    OK(core_sim_channel_validate(&p, &c));
    OK(core_sim_exchange_record_validate(&p, &c, &a));
    OK(core_sim_exchange_record_compare(&p, &c, &a, &a, &relation));
    CHECK(relation == CORE_SIM_EXCHANGE_REPLAY);
    b = a; b.payload_ref++;
    OK(core_sim_exchange_record_compare(&p, &c, &a, &b, &relation));
    CHECK(relation == CORE_SIM_EXCHANGE_CONFLICT);
    b = a; b.checkpoint_ref++;
    OK(core_sim_exchange_record_compare(&p, &c, &a, &b, &relation));
    CHECK(relation == CORE_SIM_EXCHANGE_CONFLICT);
    b = a; b.support.point.ticks++; b.checkpoint_time.ticks++;
    OK(core_sim_exchange_record_compare(&p, &c, &a, &b, &relation));
    CHECK(relation == CORE_SIM_EXCHANGE_CONFLICT);
    b = a; b.sequence++;
    OK(core_sim_exchange_record_compare(&p, &c, &a, &b, &relation));
    CHECK(relation == CORE_SIM_EXCHANGE_DISTINCT);
    b = a; b.window_index++;
    OK(core_sim_exchange_record_compare(&p, &c, &a, &b, &relation));
    CHECK(relation == CORE_SIM_EXCHANGE_DISTINCT);
    b = a; b.plan_id++;
    ERROR(core_sim_exchange_record_compare(&p, &c, &a, &b, &relation), relation,
        CORE_SIM_EXCHANGE_IDENTITY_MISMATCH);
    for (i = 0; i < 17; ++i) {
        bad = c;
        switch (i) {
        case 0: bad.semantic_version = 0; break;
        case 1: bad.required_features = 1; break;
        case 2: bad.channel_id = 0; break;
        case 3: bad.producer_id = 0; break;
        case 4: bad.consumer_id = bad.producer_id; break;
        case 5: bad.quantity_id = 0; break;
        case 6: bad.unit_id = 0; break;
        case 7: bad.spatial_support_id = 0; break;
        case 8: bad.adapter_id = 0; break;
        case 9: bad.profile_id = 0; break;
        case 10: bad.max_age.timebase.domain = 0; break;
        case 11: bad.max_age.timebase.ticks_per_second = 0; break;
        case 12: bad.kind = (CoreSimTemporalKind)99; break;
        case 13: bad.policy = (CoreSimTemporalPolicy)99; break;
        case 14: bad.max_age.ticks = 1; break;
        case 15: bad.plan_id = 0; break;
        default: bad.producer_id = 99; break;
        }
        CHECK(core_sim_channel_validate(&p,&bad) != CORE_SIM_EXCHANGE_OK);
    }
    bad = c; bad.plan_id++;
    CHECK(core_sim_channel_validate(&p,&bad) == CORE_SIM_EXCHANGE_IDENTITY_MISMATCH);
    bad = c; bad.max_age.timebase.ticks_per_second++;
    CHECK(core_sim_channel_validate(&p,&bad) == CORE_SIM_EXCHANGE_DOMAIN_MISMATCH);
    bad = c; bad.policy = CORE_SIM_POLICY_HOLD;
    CHECK(core_sim_channel_validate(&p,&bad) == CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    for (i = 0; i < 12; ++i) {
        static const CoreSimExchangeStatus expected[] = {
            CORE_SIM_EXCHANGE_UNSUPPORTED, CORE_SIM_EXCHANGE_UNSUPPORTED,
            CORE_SIM_EXCHANGE_INVALID_ARGUMENT, CORE_SIM_EXCHANGE_INVALID_ARGUMENT,
            CORE_SIM_EXCHANGE_INVALID_ARGUMENT, CORE_SIM_EXCHANGE_IDENTITY_MISMATCH,
            CORE_SIM_EXCHANGE_IDENTITY_MISMATCH, CORE_SIM_EXCHANGE_OUT_OF_RANGE,
            CORE_SIM_EXCHANGE_DOMAIN_MISMATCH, CORE_SIM_EXCHANGE_DOMAIN_MISMATCH,
            CORE_SIM_EXCHANGE_OUT_OF_RANGE, CORE_SIM_EXCHANGE_OUT_OF_RANGE
        };
        b = a;
        switch (i) {
        case 0: b.semantic_version++; break;
        case 1: b.required_features = UINT64_MAX; break;
        case 2: b.sequence = 0; break;
        case 3: b.checkpoint_ref = 0; break;
        case 4: b.payload_ref = 0; break;
        case 5: b.channel_id++; break;
        case 6: b.kind = CORE_SIM_TEMPORAL_INTEGRATED; break;
        case 7: b.window_index = UINT64_MAX; break;
        case 8: b.checkpoint_time.timebase.domain++; break;
        case 9: b.support.point.timebase.ticks_per_second++; break;
        case 10: b.support.point.ticks = 41; b.checkpoint_time.ticks = 41; break;
        default: b.support.point.ticks++; break;
        }
        ERROR(core_sim_exchange_record_compare(&p, &c, &a, &b, &relation), relation,
            expected[i]);
    }
    CHECK(core_sim_channel_validate(NULL,&c) == CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    CHECK(core_sim_channel_validate(&p,NULL) == CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    bad_plan = p; bad_plan.exchange.ticks = 3;
    CHECK(core_sim_channel_validate(&bad_plan,&c) == CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    CHECK(core_sim_exchange_record_validate(&p,&c,NULL) == CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    CHECK(core_sim_exchange_record_compare(&p,&c,&a,&a,NULL) == CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    CHECK(core_sim_exchange_sample(&p,&c,point(8),point(8),&a,NULL,NULL) == CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    CHECK(core_sim_exchange_overlap(&p,&c,&a,point(8),interval(0,1),NULL) == CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    memset(&output,0x5A,sizeof(output)); memset(&overlap,0x5A,sizeof(overlap));
    ERROR(core_sim_exchange_sample(&p,&c,point(8),point(8),NULL,NULL,&output), output,
        CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    {
        CoreSimTimePoint foreign = point(8); foreign.timebase.domain++;
        ERROR(core_sim_exchange_sample(&p,&c,foreign,point(8),&a,NULL,&output), output,
            CORE_SIM_EXCHANGE_DOMAIN_MISMATCH);
        foreign = point(8); foreign.timebase.ticks_per_second++;
        ERROR(core_sim_exchange_sample(&p,&c,point(8),foreign,&a,NULL,&output), output,
            CORE_SIM_EXCHANGE_DOMAIN_MISMATCH);
    }
    c = channel(CORE_SIM_TEMPORAL_INTEGRATED,CORE_SIM_POLICY_UNIFORM_RATE);
    a = amount(4,20); b = a; b.support.interval.begin.ticks++;
    OK(core_sim_exchange_record_compare(&p,&c,&a,&b,&relation));
    CHECK(relation == CORE_SIM_EXCHANGE_CONFLICT);
    ERROR(core_sim_exchange_sample(&p,&c,point(20),point(20),&a,NULL,&output), output,
        CORE_SIM_EXCHANGE_UNSUPPORTED);
    b = a; b.support.interval.begin.ticks = b.support.interval.end.ticks;
    CHECK(core_sim_exchange_record_validate(&p,&c,&b) == CORE_SIM_EXCHANGE_INVALID_ARGUMENT);
    b = a; b.checkpoint_time.ticks++;
    CHECK(core_sim_exchange_record_validate(&p,&c,&b) == CORE_SIM_EXCHANGE_OUT_OF_RANGE);
    b = a; b.window_index = 1;
    CHECK(core_sim_exchange_record_validate(&p,&c,&b) == CORE_SIM_EXCHANGE_OUT_OF_RANGE);
    c.policy = CORE_SIM_POLICY_LINEAR;
    CHECK(core_sim_channel_validate(&p,&c) == CORE_SIM_EXCHANGE_UNSUPPORTED);
}

static void policy_matrix_and_windows(void) {
    CoreSimSchedule p = plan();
    CoreSimTemporalKind kind;
    CoreSimTemporalPolicy policy;
    uint64_t w;
    for (kind = CORE_SIM_TEMPORAL_INTEGRATED; kind <= CORE_SIM_TEMPORAL_DISCRETE; ++kind)
        for (policy = CORE_SIM_POLICY_UNIFORM_RATE; policy <= CORE_SIM_POLICY_LINEAR; ++policy) {
            int supported = (kind == CORE_SIM_TEMPORAL_INTEGRATED && policy == CORE_SIM_POLICY_UNIFORM_RATE) ||
                (kind == CORE_SIM_TEMPORAL_POINT && policy != CORE_SIM_POLICY_UNIFORM_RATE) ||
                (kind == CORE_SIM_TEMPORAL_DISCRETE && (policy == CORE_SIM_POLICY_EXACT || policy == CORE_SIM_POLICY_HOLD));
            CoreSimChannelDesc c = channel(kind,policy);
            CHECK(core_sim_channel_validate(&p,&c) == (supported ? CORE_SIM_EXCHANGE_OK : CORE_SIM_EXCHANGE_UNSUPPORTED));
        }
    for (w = 0; w < 3; ++w) {
        CoreSimChannelDesc c = channel(CORE_SIM_TEMPORAL_INTEGRATED,CORE_SIM_POLICY_UNIFORM_RATE);
        CoreSimExchangeRecord r = amount(40*w,40*(w+1));
        CoreSimExchangeOverlap out;
        uint64_t j, sum = 0;
        r.window_index = w;
        for (j = 0; j < 40; ++j) {
            OK(core_sim_exchange_overlap(&p,&c,&r,point(40*(w+1)),interval(40*w+j,40*w+j+1),&out));
            CHECK(out.duration.ticks == 1 && out.source_fraction.numerator == 1 && out.source_fraction.denominator == 40);
            sum += out.duration.ticks;
        }
        CHECK(sum == 40);
        c = channel(CORE_SIM_TEMPORAL_POINT,CORE_SIM_POLICY_EXACT);
        r = sample(c.kind,40*(w+1),1); r.window_index = w;
        {
            CoreSimExchangeSample out_sample;
            OK(core_sim_exchange_sample(&p,&c,point(40*(w+1)),point(40*(w+1)),&r,NULL,&out_sample));
            CHECK(out_sample.age.ticks == 0);
        }
    }
}

static void limits(void) {
    CoreSimSchedule p = plan();
    CoreSimChannelDesc c = channel(CORE_SIM_TEMPORAL_INTEGRATED, CORE_SIM_POLICY_UNIFORM_RATE);
    CoreSimExchangeRecord r = amount(0,UINT64_MAX);
    CoreSimExchangeOverlap out;
    CoreSimExchangeSample bracket;
    CoreSimExchangeRecord lo,hi;
    p.horizon = interval(0,UINT64_MAX); p.exchange.ticks = UINT64_MAX;
    /* Independent unit steps make this enormous horizon aligned. */
    CoreSimParticipantSchedule unit_steps[] = {{1,{base,1}},{2,{base,1}}};
    p.participants = unit_steps;
    OK(core_sim_exchange_overlap(&p,&c,&r,point(UINT64_MAX),interval(0,1),&out));
    CHECK(out.duration.ticks == 1 && out.source_fraction.numerator == 1 &&
        out.source_fraction.denominator == UINT64_MAX);
    OK(core_sim_exchange_overlap(&p,&c,&r,point(UINT64_MAX),interval(0,UINT64_MAX),&out));
    CHECK(out.source_fraction.numerator == 1 && out.source_fraction.denominator == 1);
    c = channel(CORE_SIM_TEMPORAL_POINT, CORE_SIM_POLICY_LINEAR);
    lo = sample(c.kind,0,1); hi = sample(c.kind,UINT64_MAX,2);
    OK(core_sim_exchange_sample(&p,&c,point(UINT64_MAX-1),point(UINT64_MAX),&lo,&hi,&bracket));
    CHECK(bracket.weight.numerator == UINT64_MAX-1 && bracket.weight.denominator == UINT64_MAX);
    c = channel(CORE_SIM_TEMPORAL_POINT, CORE_SIM_POLICY_HOLD); c.max_age.ticks = UINT64_MAX;
    OK(core_sim_exchange_sample(&p,&c,point(UINT64_MAX),point(0),&lo,NULL,&bracket));
    CHECK(bracket.age.ticks == UINT64_MAX);
    p.horizon = interval(UINT64_MAX-40, UINT64_MAX); p.exchange.ticks = 40;
    c = channel(CORE_SIM_TEMPORAL_INTEGRATED,CORE_SIM_POLICY_UNIFORM_RATE);
    r = amount(UINT64_MAX-40, UINT64_MAX);
    OK(core_sim_exchange_overlap(&p,&c,&r,point(UINT64_MAX),interval(UINT64_MAX-20,UINT64_MAX),&out));
    CHECK(out.source_fraction.numerator == 1 && out.source_fraction.denominator == 2);
}

int main(void) {
    samples(); overlaps(); identities_and_validation(); policy_matrix_and_windows(); limits();
    printf("core_sim_exchange_test: PASS (%lu checks)\n",checks);
    return 0;
}
