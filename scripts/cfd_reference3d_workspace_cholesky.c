/* Optional exact macOS factor with caller-owned numeric storage and scratch. */
#include <Accelerate/Accelerate.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    int count;
    SparseMatrix_Double matrix;
    SparseOpaqueSymbolicFactorization symbolic;
    SparseOpaqueFactorization_Double factor;
    void *factor_storage;
    int numeric_created;
    size_t numeric_workspace_bytes;
} cfd_workspace_factor;

void *cfd_reference_factor_create_symbolic(int count, long *starts, int *rows,
                                         double *values, int ordering, int *status) {
    if (!status) return NULL;
    *status = -100;
    if (count < 1 || !starts || !rows || !values || (ordering != 2 && ordering != 3)) return NULL;
    cfd_workspace_factor *handle = calloc(1, sizeof(*handle));
    if (!handle) return NULL;
    handle->count = count;
    handle->matrix = (SparseMatrix_Double){
        .structure = {.rowCount = count, .columnCount = count,
            .columnStarts = starts, .rowIndices = rows,
            .attributes = {.kind = SparseSymmetric, .triangle = SparseLowerTriangle},
            .blockSize = 1}, .data = values
    };
    SparseSymbolicFactorOptions options = {.orderMethod = ordering, .malloc = malloc, .free = free};
    handle->symbolic = SparseFactor(SparseFactorizationCholesky, handle->matrix.structure, options);
    *status = handle->symbolic.status;
    if (*status != SparseStatusOK) {
        SparseCleanup(handle->symbolic);
        free(handle);
        return NULL;
    }
    handle->numeric_workspace_bytes = handle->symbolic.workspaceSize_Double;
    return handle;
}

int cfd_reference_factor_numeric(void *opaque) {
    cfd_workspace_factor *handle = opaque;
    if (!handle || handle->numeric_created || handle->factor_storage) return -100;
    handle->factor_storage = malloc(handle->symbolic.factorSize_Double);
    void *workspace = malloc(handle->numeric_workspace_bytes ? handle->numeric_workspace_bytes : 16);
    if (!handle->factor_storage || !workspace) {
        free(workspace);
        return -200;
    }
    if ((uintptr_t)handle->factor_storage % 16 || (uintptr_t)workspace % 16) {
        free(workspace);
        return -201;
    }
    SparseNumericFactorOptions options = {.scalingMethod = SparseScalingUser};
    handle->factor = SparseFactor(handle->symbolic, handle->matrix, options,
                                 handle->factor_storage, workspace);
    handle->numeric_created = 1;
    free(workspace);
    return handle->factor.status;
}

size_t cfd_reference_factor_storage(void *opaque) {
    cfd_workspace_factor *handle = opaque;
    return handle ? handle->symbolic.factorSize_Double : 0;
}

size_t cfd_reference_factor_numeric_workspace(void *opaque) {
    cfd_workspace_factor *handle = opaque;
    return handle ? handle->numeric_workspace_bytes : 0;
}

size_t cfd_reference_factor_workspace(void *opaque) {
    cfd_workspace_factor *handle = opaque;
    return handle && handle->numeric_created ? handle->factor.solveWorkspaceRequiredStatic +
        handle->factor.solveWorkspaceRequiredPerRHS : 0;
}

int cfd_reference_factor_user_storage(void *opaque) {
    cfd_workspace_factor *handle = opaque;
    return handle && handle->numeric_created && handle->factor.userFactorStorage;
}

int cfd_reference_factor_solve(void *opaque, const double *rhs, double *solution) {
    cfd_workspace_factor *handle = opaque;
    if (!handle || !handle->numeric_created || handle->factor.status != SparseStatusOK || !rhs || !solution) return -100;
    for (int i = 0; i < handle->count; ++i) if (!isfinite(rhs[i])) return -101;
    DenseVector_Double b = {.count = handle->count, .data = (double *)rhs};
    DenseVector_Double x = {.count = handle->count, .data = solution};
    SparseSolve(handle->factor, b, x);
    for (int i = 0; i < handle->count; ++i) if (!isfinite(solution[i])) return -102;
    return 0;
}

void cfd_reference_factor_destroy(void *opaque) {
    cfd_workspace_factor *handle = opaque;
    if (!handle) return;
    if (handle->numeric_created) SparseCleanup(handle->factor);
    SparseCleanup(handle->symbolic);
    free(handle->factor_storage);
    free(handle);
}
