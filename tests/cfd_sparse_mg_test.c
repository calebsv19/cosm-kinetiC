#include "app/cfd_sparse_mg.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double dot(const double *a, const double *b, int n) {
    double v = 0;
    for (int i = 0; i < n; i++)
        v += a[i] * b[i];
    return v;
}
static void check(int nx, int ny) {
    int n = nx * ny;
    int *row = calloc((size_t)n + 1, sizeof(int)), *col = malloc((size_t)5 * n * sizeof(int));
    double *a = malloc((size_t)5 * n * sizeof(double)), *v = calloc((size_t)9 * n, sizeof(double));
    assert(row && col && a && v);
    double *x = v, *y = x + n, *b = y + n, *c = b + n, *mb = c + n, *mc = mb + n, *sum = mc + n,
           *ms = sum + n, *r = ms + n;
    int nz = 0;
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++) {
            int k = j * nx + i;
            x[k] = i + .5;
            y[k] = j + .5;
            if (j) {
                col[nz] = k - nx;
                a[nz++] = -1;
            }
            if (i) {
                col[nz] = k - 1;
                a[nz++] = -1;
            }
            col[nz] = k;
            a[nz++] = 4;
            if (i + 1 < nx) {
                col[nz] = k + 1;
                a[nz++] = -1;
            }
            if (j + 1 < ny) {
                col[nz] = k + nx;
                a[nz++] = -1;
            }
            row[k + 1] = nz;
            b[k] = sin(.31 * k) + .7;
            c[k] = cos(.17 * k) - .2;
            sum[k] = 2 * b[k] - 3 * c[k];
        }
    CfdSparseMg *m = cfd_sparse_mg_create(n, row, col, a, x, y, 2, 2);
    assert(m);
    cfd_sparse_mg_apply(m, b, mb);
    cfd_sparse_mg_apply(m, c, mc);
    cfd_sparse_mg_apply(m, sum, ms);
    double symmetry = fabs(dot(b, mc, n) - dot(c, mb, n)) / fmax(1, fabs(dot(b, mc, n))),
           linear = 0;
    for (int k = 0; k < n; k++)
        linear = fmax(linear, fabs(ms[k] - 2 * mb[k] + 3 * mc[k]));
    assert(symmetry < 1e-12 && linear < 1e-11 && dot(b, mb, n) > 0);
    for (int k = 0; k < n; k++) {
        r[k] = b[k];
        for (int q = row[k]; q < row[k + 1]; q++)
            r[k] -= a[q] * mb[col[q]];
    }
    double reduction = sqrt(dot(r, r, n) / dot(b, b, n));
    assert(reduction < .8);
    printf("grid=%dx%d symmetry=%.3g linearity=%.3g residual_ratio=%.6g bytes=%zu\n", nx, ny,
           symmetry, linear, reduction, cfd_sparse_mg_bytes(m));
    cfd_sparse_mg_destroy(m);
    free(row);
    free(col);
    free(a);
    free(v);
}
int main(void) {
    int row[2] = {0, 1}, col[1] = {0};
    double a[1] = {1}, x[1] = {0}, y[1] = {0};
    assert(!cfd_sparse_mg_create(0, row, col, a, x, y, 1, 1));
    x[0] = NAN;
    assert(!cfd_sparse_mg_create(1, row, col, a, x, y, 1, 1));
    x[0] = 0;
    a[0] = -1;
    assert(!cfd_sparse_mg_create(1, row, col, a, x, y, 1, 1));
    a[0] = 1;
    col[0] = 1;
    assert(!cfd_sparse_mg_create(1, row, col, a, x, y, 1, 1));
    col[0] = 0;
    assert(!cfd_sparse_mg_create(1, row, col, a, x, y, 0, 1));
    check(5, 4);
    check(17, 13);
    check(64, 32);
    return 0;
}
