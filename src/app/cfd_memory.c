#include "app/cfd_memory.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef union {
    max_align_t alignment;
    struct {
        CfdMemoryBudget *owner;
        size_t bytes;
    } allocation;
} Header;
static _Thread_local CfdMemoryBudget *active;
CfdMemoryBudget *cfd_memory_scope(CfdMemoryBudget *budget) {
    CfdMemoryBudget *previous = active;
    active = budget;
    return previous;
}
static void reject(CfdMemoryBudget *b, size_t bytes, CfdMemoryFailure failure) {
    if (b) {
        b->rejected_allocations++;
        b->last_requested_bytes = bytes;
        b->last_failure = failure;
    }
}
static void *allocate(CfdMemoryBudget *b, size_t bytes) {
    if (bytes > SIZE_MAX - sizeof(Header)) {
        reject(b, bytes, CFD_MEMORY_SIZE_OVERFLOW);
        return NULL;
    }
    size_t charge = bytes + sizeof(Header);
    if (b && (b->live_bytes > b->limit_bytes || charge > b->limit_bytes - b->live_bytes)) {
        reject(b, charge, CFD_MEMORY_LIMIT);
        return NULL;
    }
    Header *h = malloc(charge);
    if (!h) {
        reject(b, charge, CFD_MEMORY_SYSTEM_FAILURE);
        return NULL;
    }
    h->allocation.owner = b;
    h->allocation.bytes = charge;
    if (b) {
        b->successful_allocations++;
        b->live_bytes += charge;
        if (b->live_bytes > b->peak_bytes)
            b->peak_bytes = b->live_bytes;
    }
    return h + 1;
}
void *cfd_memory_malloc(size_t bytes) { return allocate(active, bytes); }
void *cfd_memory_calloc(size_t count, size_t bytes) {
    if (bytes && count > SIZE_MAX / bytes) {
        reject(active, SIZE_MAX, CFD_MEMORY_SIZE_OVERFLOW);
        return NULL;
    }
    size_t total = count * bytes;
    void *p = allocate(active, total);
    if (p)
        memset(p, 0, total);
    return p;
}
void cfd_memory_free(void *pointer) {
    if (!pointer)
        return;
    Header *h = (Header *)pointer - 1;
    if (h->allocation.owner)
        h->allocation.owner->live_bytes -= h->allocation.bytes;
    free(h);
}
void *cfd_memory_realloc(void *pointer, size_t bytes) {
    if (!pointer)
        return allocate(active, bytes);
    if (!bytes) {
        cfd_memory_free(pointer);
        return NULL;
    }
    Header *h = (Header *)pointer - 1;
    /* Allocate before freeing so admission accounts for the actual resize
     * peak, and a failed resize leaves the previous block valid. */
    void *p = allocate(h->allocation.owner, bytes);
    if (!p)
        return NULL;
    size_t old = h->allocation.bytes - sizeof(Header);
    memcpy(p, pointer, old < bytes ? old : bytes);
    cfd_memory_free(pointer);
    return p;
}
