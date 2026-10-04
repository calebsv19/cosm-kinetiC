#include "app/cfd_startup3d.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static bool cancel(void *c) { return ++(*(int *)c) < 4; }
static uint64_t field_hash(const CfdStartup3d *s) {
    uint64_t h = UINT64_C(1469598103934665603);
    for (int a = 0; a < 2; a++) {
        const unsigned char *v = (const unsigned char *)(a ? s->pressure : s->velocity);
        size_t bytes = (size_t)(a ? s->grid.count : s->count) * sizeof(double);
        for (size_t q = 0; q < bytes; q++)
            h = (h ^ v[q]) * UINT64_C(1099511628211);
    }
    return h;
}
static void references(CfdStartup3d *s) {
    double times[] = {.005, .5, 2, 12};
    for (int k = 0; k < 4; k++) {
        double previous[8] = {0};
        for (int terms = 128; terms <= 512; terms *= 2) {
            cfd_startup3d_reference(s, times[k], terms, NULL);
            double v[8] = {s->reference_flow,        s->reference_wall[0],    s->reference_wall[1],
                           s->reference_wall[2],     s->reference_wall[3],    s->reference_kinetic,
                           s->reference_dissipation, s->reference_energy_rate};
            if (terms > 128)
                for (int a = 0; a < 8; a++)
                    assert(times[k] < .5 ||
                           fabs(v[a] - previous[a]) < 1e-5 * fmax(fabs(v[a]), 1e-30));
            double imbalance =
                fabs(s->reference_power - s->reference_dissipation - s->reference_energy_rate) /
                s->reference_power;
            fprintf(stderr, "time=%g terms=%d energy imbalance=%g\n", times[k], terms, imbalance);
            assert((times[k] < .5 && terms < 512) || imbalance < 1e-5);
            memcpy(previous, v, sizeof(v));
            printf("{\"time\":%.17g,\"terms\":%d,\"reference_energy_imbalance\":%.17g}\n", times[k],
                   terms, imbalance);
        }
    }
}
int main(void) {
    int n[3] = {16, 8, 8};
    double l[3] = {4, 2, 2};
    for (int cap = 1; cap <= 1024; cap *= 2) {
        CfdMemoryBudget b = {.limit_bytes = (size_t)cap * 1024};
        CfdMemoryBudget *prev = cfd_memory_scope(&b);
        CfdStartup3d s;
        assert(!cfd_startup3d_init(&s, n, l, 1, .1, .02, .008));
        cfd_startup3d_destroy(&s);
        assert(b.live_bytes == 0 && b.last_failure == CFD_MEMORY_LIMIT);
        cfd_memory_scope(prev);
    }
    CfdMemoryBudget b = {.limit_bytes = 64 * 1024 * 1024};
    CfdMemoryBudget *prev = cfd_memory_scope(&b);
    CfdStartup3d s;
    assert(cfd_startup3d_init(&s, n, l, 1, .1, .02, .008));
#ifdef CFD_MIXED3D_VERIFY
    assert(cfd_mixed3d_verify(s.mixed));
#endif
    references(&s);
    assert(cfd_startup3d_step(&s) && cfd_startup3d_step(&s));
    size_t allocs = b.successful_allocations;
    assert(cfd_startup3d_step(&s));
    assert(allocs == b.successful_allocations);
    double time = s.time, u = s.velocity[0], p = s.pressure[0];
    uint64_t before = field_hash(&s);
    int calls = 0;
    cfd_mixed3d_checkpoint(s.mixed, cancel, &calls);
    assert(!cfd_startup3d_step(&s));
    assert(s.time == time && s.velocity[0] == u && s.pressure[0] == p);
    assert(before == field_hash(&s));
    assert(!strcmp(s.error, "cancelled_at_krylov_checkpoint"));
    cfd_startup3d_destroy(&s);
    assert(b.live_bytes == 0);
    cfd_memory_scope(prev);
    puts("startup reference tails/energy, open operators, cached steps, cancellation and cleanup "
         "passed");
    return 0;
}
