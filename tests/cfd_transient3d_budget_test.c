#include "app/cfd_startup3d.h"
#include "app/cfd_wall3d.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint64_t hash(const double *u, int m, const double *p, int n) {
    uint64_t h = UINT64_C(1469598103934665603);
    for (int a = 0; a < 2; a++) {
        const unsigned char *v = (const unsigned char *)(a ? p : u);
        for (size_t q = 0; q < (size_t)(a ? n : m) * sizeof(double); q++)
            h = (h ^ v[q]) * UINT64_C(1099511628211);
    }
    return h;
}
static size_t cycle(int kind, size_t cap, bool must_pass) {
    CfdMemoryBudget budget = {.limit_bytes = cap};
    CfdMemoryBudget *previous = cfd_memory_scope(&budget);
    CfdWall3d wall = {0};
    CfdStartup3d startup = {0};
    int n[3] = {kind >= 2 ? 16 : 8, 8, 8};
    double l[3] = {kind >= 2 ? 4 : 2, 2, 2};
    bool ready = kind == 3   ? cfd_startup3d_init(&startup, n, l, 1, .1, .01, .008)
                 : kind == 2 ? cfd_wall3d_init_open(&wall, n, l, 1, .1, .01)
                             : cfd_wall3d_init(&wall, n, l, 1, .1, .01, kind == 1);
    bool accepted = ready;
    if (ready) {
        double *u = kind == 3 ? startup.velocity : wall.velocity,
               *p = kind == 3 ? startup.pressure : wall.pressure;
        int m = kind == 3 ? startup.count : wall.count;
        int cells = kind == 3 ? startup.grid.count : wall.grid.count;
        for (int step = 0; step < 3; step++) {
            uint64_t before = hash(u, m, p, cells);
            double time = kind == 3 ? startup.time : wall.time;
            accepted = kind == 3 ? cfd_startup3d_step(&startup) : cfd_wall3d_step(&wall);
            if (!accepted) {
                assert(before == hash(u, m, p, cells));
                assert(time == (kind == 3 ? startup.time : wall.time));
                break;
            }
        }
    }
    assert(accepted == must_pass);
    if (!must_pass)
        assert(budget.last_failure == CFD_MEMORY_LIMIT && budget.rejected_allocations > 0);
    size_t peak = budget.peak_bytes;
    cfd_wall3d_destroy(&wall);
    cfd_startup3d_destroy(&startup);
    assert(budget.live_bytes == 0);
    cfd_memory_scope(previous);
    return peak;
}
int main(void) {
    for (int kind = 0; kind < 4; kind++) {
        size_t exact = cycle(kind, 64 * 1024 * 1024, true);
        assert(cycle(kind, exact, true) == exact);
        cycle(kind, exact - 1, false);
        printf("{\"mode_index\":%d,\"exact_peak_bytes\":%zu,\"one_byte_below_rejected\":true}\n",
               kind, exact);
    }
    return 0;
}
