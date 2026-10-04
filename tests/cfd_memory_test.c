#include "app/cfd_memory.h"
#include "app/cfd_refined_channel.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    bool completed;
    size_t peak;
    double sample;
} Result;
static Result exercise(size_t limit) {
    CfdMemoryBudget budget = {.limit_bytes = limit};
    CfdMemoryBudget *previous = cfd_memory_scope(&budget);
    CfdRefinedMesh mesh = {0};
    CfdRefinedChannel channel = {0};
    CfdRefinementRegion regions[] = {{1, .5, 3, 1.5, 1}, {1.375, .625, 1.625, .875, 3}};
    bool ready = cfd_refined_mesh_init_regions(&mesh, 16, 8, 4, 2, regions, 2, 10000);
    if (ready)
        ready = cfd_refined_mesh_remove_rectangle(&mesh, 1.5, .75, 2.5, 1.25);
    if (ready)
        ready = cfd_refined_channel_init(&channel, &mesh, 1, .1, .002);
    if (ready)
        ready = cfd_refined_channel_step(&channel, .005);
    if (ready)
        ready = cfd_refined_channel_step(&channel, .005);
    size_t peak = budget.peak_bytes, live = budget.live_bytes,
           allocations = budget.successful_allocations;
    if (ready) {
        ready = cfd_refined_channel_step(&channel, .005);
        assert(budget.peak_bytes == peak && budget.live_bytes == live &&
               budget.successful_allocations == allocations);
    }
    Result result = {ready, budget.peak_bytes, ready ? channel.u[0] : 0};
    if (!ready)
        assert(budget.rejected_allocations > 0 && budget.last_failure == CFD_MEMORY_LIMIT);
    assert(budget.live_bytes <= limit && budget.peak_bytes <= limit);
    cfd_memory_scope(previous);
    /* Destruction outside the active scope must still release the owner. */
    cfd_refined_channel_destroy(&channel);
    cfd_refined_mesh_destroy(&mesh);
    assert(budget.live_bytes == 0);
    printf("limit=%zu completed=%d numerical_peak=%zu rejections=%zu\n", limit, ready,
           budget.peak_bytes, budget.rejected_allocations);
    return result;
}
static void primitives(void) {
    CfdMemoryBudget a = {.limit_bytes = 256}, b = {.limit_bytes = 4096};
    CfdMemoryBudget *old = cfd_memory_scope(&a);
    unsigned char *p = cfd_memory_calloc(8, 4);
    assert(p);
    assert((uintptr_t)p % _Alignof(max_align_t) == 0);
    for (int i = 0; i < 32; i++)
        assert(p[i] == 0);
    memset(p, 0xa5, 32);
    size_t before = a.live_bytes;
    assert(!cfd_memory_realloc(p, 256));
    assert(a.live_bytes == before && a.last_failure == CFD_MEMORY_LIMIT);
    for (int i = 0; i < 32; i++)
        assert(p[i] == 0xa5);
    assert(!cfd_memory_calloc(SIZE_MAX, 2));
    assert(a.last_failure == CFD_MEMORY_SIZE_OVERFLOW);
    assert(!cfd_memory_malloc(SIZE_MAX));
    assert(a.last_failure == CFD_MEMORY_SIZE_OVERFLOW);
    cfd_memory_scope(&b);
    p = cfd_memory_realloc(p, 64);
    assert(p && b.live_bytes == 0);
    for (int i = 0; i < 32; i++)
        assert(p[i] == 0xa5);
    cfd_memory_free(p);
    assert(a.live_bytes == 0);
    void *q = cfd_memory_malloc(1);
    assert(q && b.live_bytes > 0);
    assert(cfd_memory_realloc(q, 0) == NULL && b.live_bytes == 0);
    cfd_memory_scope(old);
}
int main(void) {
    primitives();
    Result unlimited = exercise(SIZE_MAX);
    assert(unlimited.completed);
    size_t limits[] = {0, 1024, 8192, 65536, 262144, 1048576};
    for (size_t i = 0; i < sizeof(limits) / sizeof(*limits); i++)
        exercise(limits[i]);
    Result below = exercise(unlimited.peak - 1), exact = exercise(unlimited.peak);
    assert(!below.completed && exact.completed && exact.sample == unlimited.sample);
    puts("budget enforcement, resize peak, failed-allocation cleanup, scope ownership and "
         "cached-step reuse passed");
    return 0;
}
