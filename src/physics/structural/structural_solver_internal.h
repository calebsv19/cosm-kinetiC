#ifndef STRUCTURAL_SOLVER_INTERNAL_H
#define STRUCTURAL_SOLVER_INTERNAL_H

#include <stdbool.h>

#include "physics/structural/structural_scene.h"

#define MAX_DOF (STRUCT_MAX_NODES * 3)
#define MAX_ENTRIES (STRUCT_MAX_EDGES * 36)

typedef struct SparseTriplet {
    int row;
    int col;
    float value;
} SparseTriplet;

typedef struct SparseMatrix {
    int n;
    int nnz;
    int row_offsets[MAX_DOF + 1];
    int col_indices[MAX_ENTRIES];
    float values[MAX_ENTRIES];
} SparseMatrix;

void sparse_matrix_init(SparseMatrix *mat, int n);
bool sparse_matrix_from_triplets(SparseMatrix *mat,
                                 const SparseTriplet *trips,
                                 int trip_count,
                                 int n);
void sparse_matrix_mul(const SparseMatrix *mat, const float *x, float *out);
bool solve_cg(const SparseMatrix *mat,
              const float *b,
              float *x,
              int n,
              int max_iter,
              float tol,
              int *out_iter,
              float *out_residual);

#endif
