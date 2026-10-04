#ifndef PHYSICS_SIM_CFD_SPARSE_MG_H
#define PHYSICS_SIM_CFD_SPARSE_MG_H
#include <stddef.h>
/* Cached symmetric Galerkin V-cycle for a positive-definite CSR operator.
 * The constructor copies the matrix. Coordinates select geometric aggregates;
 * restriction is P^T, prolongation is constant injection. Rows must be sorted
 * with unique columns. No apply allocation; b and x must not alias. */
typedef struct CfdSparseMg CfdSparseMg;
CfdSparseMg *cfd_sparse_mg_create(int n, const int *row, const int *col, const double *a,
                                  const double *x, const double *y, double hx, double hy);
/* Z-aware aggregates; the existing 2D constructor keeps its original behavior. */
CfdSparseMg *cfd_sparse_mg_create3d(int n, const int *row, const int *col, const double *a,
                                    const double *x, const double *y, const double *z, double hx,
                                    double hy, double hz);
void cfd_sparse_mg_destroy(CfdSparseMg *s);
void cfd_sparse_mg_apply(CfdSparseMg *s, const double *b, double *x);
size_t cfd_sparse_mg_bytes(const CfdSparseMg *s);
#endif
