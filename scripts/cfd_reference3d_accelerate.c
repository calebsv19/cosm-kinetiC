/* Optional macOS reference-factor shim. No PhysicsSim native API is changed. */
#include <Accelerate/Accelerate.h>
#include <math.h>
#include <stdlib.h>

typedef struct {
    int count;
    SparseOpaqueFactorization_Double factor;
} cfd_reference_factor;

void *cfd_reference_factor_create(int count, long *starts, int *rows,
                                 double *values, int ordering, int *status) {
    if (count < 1 || !starts || !rows || !values || !status) return NULL;
    *status = -100;
    cfd_reference_factor *handle = calloc(1, sizeof(*handle));
    if (!handle) return NULL;
    SparseMatrix_Double matrix = {
        .structure = {.rowCount = count, .columnCount = count,
            .columnStarts = starts, .rowIndices = rows,
            .attributes = {.kind = SparseSymmetric, .triangle = SparseLowerTriangle},
            .blockSize = 1},
        .data = values
    };
    handle->count = count;
    SparseSymbolicFactorOptions symbolic = {.orderMethod = ordering,
        .malloc = malloc, .free = free};
    SparseNumericFactorOptions numeric = {.scalingMethod = SparseScalingUser};
    handle->factor = SparseFactor(SparseFactorizationCholesky, matrix, symbolic, numeric);
    *status = handle->factor.status;
    if (*status != SparseStatusOK) {
        SparseCleanup(handle->factor);
        free(handle);
        return NULL;
    }
    return handle;
}

int cfd_reference_factor_solve(void *opaque, const double *rhs, double *solution) {
    cfd_reference_factor *handle = opaque;
    if (!handle || !rhs || !solution) return -100;
    for (int i = 0; i < handle->count; ++i) {
        if (!isfinite(rhs[i])) return -101;
    }
    DenseVector_Double b = {.count = handle->count, .data = (double *)rhs};
    DenseVector_Double x = {.count = handle->count, .data = solution};
    SparseSolve(handle->factor, b, x);
    for (int i = 0; i < handle->count; ++i) {
        if (!isfinite(solution[i])) return -102;
    }
    return 0;
}

size_t cfd_reference_factor_storage(void *opaque) {
    cfd_reference_factor *handle = opaque;
    return handle ? handle->factor.symbolicFactorization.factorSize_Double : 0;
}

size_t cfd_reference_factor_workspace(void *opaque) {
    cfd_reference_factor *handle = opaque;
    return handle ? handle->factor.solveWorkspaceRequiredStatic +
        handle->factor.solveWorkspaceRequiredPerRHS : 0;
}

void cfd_reference_factor_destroy(void *opaque) {
    cfd_reference_factor *handle = opaque;
    if (!handle) return;
    SparseCleanup(handle->factor);
    free(handle);
}
