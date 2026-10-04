/* Optional exact symbolic graph/cost diagnostic; no numerical field is solved. */
#include <Accelerate/Accelerate.h>
#include <limits.h>
#include <stdlib.h>

typedef struct {
    SparseOpaqueSymbolicFactorization symbolic;
} cfd_reference_symbolic;

void *cfd_reference_symbolic_create(int count, long *starts, int *rows,
                                   int ordering, int block_size, int *status) {
    if (!status) return NULL;
    *status = -100;
    if (count < 1 || !starts || !rows ||
        (block_size != 1 && block_size != 3) ||
        count > INT_MAX / block_size || (ordering != 2 && ordering != 3)) return NULL;
    cfd_reference_symbolic *handle = calloc(1, sizeof(*handle));
    if (!handle) return NULL;
    SparseMatrixStructure structure = {
        .rowCount = count, .columnCount = count,
        .columnStarts = starts, .rowIndices = rows,
        .attributes = {.kind = SparseSymmetric, .triangle = SparseLowerTriangle},
        .blockSize = (uint8_t)block_size
    };
    SparseSymbolicFactorOptions options = {
        .orderMethod = ordering, .malloc = malloc, .free = free
    };
    handle->symbolic = SparseFactor(SparseFactorizationCholesky, structure, options);
    *status = handle->symbolic.status;
    if (*status != SparseStatusOK) {
        SparseCleanup(handle->symbolic);
        free(handle);
        return NULL;
    }
    return handle;
}

size_t cfd_reference_symbolic_storage(void *opaque) {
    cfd_reference_symbolic *handle = opaque;
    return handle ? handle->symbolic.factorSize_Double : 0;
}

size_t cfd_reference_symbolic_workspace(void *opaque) {
    cfd_reference_symbolic *handle = opaque;
    return handle ? handle->symbolic.workspaceSize_Double : 0;
}

void cfd_reference_symbolic_destroy(void *opaque) {
    cfd_reference_symbolic *handle = opaque;
    if (!handle) return;
    SparseCleanup(handle->symbolic);
    free(handle);
}
