#include "app/cfd_sparse_mg.h"
#include "app/cfd_memory.h"
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    int i, j;
    double a;
} Triplet;
typedef struct {
    long x, y, z;
    int id;
} Bin;
/* Retain the original 2D setup allocation shape and budget thresholds. */
typedef struct {
    long x, y;
    int id;
} Bin2d;
static Bin bin_at(const void *storage, int i, bool three) {
    if (three)
        return ((const Bin *)storage)[i];
    const Bin2d *p = (const Bin2d *)storage + i;
    return (Bin){p->x, p->y, 0, p->id};
}
struct CfdSparseMg {
    int n, nz;
    int *row, *col, *diag, *aggregate;
    double *a, *work, *factor;
    struct CfdSparseMg *coarse;
};
static int triplet_compare(const void *a, const void *b) {
    const Triplet *p = a, *q = b;
    if (p->i != q->i)
        return (p->i > q->i) - (p->i < q->i);
    return (p->j > q->j) - (p->j < q->j);
}
static int bin_compare(const void *a, const void *b) {
    const Bin *p = a, *q = b;
    if (p->x != q->x)
        return (p->x > q->x) - (p->x < q->x);
    if (p->y != q->y)
        return (p->y > q->y) - (p->y < q->y);
    if (p->z != q->z)
        return (p->z > q->z) - (p->z < q->z);
    return (p->id > q->id) - (p->id < q->id);
}
static int bin2d_compare(const void *a, const void *b) {
    const Bin2d *p = a, *q = b;
    if (p->x != q->x)
        return (p->x > q->x) - (p->x < q->x);
    if (p->y != q->y)
        return (p->y > q->y) - (p->y < q->y);
    return (p->id > q->id) - (p->id < q->id);
}
void cfd_sparse_mg_destroy(CfdSparseMg *s) {
    if (!s)
        return;
    cfd_sparse_mg_destroy(s->coarse);
    cfd_memory_free(s->row);
    cfd_memory_free(s->col);
    cfd_memory_free(s->diag);
    cfd_memory_free(s->aggregate);
    cfd_memory_free(s->a);
    cfd_memory_free(s->work);
    cfd_memory_free(s->factor);
    cfd_memory_free(s);
}
static int factor(CfdSparseMg *s) {
    int n = s->n;
    s->factor = cfd_memory_calloc((size_t)n * n, sizeof(double));
    if (!s->factor)
        return 0;
    for (int i = 0; i < n; i++)
        for (int k = s->row[i]; k < s->row[i + 1]; k++)
            s->factor[(size_t)i * n + s->col[k]] = s->a[k];
    for (int i = 0; i < n; i++)
        for (int j = 0; j <= i; j++) {
            double v = s->factor[(size_t)i * n + j];
            for (int k = 0; k < j; k++)
                v -= s->factor[(size_t)i * n + k] * s->factor[(size_t)j * n + k];
            if (i == j) {
                if (!isfinite(v) || v <= 0)
                    return 0;
                s->factor[(size_t)i * n + j] = sqrt(v);
            } else
                s->factor[(size_t)i * n + j] = v / s->factor[(size_t)j * n + j];
        }
    return 1;
}
static CfdSparseMg *create(int n, const int *row, const int *col, const double *a, const double *x,
                           const double *y, const double *z, double hx, double hy, double hz) {
    if (n <= 0 || !row || !col || !a || !x || !y || !isfinite(hx) || !isfinite(hy) || hx <= 0 ||
        hy <= 0 || (z && (!isfinite(hz) || hz <= 0)))
        return NULL;
    if (row[0] != 0 || row[n] <= 0)
        return NULL;
    for (int i = 0; i < n; i++) {
        if (row[i] < 0 || row[i + 1] < row[i] || row[i + 1] > row[n] || !isfinite(x[i]) ||
            !isfinite(y[i]) || !isfinite(x[i] / hx) || !isfinite(y[i] / hy) ||
            x[i] / hx <= (double)LONG_MIN || x[i] / hx >= (double)LONG_MAX ||
            y[i] / hy <= (double)LONG_MIN || y[i] / hy >= (double)LONG_MAX ||
            (z && (!isfinite(z[i]) || !isfinite(z[i] / hz) || z[i] / hz <= (double)LONG_MIN ||
                   z[i] / hz >= (double)LONG_MAX)))
            return NULL;
        for (int k = row[i]; k < row[i + 1]; k++)
            if (col[k] < 0 || col[k] >= n || !isfinite(a[k]) ||
                (k > row[i] && col[k] <= col[k - 1]))
                return NULL;
    }
    CfdSparseMg *s = cfd_memory_calloc(1, sizeof(*s));
    void *bins = NULL;
    Triplet *entries = NULL;
    double *coords = NULL;
    int *cr = NULL, *cc = NULL;
    double *ca = NULL;
    if (!s)
        return NULL;
    s->n = n;
    s->nz = row[n];
    s->row = cfd_memory_malloc(((size_t)n + 1) * sizeof(int));
    s->col = cfd_memory_malloc((size_t)s->nz * sizeof(int));
    s->diag = cfd_memory_malloc((size_t)n * sizeof(int));
    s->a = cfd_memory_malloc((size_t)s->nz * sizeof(double));
    s->work = cfd_memory_calloc((size_t)2 * n, sizeof(double));
    if (!s->row || !s->col || !s->diag || !s->a || !s->work)
        goto fail;
    memcpy(s->row, row, ((size_t)n + 1) * sizeof(int));
    memcpy(s->col, col, (size_t)s->nz * sizeof(int));
    memcpy(s->a, a, (size_t)s->nz * sizeof(double));
    for (int i = 0; i < n; i++) {
        s->diag[i] = -1;
        for (int k = row[i]; k < row[i + 1]; k++)
            if (col[k] == i)
                s->diag[i] = k;
        if (s->diag[i] < 0 || !isfinite(a[s->diag[i]]) || a[s->diag[i]] <= 0)
            goto fail;
    }
    if (n <= 48) {
        if (!factor(s))
            goto fail;
        return s;
    }
    size_t bin_size = z ? sizeof(Bin) : sizeof(Bin2d);
    bins = cfd_memory_malloc((size_t)n * bin_size);
    s->aggregate = cfd_memory_malloc((size_t)n * sizeof(int));
    coords = cfd_memory_malloc((size_t)(z ? 3 : 2) * n * sizeof(double));
    entries = cfd_memory_malloc((size_t)s->nz * sizeof(*entries));
    if (!bins || !s->aggregate || !coords || !entries)
        goto fail;
    for (int i = 0; i < n; i++) {
        if (z)
            ((Bin *)bins)[i] =
                (Bin){(long)floor(x[i] / hx), (long)floor(y[i] / hy), (long)floor(z[i] / hz), i};
        else
            ((Bin2d *)bins)[i] = (Bin2d){(long)floor(x[i] / hx), (long)floor(y[i] / hy), i};
    }
    qsort(bins, n, bin_size, z ? bin_compare : bin2d_compare);
    int nc = 0;
    Bin previous = {0};
    for (int i = 0; i < n; i++) {
        Bin current = bin_at(bins, i, z != NULL);
        if (i == 0 || current.x != previous.x || current.y != previous.y ||
            current.z != previous.z) {
            coords[nc] = (current.x + .5) * hx;
            coords[n + nc] = (current.y + .5) * hy;
            if (z)
                coords[2 * n + nc] = (current.z + .5) * hz;
            nc++;
        }
        s->aggregate[current.id] = nc - 1;
        previous = current;
    }
    if (nc >= n)
        goto fail;
    int used = 0;
    for (int i = 0; i < n; i++)
        for (int k = row[i]; k < row[i + 1]; k++)
            entries[used++] = (Triplet){s->aggregate[i], s->aggregate[col[k]], a[k]};
    qsort(entries, used, sizeof(*entries), triplet_compare);
    int nz = 0;
    for (int k = 0; k < used; k++) {
        if (nz && entries[nz - 1].i == entries[k].i && entries[nz - 1].j == entries[k].j)
            entries[nz - 1].a += entries[k].a;
        else
            entries[nz++] = entries[k];
    }
    cr = cfd_memory_calloc((size_t)nc + 1, sizeof(int));
    cc = cfd_memory_malloc((size_t)nz * sizeof(int));
    ca = cfd_memory_malloc((size_t)nz * sizeof(double));
    if (!cr || !cc || !ca)
        goto fail;
    for (int k = 0; k < nz; k++) {
        cr[entries[k].i + 1]++;
        cc[k] = entries[k].j;
        ca[k] = entries[k].a;
    }
    for (int i = 0; i < nc; i++)
        cr[i + 1] += cr[i];
    s->coarse = create(nc, cr, cc, ca, coords, coords + n, z ? coords + 2 * n : NULL, 2 * hx,
                       2 * hy, 2 * hz);
    if (!s->coarse)
        goto fail;
    cfd_memory_free(bins);
    cfd_memory_free(coords);
    cfd_memory_free(entries);
    cfd_memory_free(cr);
    cfd_memory_free(cc);
    cfd_memory_free(ca);
    return s;
fail:
    cfd_memory_free(bins);
    cfd_memory_free(coords);
    cfd_memory_free(entries);
    cfd_memory_free(cr);
    cfd_memory_free(cc);
    cfd_memory_free(ca);
    cfd_sparse_mg_destroy(s);
    return NULL;
}
CfdSparseMg *cfd_sparse_mg_create(int n, const int *row, const int *col, const double *a,
                                  const double *x, const double *y, double hx, double hy) {
    return create(n, row, col, a, x, y, NULL, hx, hy, 1);
}
CfdSparseMg *cfd_sparse_mg_create3d(int n, const int *row, const int *col, const double *a,
                                    const double *x, const double *y, const double *z, double hx,
                                    double hy, double hz) {
    if (!z)
        return NULL;
    return create(n, row, col, a, x, y, z, hx, hy, hz);
}
static void sweep(CfdSparseMg *s, const double *b, double *x, int forward) {
    for (int t = 0; t < s->n; t++) {
        int i = forward ? t : s->n - 1 - t;
        double v = b[i];
        for (int k = s->row[i]; k < s->row[i + 1]; k++)
            if (s->col[k] != i)
                v -= s->a[k] * x[s->col[k]];
        x[i] = v / s->a[s->diag[i]];
    }
}
void cfd_sparse_mg_apply(CfdSparseMg *s, const double *b, double *x) {
    int n = s->n;
    memset(x, 0, (size_t)n * sizeof(double));
    if (s->factor) {
        for (int i = 0; i < n; i++) {
            double v = b[i];
            for (int j = 0; j < i; j++)
                v -= s->factor[(size_t)i * n + j] * x[j];
            x[i] = v / s->factor[(size_t)i * n + i];
        }
        for (int i = n - 1; i >= 0; i--) {
            double v = x[i];
            for (int j = i + 1; j < n; j++)
                v -= s->factor[(size_t)j * n + i] * x[j];
            x[i] = v / s->factor[(size_t)i * n + i];
        }
        return;
    }
    sweep(s, b, x, 1);
    sweep(s, b, x, 1);
    double *cb = s->work, *cx = cb + n;
    memset(cb, 0, (size_t)s->coarse->n * sizeof(double));
    for (int i = 0; i < n; i++) {
        double r = b[i];
        for (int k = s->row[i]; k < s->row[i + 1]; k++)
            r -= s->a[k] * x[s->col[k]];
        cb[s->aggregate[i]] += r;
    }
    cfd_sparse_mg_apply(s->coarse, cb, cx);
    for (int i = 0; i < n; i++)
        x[i] += cx[s->aggregate[i]];
    sweep(s, b, x, 0);
    sweep(s, b, x, 0);
}
size_t cfd_sparse_mg_bytes(const CfdSparseMg *s) {
    if (!s)
        return 0;
    return sizeof(*s) + ((size_t)2 * s->n + 1 + s->nz + (s->aggregate ? s->n : 0)) * sizeof(int) +
           ((size_t)s->nz + 2 * s->n + (s->factor ? (size_t)s->n * s->n : 0)) * sizeof(double) +
           cfd_sparse_mg_bytes(s->coarse);
}
