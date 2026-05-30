#include "physics/structural/structural_solver_internal.h"

#include <math.h>
#include <string.h>

static float vec_dot(const float *a, const float *b, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

static void vec_axpy(float *y, const float *x, float a, int n) {
    for (int i = 0; i < n; ++i) {
        y[i] += a * x[i];
    }
}

static void vec_copy(float *dst, const float *src, int n) {
    memcpy(dst, src, (size_t)n * sizeof(float));
}

void sparse_matrix_init(SparseMatrix *mat, int n) {
    if (!mat) return;
    mat->n = n;
    mat->nnz = 0;
    memset(mat->row_offsets, 0, sizeof(mat->row_offsets));
}

bool sparse_matrix_from_triplets(SparseMatrix *mat,
                                 const SparseTriplet *trips,
                                 int trip_count,
                                 int n) {
    if (!mat || !trips || n <= 0) return false;
    if (trip_count < 0 || trip_count > MAX_ENTRIES) return false;

    sparse_matrix_init(mat, n);

    int counts[MAX_DOF] = {0};
    for (int i = 0; i < trip_count; ++i) {
        int row = trips[i].row;
        if (row < 0 || row >= n) continue;
        counts[row]++;
    }

    mat->row_offsets[0] = 0;
    for (int i = 0; i < n; ++i) {
        mat->row_offsets[i + 1] = mat->row_offsets[i] + counts[i];
    }
    {
        int total = mat->row_offsets[n];
        if (total > MAX_ENTRIES) return false;
        mat->nnz = total;
    }

    int offsets[MAX_DOF];
    for (int i = 0; i < n; ++i) {
        offsets[i] = mat->row_offsets[i];
    }

    for (int i = 0; i < trip_count; ++i) {
        int row = trips[i].row;
        int col = trips[i].col;
        float val = trips[i].value;
        if (row < 0 || row >= n || col < 0 || col >= n) continue;
        {
            int dst = offsets[row]++;
            if (dst >= MAX_ENTRIES) return false;
            mat->col_indices[dst] = col;
            mat->values[dst] = val;
        }
    }
    return true;
}

void sparse_matrix_mul(const SparseMatrix *mat, const float *x, float *out) {
    if (!mat || !x || !out) return;
    for (int i = 0; i < mat->n; ++i) {
        float sum = 0.0f;
        int start = mat->row_offsets[i];
        int end = mat->row_offsets[i + 1];
        for (int j = start; j < end; ++j) {
            sum += mat->values[j] * x[mat->col_indices[j]];
        }
        out[i] = sum;
    }
}

bool solve_cg(const SparseMatrix *mat,
              const float *b,
              float *x,
              int n,
              int max_iter,
              float tol,
              int *out_iter,
              float *out_residual) {
    if (!mat || !b || !x || n <= 0) return false;

    float r[MAX_DOF] = {0};
    float p[MAX_DOF] = {0};
    float ap[MAX_DOF] = {0};

    for (int i = 0; i < n; ++i) {
        x[i] = 0.0f;
    }

    sparse_matrix_mul(mat, x, ap);
    for (int i = 0; i < n; ++i) {
        r[i] = b[i] - ap[i];
    }
    vec_copy(p, r, n);

    float rsold = vec_dot(r, r, n);
    if (rsold < tol * tol) {
        if (out_iter) *out_iter = 0;
        if (out_residual) *out_residual = sqrtf(rsold);
        return true;
    }

    for (int iter = 0; iter < max_iter; ++iter) {
        sparse_matrix_mul(mat, p, ap);
        {
            float denom = vec_dot(p, ap, n);
            if (fabsf(denom) < 1e-8f) {
                if (out_iter) *out_iter = iter;
                if (out_residual) *out_residual = sqrtf(rsold);
                return false;
            }
            {
                float alpha = rsold / denom;
                vec_axpy(x, p, alpha, n);
                for (int i = 0; i < n; ++i) {
                    r[i] -= alpha * ap[i];
                }
            }
        }
        {
            float rsnew = vec_dot(r, r, n);
            if (sqrtf(rsnew) < tol) {
                if (out_iter) *out_iter = iter + 1;
                if (out_residual) *out_residual = sqrtf(rsnew);
                return true;
            }
            {
                float beta = rsnew / rsold;
                for (int i = 0; i < n; ++i) {
                    p[i] = r[i] + beta * p[i];
                }
                rsold = rsnew;
            }
        }
    }

    if (out_iter) *out_iter = max_iter;
    if (out_residual) *out_residual = sqrtf(rsold);
    return false;
}
