/* Approximate float32 preconditioner; float64 physical operator remains separate. */
#include <Accelerate/Accelerate.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    int count;
    SparseMatrix_Float matrix;
    SparseOpaqueSymbolicFactorization symbolic;
    SparseOpaqueFactorization_Float factor;
    void *factor_storage;
    int numeric_created;
    size_t numeric_workspace_bytes;
} cfd_workspace_factor;

void *cfd_reference_factor_create_symbolic(int count, long *starts, int *rows,
                                         float *values, int ordering, int block_size, int *status) {
    if (!status) return NULL;
    *status = -100;
    if (count < 1 || !starts || !rows || !values || (ordering != 2 && ordering != 3) || (block_size != 1 && block_size != 3) || count > INT32_MAX / block_size) return NULL;
    cfd_workspace_factor *handle = calloc(1, sizeof(*handle));
    if (!handle) return NULL;
    handle->count = count * block_size;
    handle->matrix = (SparseMatrix_Float){
        .structure = {.rowCount = count, .columnCount = count,
            .columnStarts = starts, .rowIndices = rows,
            .attributes = {.kind = SparseSymmetric, .triangle = SparseLowerTriangle},
            .blockSize = (uint8_t)block_size}, .data = values
    };
    SparseSymbolicFactorOptions options = {.orderMethod = ordering, .malloc = malloc, .free = free};
    handle->symbolic = SparseFactor(SparseFactorizationCholesky, handle->matrix.structure, options);
    *status = handle->symbolic.status;
    if (*status != SparseStatusOK) {
        SparseCleanup(handle->symbolic);
        free(handle);
        return NULL;
    }
    handle->numeric_workspace_bytes = handle->symbolic.workspaceSize_Float;
    return handle;
}

int cfd_reference_factor_numeric(void *opaque) {
    cfd_workspace_factor *handle = opaque;
    if (!handle || handle->numeric_created || handle->factor_storage) return -100;
    handle->factor_storage = malloc(handle->symbolic.factorSize_Float);
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
    return handle ? handle->symbolic.factorSize_Float : 0;
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
    float *input = malloc((size_t)handle->count * sizeof(float));
    float *output = malloc((size_t)handle->count * sizeof(float));
    if (!input || !output) { free(input); free(output); return -200; }
    for (int i = 0; i < handle->count; ++i) {
        input[i] = (float)rhs[i];
        if (!isfinite(input[i]) || (rhs[i] != 0.0 && input[i] == 0.0f)) {
            free(input); free(output); return -103;
        }
    }
    DenseVector_Float b = {.count = handle->count, .data = input};
    DenseVector_Float x = {.count = handle->count, .data = output};
    SparseSolve(handle->factor, b, x);
    int status = 0;
    for (int i = 0; i < handle->count; ++i) {
        solution[i] = (double)output[i];
        if (!isfinite(solution[i])) status = -102;
    }
    free(input);
    free(output);
    if (status) return status;
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

/* Upper node-pair row-major blocks act in original component-major coordinates. */
int cfd_reference_vector_action(int nodes, const long *starts, const int *rows,
                                const double *values, const double *x, double *y) {
    if (nodes < 1 || !starts || !rows || !values || !x || !y) return -100;
    for (int i = 0; i < 3 * nodes; ++i) if (!isfinite(x[i])) return -101;
    for (int i = 0; i < nodes; ++i) {
        for (long k = starts[i]; k < starts[i + 1]; ++k) {
            int j = rows[k];
            const double *block = values + 9 * k;
            for (int a = 0; a < 3; ++a) {
                for (int b = 0; b < 3; ++b) {
                    double value = block[3 * a + b];
                    y[a * nodes + i] += value * x[b * nodes + j];
                    if (i != j) y[b * nodes + j] += value * x[a * nodes + i];
                }
            }
        }
    }
    for (int i = 0; i < 3 * nodes; ++i) if (!isfinite(y[i])) return -102;
    return 0;
}

/* Exact physical coefficients restored locally from shared PC predictor bits. */
#include <string.h>
#include <float.h>
_Static_assert(sizeof(double) == sizeof(uint64_t), "64-bit double required");
_Static_assert(sizeof(float) == sizeof(uint32_t), "32-bit float required");
_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && FLT_MANT_DIG == 24,
               "IEEE binary64/binary32 required");
int cfd_reference_encoded_vector_action(int nodes, const long *starts, const int *rows,
                                       const float *predictor, const int32_t *correction,
                                       const double *x, double *y) {
    if (nodes < 1 || nodes > INT32_MAX / 3 || !starts || !rows || !predictor || !correction || !x || !y) return -100;
    for (int i = 0; i < 3 * nodes; ++i) if (!isfinite(x[i])) return -101;
    for (int i = 0; i < nodes; ++i) {
        for (long k = starts[i]; k < starts[i + 1]; ++k) {
            int j = rows[k];
            for (int a = 0; a < 3; ++a) {
                for (int b = 0; b < 3; ++b) {
                    long pos = 9 * k + 3 * a + b;
                    double value = (double)predictor[pos];
                    uint64_t bits;
                    memcpy(&bits, &value, sizeof(bits));
                    bits += (uint64_t)(int64_t)correction[pos];
                    memcpy(&value, &bits, sizeof(value));
                    y[a * nodes + i] += value * x[b * nodes + j];
                    if (i != j) y[b * nodes + j] += value * x[a * nodes + i];
                }
            }
        }
    }
    for (int i = 0; i < 3 * nodes; ++i) if (!isfinite(y[i])) return -102;
    return 0;
}

#include <limits.h>

/* Fixed-pattern block IC0: only the approximate inverse is changed. */
typedef struct {
    int nodes;
    long blocks;
    const long *starts;
    const int *rows;
    double *lower;
    long shifted_pivots; long fill_pairs; double fill_sum, fill_max;
    double shift_sum, shift_max, min_pivot;
} cfd_ic0_factor;

static int ic0_chol3(const double *a, double *l) {
    for (int k = 0; k < 9; ++k) l[k] = 0.0;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j <= i; ++j) {
            double s = 0.5 * (a[3*i+j] + a[3*j+i]);
            for (int k = 0; k < j; ++k) s -= l[3*i+k] * l[3*j+k];
            if (!isfinite(s)) return 0;
            if (i == j) {
                if (s <= 0.0) return 0;
                l[3*i+j] = sqrt(s);
            } else l[3*i+j] = s / l[3*j+j];
        }
    }
    return 1;
}

static long ic0_find(const cfd_ic0_factor *f, int column, int row) {
    long a = f->starts[column], b = f->starts[column+1];
    while (a < b) {
        long mid = a + (b-a)/2;
        if (f->rows[mid] < row) a = mid+1; else b = mid;
    }
    return a < f->starts[column+1] && f->rows[a] == row ? a : -1;
}

void cfd_reference_ic0_destroy(void *opaque) {
    cfd_ic0_factor *f = opaque;
    if (!f) return;
    free(f->lower); free(f);
}

void *cfd_reference_ic0_create(int nodes, long blocks, const long *starts,
                              const int *rows, const float *values, int *status) {
    if (!status) return NULL;
    *status = -100;
    if (nodes < 1 || nodes > INT32_MAX/3 || blocks < nodes ||
        (uint64_t)blocks > SIZE_MAX/(9*sizeof(double)) || !starts || !rows || !values ||
        starts[0] != 0 || starts[nodes] != blocks) return NULL;
    for (int i = 0; i < nodes; ++i) {
        if (starts[i] < 0 || starts[i+1] <= starts[i] || starts[i+1] > blocks || rows[starts[i]] != i) return NULL;
        for (long k = starts[i]; k < starts[i+1]; ++k) {
            if (rows[k] < i || rows[k] >= nodes || (k > starts[i] && rows[k] <= rows[k-1])) return NULL;
            for (int a = 0; a < 9; ++a) if (!isfinite(values[9*k+a])) return NULL;
        }
    }
    cfd_ic0_factor *f = calloc(1,sizeof(*f));
    if (!f) { *status=-200; return NULL; }
    f->nodes=nodes; f->blocks=blocks; f->starts=starts; f->rows=rows; f->min_pivot=INFINITY;
    f->lower=malloc((size_t)blocks*9*sizeof(double));
    if (!f->lower) { cfd_reference_ic0_destroy(f); *status=-200; return NULL; }
    for (long k = 0; k < blocks; ++k)
        for (int a=0; a<3; ++a) for (int b=0; b<3; ++b) f->lower[9*k+3*a+b]=(double)values[9*k+3*b+a];
    for (int col=0; col<nodes; ++col) {
        long first=starts[col], end=starts[col+1];
        double d[9], l[9];
        for (int k=0; k<9; ++k) d[k]=f->lower[9*first+k];
        if (!ic0_chol3(d,l)) {
            double magnitude=0.0, bound=INFINITY;
            for (int a=0; a<3; ++a) {
                double rowbound=d[3*a+a];
                for (int b=0; b<3; ++b) {
                    magnitude=fmax(magnitude,fabs(d[3*a+b]));
                    if (a!=b) rowbound-=fabs(d[3*a+b]);
                }
                bound=fmin(bound,rowbound);
            }
            double shift=fmax(0.0,-bound)+0.05*magnitude;
            if (!isfinite(shift) || shift<=0.0) { *status=-301; cfd_reference_ic0_destroy(f); return NULL; }
            for (int a=0; a<3; ++a) d[3*a+a]+=shift;
            if (!ic0_chol3(d,l)) { *status=-302; cfd_reference_ic0_destroy(f); return NULL; }
            ++f->shifted_pivots; f->shift_sum+=shift; f->shift_max=fmax(f->shift_max,shift);
        }
        for (int a=0; a<3; ++a) f->min_pivot=fmin(f->min_pivot,l[3*a+a]);
        for (int k=0; k<9; ++k) f->lower[9*first+k]=l[k];
        for (long k=first+1; k<end; ++k) {
            double *v=f->lower+9*k;
            for (int a=0; a<3; ++a) for (int b=0; b<3; ++b) {
                double s=v[3*a+b];
                for (int c=0; c<b; ++c) s-=v[3*a+c]*l[3*b+c];
                v[3*a+b]=s/l[3*b+b];
                if (!isfinite(v[3*a+b])) { *status=-303; cfd_reference_ic0_destroy(f); return NULL; }
            }
        }
        for (long ki=first+1; ki<end; ++ki) {
            int i=rows[ki]; const double *li=f->lower+9*ki;
            for (long kj=ki; kj<end; ++kj) {
                int j=rows[kj]; long pos=ic0_find(f,i,j);
                const double *lj=f->lower+9*kj; double update[9], norm=0.0;
                for (int a=0; a<3; ++a) for (int b=0; b<3; ++b) {
                    double s=0.0;
                    for (int c=0; c<3; ++c) s+=lj[3*a+c]*li[3*b+c];
                    update[3*a+b]=s; norm=hypot(norm,s);
                }
                if (!isfinite(norm)) { *status=-305; cfd_reference_ic0_destroy(f); return NULL; }
                if (pos<0) {
                    /* The omitted symmetric pair plus norm-I diagonals is PSD. */
                    double *di=f->lower+9*starts[i], *dj=f->lower+9*starts[j];
                    for (int a=0; a<3; ++a) { di[3*a+a]+=norm; dj[3*a+a]+=norm; }
                    ++f->fill_pairs; f->fill_sum+=2.0*norm; f->fill_max=fmax(f->fill_max,norm);
                } else {
                    double *v=f->lower+9*pos;
                    for (int a=0; a<9; ++a) v[a]-=update[a];
                }
            }
        }
    }
    if (!isfinite(f->shift_sum) || !isfinite(f->min_pivot)) { *status=-304; cfd_reference_ic0_destroy(f); return NULL; }
    *status=0; return f;
}

int cfd_reference_ic0_solve(void *opaque, const double *rhs, double *out, double *work) {
    const cfd_ic0_factor *f=opaque;
    if (!f || !rhs || !out || !work) return -100;
    int n=f->nodes;
    for (int i=0; i<n; ++i) for (int a=0; a<3; ++a) {
        double v=rhs[a*n+i]; if (!isfinite(v)) return -101; work[3*i+a]=v;
    }
    for (int i=0; i<n; ++i) {
        long first=f->starts[i]; const double *l=f->lower+9*first; double *x=work+3*i;
        for (int a=0; a<3; ++a) { for (int b=0; b<a; ++b) x[a]-=l[3*a+b]*x[b]; x[a]/=l[3*a+a]; }
        for (long k=first+1; k<f->starts[i+1]; ++k) {
            const double *v=f->lower+9*k; double *y=work+3*f->rows[k];
            for (int a=0; a<3; ++a) for (int b=0; b<3; ++b) y[a]-=v[3*a+b]*x[b];
        }
    }
    for (int i=n-1; i>=0; --i) {
        long first=f->starts[i]; const double *l=f->lower+9*first; double *x=work+3*i;
        for (long k=first+1; k<f->starts[i+1]; ++k) {
            const double *v=f->lower+9*k; const double *y=work+3*f->rows[k];
            for (int a=0; a<3; ++a) for (int b=0; b<3; ++b) x[a]-=v[3*b+a]*y[b];
        }
        for (int a=2; a>=0; --a) { for (int b=a+1; b<3; ++b) x[a]-=l[3*b+a]*x[b]; x[a]/=l[3*a+a]; }
    }
    for (int i=0; i<n; ++i) for (int a=0; a<3; ++a) {
        out[a*n+i]=work[3*i+a]; if (!isfinite(out[a*n+i])) return -102;
    }
    return 0;
}

size_t cfd_reference_ic0_storage(void *opaque) {
    cfd_ic0_factor *f=opaque; return f ? sizeof(*f)+(size_t)f->blocks*9*sizeof(double) : 0;
}
long cfd_reference_ic0_shifted(void *opaque) { cfd_ic0_factor *f=opaque; return f ? f->shifted_pivots : 0; }
double cfd_reference_ic0_shift_sum(void *opaque) { cfd_ic0_factor *f=opaque; return f ? f->shift_sum : 0.0; }
double cfd_reference_ic0_shift_max(void *opaque) { cfd_ic0_factor *f=opaque; return f ? f->shift_max : 0.0; }
double cfd_reference_ic0_min_pivot(void *opaque) { cfd_ic0_factor *f=opaque; return f ? f->min_pivot : 0.0; }
const double *cfd_reference_ic0_values(void *opaque) { cfd_ic0_factor *f=opaque; return f ? f->lower : NULL; }

long cfd_reference_ic0_fill_pairs(void *opaque) { cfd_ic0_factor *f=opaque; return f ? f->fill_pairs : 0; }
double cfd_reference_ic0_fill_sum(void *opaque) { cfd_ic0_factor *f=opaque; return f ? f->fill_sum : 0.0; }
double cfd_reference_ic0_fill_max(void *opaque) { cfd_ic0_factor *f=opaque; return f ? f->fill_max : 0.0; }

void *cfd_fill1_factor_create(int nodes, long blocks, const long *starts,
                              const int *rows, const long *original_starts, const int *original_rows,
                              const float *values, const int *inverse_permutation, int *status) {
    if (!status) return NULL;
    *status = -100;
    if (nodes < 1 || nodes > INT32_MAX/3 || blocks < nodes ||
        (uint64_t)blocks > SIZE_MAX/(9*sizeof(double)) || !starts || !rows || !values || !original_starts || !original_rows || !inverse_permutation ||
        starts[0] != 0 || starts[nodes] != blocks) return NULL;
    for (int i = 0; i < nodes; ++i) {
        if (starts[i] < 0 || starts[i+1] <= starts[i] || starts[i+1] > blocks || rows[starts[i]] != i) return NULL;
        for (long k = starts[i]; k < starts[i+1]; ++k) {
            if (rows[k] < i || rows[k] >= nodes || (k > starts[i] && rows[k] <= rows[k-1])) return NULL;

        }
    }
    if (original_starts[0]!=0 || original_starts[nodes]<nodes) return NULL;
    for (int i=0; i<nodes; ++i) {
        if (inverse_permutation[i]<0 || inverse_permutation[i]>=nodes || original_starts[i]<0 || original_starts[i+1]<=original_starts[i] || original_starts[i+1]>original_starts[nodes] || original_rows[original_starts[i]]!=i) return NULL;
        for (long k=original_starts[i]; k<original_starts[i+1]; ++k) {
            if (original_rows[k]<i || original_rows[k]>=nodes) return NULL;
            for (int a=0; a<9; ++a) if (!isfinite(values[9*k+a])) return NULL;
        }
    }
    cfd_ic0_factor *f = calloc(1,sizeof(*f));
    if (!f) { *status=-200; return NULL; }
    f->nodes=nodes; f->blocks=blocks; f->starts=starts; f->rows=rows; f->min_pivot=INFINITY;
    f->lower=malloc((size_t)blocks*9*sizeof(double));
    if (!f->lower) { cfd_reference_ic0_destroy(f); *status=-200; return NULL; }
    for (long k=0; k<blocks*9; ++k) f->lower[k]=0.0;
    for (int oldcol=0; oldcol<nodes; ++oldcol) {
        int i=inverse_permutation[oldcol];
        for (long k=original_starts[oldcol]; k<original_starts[oldcol+1]; ++k) {
            int j=inverse_permutation[original_rows[k]];
            int col=i<j?i:j, row=i<j?j:i; long pos=ic0_find(f,col,row);
            if (pos<0) { *status=-306; cfd_reference_ic0_destroy(f); return NULL; }
            for (int a=0; a<3; ++a) for (int b=0; b<3; ++b)
                f->lower[9*pos+3*a+b]=(double)values[9*k+(i<=j?3*b+a:3*a+b)];
        }
    }
    for (int col=0; col<nodes; ++col) {
        long first=starts[col], end=starts[col+1];
        double d[9], l[9];
        for (int k=0; k<9; ++k) d[k]=f->lower[9*first+k];
        if (!ic0_chol3(d,l)) {
            double magnitude=0.0, bound=INFINITY;
            for (int a=0; a<3; ++a) {
                double rowbound=d[3*a+a];
                for (int b=0; b<3; ++b) {
                    magnitude=fmax(magnitude,fabs(d[3*a+b]));
                    if (a!=b) rowbound-=fabs(d[3*a+b]);
                }
                bound=fmin(bound,rowbound);
            }
            double shift=fmax(0.0,-bound)+0.05*magnitude;
            if (!isfinite(shift) || shift<=0.0) { *status=-301; cfd_reference_ic0_destroy(f); return NULL; }
            for (int a=0; a<3; ++a) d[3*a+a]+=shift;
            if (!ic0_chol3(d,l)) { *status=-302; cfd_reference_ic0_destroy(f); return NULL; }
            ++f->shifted_pivots; f->shift_sum+=shift; f->shift_max=fmax(f->shift_max,shift);
        }
        for (int a=0; a<3; ++a) f->min_pivot=fmin(f->min_pivot,l[3*a+a]);
        for (int k=0; k<9; ++k) f->lower[9*first+k]=l[k];
        for (long k=first+1; k<end; ++k) {
            double *v=f->lower+9*k;
            for (int a=0; a<3; ++a) for (int b=0; b<3; ++b) {
                double s=v[3*a+b];
                for (int c=0; c<b; ++c) s-=v[3*a+c]*l[3*b+c];
                v[3*a+b]=s/l[3*b+b];
                if (!isfinite(v[3*a+b])) { *status=-303; cfd_reference_ic0_destroy(f); return NULL; }
            }
        }
        for (long ki=first+1; ki<end; ++ki) {
            int i=rows[ki]; const double *li=f->lower+9*ki;
            for (long kj=ki; kj<end; ++kj) {
                int j=rows[kj]; long pos=ic0_find(f,i,j);
                const double *lj=f->lower+9*kj; double update[9], norm=0.0;
                for (int a=0; a<3; ++a) for (int b=0; b<3; ++b) {
                    double s=0.0;
                    for (int c=0; c<3; ++c) s+=lj[3*a+c]*li[3*b+c];
                    update[3*a+b]=s; norm=hypot(norm,s);
                }
                if (!isfinite(norm)) { *status=-305; cfd_reference_ic0_destroy(f); return NULL; }
                if (pos<0) {
                    /* The omitted symmetric pair plus norm-I diagonals is PSD. */
                    double *di=f->lower+9*starts[i], *dj=f->lower+9*starts[j];
                    for (int a=0; a<3; ++a) { di[3*a+a]+=norm; dj[3*a+a]+=norm; }
                    ++f->fill_pairs; f->fill_sum+=2.0*norm; f->fill_max=fmax(f->fill_max,norm);
                } else {
                    double *v=f->lower+9*pos;
                    for (int a=0; a<9; ++a) v[a]-=update[a];
                }
            }
        }
    }
    if (!isfinite(f->shift_sum) || !isfinite(f->min_pivot)) { *status=-304; cfd_reference_ic0_destroy(f); return NULL; }
    *status=0; return f;
}

/* Original-edge level-one candidates; fixed per-column fill quota. */
typedef struct {
    int nodes; long original_blocks, blocks, complete_fill, kept_fill;
    size_t workspace_bound; long *starts; int *rows;
} cfd_fill1_pattern;

static int fill1_int_compare(const void *a, const void *b) {
    int aa=*(const int *)a, bb=*(const int *)b; return (aa>bb)-(aa<bb);
}

int cfd_fill1_workspace(int nodes, const long *starts, const int *rows, const int *inverse,
                       long *counts, size_t *required, long *pairs) {
    if (nodes<1 || !starts || !rows || !inverse || !counts || !required || !pairs || starts[0]!=0 || starts[nodes]<nodes) return -400;
    for (int i=0; i<nodes; ++i) counts[i]=0;
    for (int i=0; i<nodes; ++i) {
        if (inverse[i]<0 || inverse[i]>=nodes || ++counts[inverse[i]]!=1) return -401;
    }
    for (int i=0; i<nodes; ++i) counts[i]=0;
    for (int i=0; i<nodes; ++i) {
        if (starts[i]<0 || starts[i+1]<=starts[i] || starts[i+1]>starts[nodes] || rows[starts[i]]!=i) return -402;
        for (long k=starts[i]; k<starts[i+1]; ++k) {
            int j=rows[k]; if (j<i || j>=nodes || (k>starts[i] && j<=rows[k-1])) return -402;
            int a=inverse[i], b=inverse[j]; ++counts[a<b?a:b];
        }
    }
    uint64_t total=0;
    for (int i=0; i<nodes; ++i) {
        uint64_t d=(uint64_t)counts[i]-1; total+=d*(d-1)/2;
    }
    uint64_t bytes=4*total+24*(uint64_t)starts[nodes]+64*((uint64_t)nodes+1)+4096;
    if (bytes>SIZE_MAX || total>LONG_MAX) return -403;
    *required=(size_t)bytes; *pairs=(long)total; return 0;
}

void cfd_fill1_pattern_destroy(void *opaque) {
    cfd_fill1_pattern *p=opaque; if (!p) return; free(p->starts); free(p->rows); free(p);
}

void *cfd_fill1_pattern_create(int nodes, const long *starts, const int *rows, const int *inverse,
                             size_t max_work, int *status) {
    if (!status) return NULL; *status=-400;
    if (nodes<1) return NULL;
    long *counts=malloc((size_t)nodes*sizeof(long)), pairs=0; size_t need=0;
    if (!counts) { *status=-200; return NULL; }
    int check=cfd_fill1_workspace(nodes,starts,rows,inverse,counts,&need,&pairs);
    if (check || need>max_work) { free(counts); *status=check?check:-410; return NULL; }
    long m=starts[nodes];
    long *original_starts=malloc(((size_t)nodes+1)*sizeof(long));
    int *original_rows=malloc((size_t)m*sizeof(int));
    long *cursor=malloc((size_t)nodes*sizeof(long));
    long *candidate_starts=malloc(((size_t)nodes+1)*sizeof(long));
    int *candidate_rows=malloc((size_t)(m+pairs)*sizeof(int));
    cfd_fill1_pattern *p=calloc(1,sizeof(*p));
    if (!original_starts || !original_rows || !cursor || !candidate_starts || !candidate_rows || !p) { *status=-200; goto cleanup; }
    p->nodes=nodes; p->original_blocks=m; p->workspace_bound=need;
    p->starts=malloc(((size_t)nodes+1)*sizeof(long));p->rows=malloc((size_t)(2*m-nodes)*sizeof(int));
    if (!p->starts || !p->rows) { *status=-200; goto cleanup; }
    original_starts[0]=0;
    for (int i=0; i<nodes; ++i) original_starts[i+1]=original_starts[i]+counts[i];
    for (int i=0; i<nodes; ++i) cursor[i]=original_starts[i];
    for (int i=0; i<nodes; ++i) for (long k=starts[i]; k<starts[i+1]; ++k) {
        int a=inverse[i], b=inverse[rows[k]], col=a<b?a:b, row=a<b?b:a;
        original_rows[cursor[col]++]=row;
    }
    for (int i=0; i<nodes; ++i) qsort(original_rows+original_starts[i],(size_t)counts[i],sizeof(int),fill1_int_compare);
    for (int col=0; col<nodes; ++col)
        for (long ki=original_starts[col]+1; ki<original_starts[col+1]; ++ki)
            counts[original_rows[ki]]+=original_starts[col+1]-ki-1;
    candidate_starts[0]=0;
    for (int i=0; i<nodes; ++i) candidate_starts[i+1]=candidate_starts[i]+counts[i];
    for (int i=0; i<nodes; ++i) {
        long len=original_starts[i+1]-original_starts[i];
        memcpy(candidate_rows+candidate_starts[i],original_rows+original_starts[i],(size_t)len*sizeof(int));
        cursor[i]=candidate_starts[i]+len;
    }
    for (int col=0; col<nodes; ++col)
        for (long ki=original_starts[col]+1; ki<original_starts[col+1]; ++ki) {
            int i=original_rows[ki];
            for (long kj=ki+1; kj<original_starts[col+1]; ++kj) candidate_rows[cursor[i]++]=original_rows[kj];
        }
    p->starts[0]=0;
    for (int i=0; i<nodes; ++i) {
        qsort(candidate_rows+candidate_starts[i],(size_t)counts[i],sizeof(int),fill1_int_compare);
        long orig=original_starts[i], orig_end=original_starts[i+1], quota=orig_end-orig-1, added=0;
        int previous=-1;
        for (long k=candidate_starts[i]; k<candidate_starts[i+1]; ++k) {
            int row=candidate_rows[k]; if (row==previous) continue; previous=row;
            while (orig<orig_end && original_rows[orig]<row) ++orig;
            int physical=orig<orig_end && original_rows[orig]==row;
            if (!physical) ++p->complete_fill;
            if (physical || added<quota) {
                p->rows[p->blocks++]=row;
                if (!physical) { ++added; ++p->kept_fill; }
            }
        }
        p->starts[i+1]=p->blocks;
    }
    *status=0;
cleanup:
    free(counts);free(original_starts);free(original_rows);free(cursor);free(candidate_starts);free(candidate_rows);
    if (*status) { cfd_fill1_pattern_destroy(p); return NULL; }
    return p;
}
const long *cfd_fill1_pattern_starts(void *o) { cfd_fill1_pattern *p=o; return p?p->starts:NULL; }
const int *cfd_fill1_pattern_rows(void *o) { cfd_fill1_pattern *p=o; return p?p->rows:NULL; }
long cfd_fill1_pattern_blocks(void *o) { cfd_fill1_pattern *p=o; return p?p->blocks:0; }
long cfd_fill1_pattern_complete_fill(void *o) { cfd_fill1_pattern *p=o; return p?p->complete_fill:0; }
long cfd_fill1_pattern_kept_fill(void *o) { cfd_fill1_pattern *p=o; return p?p->kept_fill:0; }
size_t cfd_fill1_pattern_workspace(void *o) { cfd_fill1_pattern *p=o; return p?p->workspace_bound:0; }

/* Same cap-eight recurrence and exact existing physical action, one FFI call. */
static int cg8_ranges_disjoint(const void *a, size_t na, const void *b, size_t nb) {
    uintptr_t aa=(uintptr_t)a,bb=(uintptr_t)b;
    if (aa>UINTPTR_MAX-na || bb>UINTPTR_MAX-nb) return 0;
    return aa+na<=bb || bb+nb<=aa;
}
static int cg8_permuted_solve(void *factor, int nodes, const int *perm, const double *r,
                              double *z, double *fr, double *fo, double *fw) {
    for (int a=0; a<3; ++a) for (int i=0; i<nodes; ++i) fr[a*nodes+i]=r[a*nodes+perm[i]];
    int status=cfd_reference_ic0_solve(factor,fr,fo,fw);
    if (status) return status;
    for (int a=0; a<3; ++a) for (int i=0; i<nodes; ++i) z[a*nodes+perm[i]]=fo[a*nodes+i];
    return 0;
}
int cfd_reference_inner_cg8(void *opaque, int nodes, const long *starts, const int *rows,
                            const double *values, const float *predictor, const int32_t *corrections,
                            const int *perm, const double *rhs, double *out, double *work,
                            size_t work_count, int encoded, int *steps) {
    cfd_ic0_factor *f=opaque;
    if (!f || nodes<1 || f->nodes!=nodes || !starts || !rows || !perm || !rhs || !out || !work || !steps ||
        (encoded!=0 && encoded!=1) || (encoded && (!predictor || !corrections)) || (!encoded && !values)) return -500;
    size_t count=(size_t)3*nodes,bytes=count*sizeof(double);
    if (work_count<7*count || !cg8_ranges_disjoint(rhs,bytes,out,bytes) ||
        !cg8_ranges_disjoint(rhs,bytes,work,7*bytes) || !cg8_ranges_disjoint(out,bytes,work,7*bytes)) return -501;
    if (starts[nodes]<nodes || (uint64_t)starts[nodes]>SIZE_MAX/(9*sizeof(double))) return -500;
    size_t blocks=(size_t)starts[nodes];
    const void *inputs[8]={starts,rows,perm,f->lower,f->starts,f->rows,encoded?(const void *)predictor:(const void *)values,corrections};
    size_t sizes[8]={((size_t)nodes+1)*sizeof(long),blocks*sizeof(int),(size_t)nodes*sizeof(int),(size_t)f->blocks*9*sizeof(double),((size_t)nodes+1)*sizeof(long),(size_t)f->blocks*sizeof(int),blocks*9*(encoded?sizeof(float):sizeof(double)),encoded?blocks*9*sizeof(int32_t):0};
    for (int i=0; i<8; ++i) if (sizes[i] && (!cg8_ranges_disjoint(out,bytes,inputs[i],sizes[i]) || !cg8_ranges_disjoint(work,7*bytes,inputs[i],sizes[i]))) return -501;
    double *r=work,*z=r+count,*p=z+count,*Ap=p+count,*fr=Ap+count,*fo=fr+count,*fw=fo+count;
    *steps=0;
    for (int i=0; i<nodes; ++i) r[i]=0.0;
    for (int i=0; i<nodes; ++i) {
        if (perm[i]<0 || perm[i]>=nodes || r[perm[i]]!=0.0) return -502;
        r[perm[i]]=1.0;
    }
    int nonzero=0;
    for (size_t i=0; i<count; ++i) {
        if (!isfinite(rhs[i])) return -503;
        r[i]=rhs[i];out[i]=0.0;nonzero|=rhs[i]!=0.0;
    }
    if (!nonzero) return 0;
    if (cg8_permuted_solve(f,nodes,perm,r,z,fr,fo,fw)) return -504;
    for (size_t i=0; i<count; ++i) p[i]=z[i];
    double rz=cblas_ddot((int)count,r,1,z,1);
    if (!isfinite(rz) || rz<=0.0) return -505;
    for (int iteration=0; iteration<8; ++iteration) {
        for (size_t i=0; i<count; ++i) Ap[i]=0.0;
        int status=encoded?cfd_reference_encoded_vector_action(nodes,starts,rows,predictor,corrections,p,Ap):
                           cfd_reference_vector_action(nodes,starts,rows,values,p,Ap);
        if (status) return -506;
        double curvature=cblas_ddot((int)count,p,1,Ap,1);
        if (!isfinite(curvature) || curvature<=0.0) return -507;
        double alpha=rz/curvature;nonzero=0;
        for (size_t i=0; i<count; ++i) {
            out[i]+=alpha*p[i];r[i]-=alpha*Ap[i];nonzero|=r[i]!=0.0;
            if (!isfinite(out[i]) || !isfinite(r[i])) return -508;
        }
        *steps=iteration+1;
        if (iteration==7 || !nonzero) break;
        if (cg8_permuted_solve(f,nodes,perm,r,z,fr,fo,fw)) return -504;
        double next=cblas_ddot((int)count,r,1,z,1);
        if (!isfinite(next) || next<=0.0) return -505;
        double beta=next/rz;
        for (size_t i=0; i<count; ++i) p[i]=z[i]+beta*p[i];
        rz=next;
    }
    return 0;
}
