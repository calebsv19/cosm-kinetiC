#ifndef PHYSICS_SIM_CFD_MEMORY_H
#define PHYSICS_SIM_CFD_MEMORY_H
#include <stddef.h>
/* Single-owner numerical allocation budget. The budget must outlive every
 * allocation charged to it. Scope selection is thread-local; sharing a live
 * budget or its allocations between threads requires external synchronization.
 * Counts include allocation headers and overlapping resize buffers, not libc
 * allocator overhead, executable mappings, JSON, or total process RSS. */
typedef enum {
    CFD_MEMORY_OK = 0,
    CFD_MEMORY_LIMIT,
    CFD_MEMORY_SIZE_OVERFLOW,
    CFD_MEMORY_SYSTEM_FAILURE
} CfdMemoryFailure;
typedef struct {
    size_t limit_bytes, live_bytes, peak_bytes, rejected_allocations, successful_allocations;
    size_t last_requested_bytes;
    CfdMemoryFailure last_failure;
} CfdMemoryBudget;
/* Returns the preceding scope for restoration. NULL means unbounded. A
 * non-NULL budget with limit_bytes==0 rejects all allocations. Existing
 * blocks retain their owner even when freed/resized outside its scope. */
CfdMemoryBudget *cfd_memory_scope(CfdMemoryBudget *budget);
void *cfd_memory_malloc(size_t bytes);
void *cfd_memory_calloc(size_t count, size_t bytes);
void *cfd_memory_realloc(void *pointer, size_t bytes);
void cfd_memory_free(void *pointer);
#endif
