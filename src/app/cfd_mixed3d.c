#include "app/cfd_mixed3d.h"
#include "app/cfd_sparse_mg.h"
#include <math.h>
#include <string.h>
typedef struct {
    int n, shape[3], *row, *col;
    double *a, *work;
    CfdSparseMg *mg;
} Momentum;
struct CfdMixed3d {
    CfdCartesian3d grid;
    Momentum h[3], lap;
    int offset[3], count, iterations, inner_iterations;
    double mu, mass, residual;
    double *vwork, *pwork;
    bool periodic;
    const char *error;
    CfdMixed3dCheckpoint check;
    void *context;
};
static bool checkpoint(CfdMixed3d *s) {
    if (s->check && !s->check(s->context)) {
        s->error = "cancelled_at_krylov_checkpoint";
        return false;
    }
    return true;
}
static double dot(const double *a, const double *b, int n) {
    double v = 0;
    for (int q = 0; q < n; q++)
        v += a[q] * b[q];
    return v;
}
static int index3(int i, int j, int k, const int n[3]) { return (k * n[1] + j) * n[0] + i; }
static void apply(const Momentum *h, const double *x, double *b) {
    for (int q = 0; q < h->n; q++) {
        double v = 0;
        for (int e = h->row[q]; e < h->row[q + 1]; e++)
            v += h->a[e] * x[h->col[e]];
        b[q] = v;
    }
}
static bool inverse(CfdMixed3d *s, Momentum *h, const double *b, double *x, int *total) {
    int n = h->n;
    double *r = h->work, *z = r + n, *p = z + n, *ap = p + n, *truth = ap + n;
    memset(x, 0, (size_t)n * sizeof(double));
    memcpy(r, b, (size_t)n * sizeof(double));
    double scale = sqrt(dot(b, b, n)), rho = 0;
    if (scale == 0)
        return true;
    for (int it = 0; it <= 1500; it++) {
        if ((it % 8) == 0 && !checkpoint(s))
            return false;
        double rel = sqrt(dot(r, r, n)) / scale;
        if (rel <= 2e-14 || it == 1500) {
            apply(h, x, truth);
            for (int q = 0; q < n; q++)
                truth[q] = b[q] - truth[q];
            rel = sqrt(dot(truth, truth, n)) / scale;
            if (isfinite(rel) && rel <= 8e-14) {
                *total += it;
                return true;
            }
            if (it == 1500)
                return false;
            memcpy(r, truth, (size_t)n * sizeof(double));
            rho = 0;
        }
        cfd_sparse_mg_apply(h->mg, r, z);
        double next = dot(r, z, n);
        if (!isfinite(next) || next <= 0)
            return false;
        double beta = rho > 0 ? next / rho : 0;
        for (int q = 0; q < n; q++)
            p[q] = z[q] + beta * p[q];
        rho = next;
        apply(h, p, ap);
        double denom = dot(p, ap, n);
        if (!isfinite(denom) || denom <= 0)
            return false;
        double alpha = rho / denom;
        for (int q = 0; q < n; q++) {
            x[q] += alpha * p[q];
            r[q] -= alpha * ap[q];
        }
    }
    return false;
}

static void destroy_matrix(Momentum *h) {
    cfd_sparse_mg_destroy(h->mg);
    cfd_memory_free(h->row);
    cfd_memory_free(h->col);
    cfd_memory_free(h->a);
    cfd_memory_free(h->work);
    memset(h, 0, sizeof(*h));
}
void cfd_mixed3d_destroy(CfdMixed3d *s) {
    if (!s)
        return;
    for (int a = 0; a < 3; a++)
        destroy_matrix(&s->h[a]);
    destroy_matrix(&s->lap);
    cfd_memory_free(s->vwork);
    cfd_memory_free(s->pwork);
    cfd_memory_free(s);
}
static void append(Momentum *h, int q, int *used, int col[7], double val[7], int count) {
    for (int a = 1; a < count; a++) {
        int c = col[a], j = a;
        double v = val[a];
        while (j > 0 && col[j - 1] > c) {
            col[j] = col[j - 1];
            val[j] = val[j - 1];
            j--;
        }
        col[j] = c;
        val[j] = v;
    }
    for (int a = 0; a < count; a++) {
        h->col[*used] = col[a];
        h->a[(*used)++] = val[a];
    }
    h->row[q + 1] = *used;
}
static bool allocate_matrix(Momentum *h) {
    h->row = cfd_memory_calloc((size_t)h->n + 1, sizeof(int));
    h->col = cfd_memory_malloc((size_t)7 * h->n * sizeof(int));
    h->a = cfd_memory_malloc((size_t)7 * h->n * sizeof(double));
    h->work = cfd_memory_calloc((size_t)5 * h->n, sizeof(double));
    return h->row && h->col && h->a && h->work;
}
static bool hierarchy(CfdMixed3d *s, Momentum *h, int axis) {
    double *c = cfd_memory_malloc((size_t)3 * h->n * sizeof(double));
    if (!c)
        return false;
    for (int k = 0; k < h->shape[2]; k++)
        for (int j = 0; j < h->shape[1]; j++)
            for (int i = 0; i < h->shape[0]; i++) {
                int p[3] = {i, j, k}, q = index3(i, j, k, h->shape);
                for (int a = 0; a < 3; a++)
                    c[a * h->n + q] = (p[a] + (a == axis ? (a == 0 ? 0 : 1) : .5)) * s->grid.h[a];
            }
    CfdSparseMg *mg = cfd_sparse_mg_create3d(h->n, h->row, h->col, h->a, c, c + h->n, c + 2 * h->n,
                                             2 * s->grid.h[0], 2 * s->grid.h[1], 2 * s->grid.h[2]);
    cfd_memory_free(c);
    if (!mg)
        return false;
    cfd_sparse_mg_destroy(h->mg);
    h->mg = mg;
    return true;
}
static bool build(CfdMixed3d *s, int axis) {
    Momentum *h = &s->h[axis];
    const CfdCartesian3d *g = &s->grid;
    if (!allocate_matrix(h))
        return false;
    int used = 0;
    for (int k = 0; k < h->shape[2]; k++)
        for (int j = 0; j < h->shape[1]; j++)
            for (int i = 0; i < h->shape[0]; i++) {
                int p[3] = {i, j, k}, q = index3(i, j, k, h->shape), col[7] = {q}, count = 1;
                double vol = g->volume *
                             (axis == 0 && !s->periodic && (i == 0 || i == g->n[0]) ? .5 : 1),
                       val[7] = {s->mass * vol};
                for (int a = 0; a < 3; a++)
                    for (int d = -1; d <= 1; d += 2) {
                        double coeff = s->mu * vol / (g->h[a] * g->h[a]);
                        if (axis == 0 && a == 0)
                            coeff = s->mu * g->area[0] / g->h[0];
                        int c = p[a] + d, st = a == 0   ? 1
                                               : a == 1 ? h->shape[0]
                                                        : h->shape[0] * h->shape[1];
                        if (a == 0 && s->periodic) {
                            if (c < 0)
                                c += h->shape[0];
                            if (c >= h->shape[0])
                                c -= h->shape[0];
                        }
                        if (c >= 0 && c < h->shape[a]) {
                            val[0] += coeff;
                            col[count] = q + (c - p[a]) * st;
                            val[count++] = -coeff;
                        } else if (a != 0)
                            val[0] += (a == axis ? 1 : 2) * coeff;
                    }
                append(h, q, &used, col, val, count);
            }
    return hierarchy(s, h, axis);
}
static bool laplacian(CfdMixed3d *s) {
    Momentum *h = &s->lap;
    const CfdCartesian3d *g = &s->grid;
    h->n = g->count;
    memcpy(h->shape, g->n, sizeof(h->shape));
    if (!allocate_matrix(h))
        return false;
    int used = 0;
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int p[3] = {i, j, k}, q = index3(i, j, k, g->n), col[7] = {q}, count = 1;
                double val[7] = {0};
                if (s->periodic && q == 0)
                    val[0] = 1;
                else
                    for (int a = 0; a < 3; a++)
                        for (int d = -1; d <= 1; d += 2) {
                            int c = p[a] + d, st = a == 0   ? 1
                                                   : a == 1 ? g->n[0]
                                                            : g->n[0] * g->n[1];
                            double coeff = g->volume / (g->h[a] * g->h[a]);
                            if (a == 0 && s->periodic) {
                                if (c < 0)
                                    c += g->n[0];
                                if (c >= g->n[0])
                                    c -= g->n[0];
                            }
                            if (c >= 0 && c < g->n[a]) {
                                int t = q + (c - p[a]) * st;
                                val[0] += coeff;
                                if (!(s->periodic && t == 0)) {
                                    col[count] = t;
                                    val[count++] = -coeff;
                                }
                            } else if (a == 0)
                                val[0] += 2 * coeff;
                        }
                append(h, q, &used, col, val, count);
            }
    return hierarchy(s, h, -1);
}
CfdMixed3d *cfd_mixed3d_create(const CfdCartesian3d *g, double mu, double mass, bool periodic) {
    if (!g || !isfinite(mu) || mu <= 0 || !isfinite(mass) || mass < 0)
        return NULL;
    CfdMixed3d *s = cfd_memory_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->grid = *g;
    s->mu = mu;
    s->mass = mass;
    s->periodic = periodic;
    for (int a = 0; a < 3; a++) {
        Momentum *h = &s->h[a];
        for (int b = 0; b < 3; b++)
            h->shape[b] = g->n[b] + (a == 0 && b == 0 && !periodic) - (a == b && a != 0);
        h->n = h->shape[0] * h->shape[1] * h->shape[2];
        s->offset[a] = s->count;
        s->count += h->n;
        if (!build(s, a))
            goto fail;
    }
    if (!laplacian(s))
        goto fail;
    s->vwork = cfd_memory_calloc((size_t)3 * s->count, sizeof(double));
    s->pwork = cfd_memory_calloc((size_t)6 * g->count, sizeof(double));
    if (!s->vwork || !s->pwork)
        goto fail;
    return s;
fail:
    cfd_mixed3d_destroy(s);
    return NULL;
}
int cfd_mixed3d_count(const CfdMixed3d *s) { return s->count; }
int cfd_mixed3d_index(const CfdMixed3d *s, int axis, int i, int j, int k) {
    int c[3] = {i, j, k};
    const Momentum *h = &s->h[axis];
    if (axis > 0) {
        if (c[axis] == 0 || c[axis] == s->grid.n[axis])
            return -1;
        c[axis]--;
    }
    if (s->periodic) {
        c[0] %= s->grid.n[0];
        if (c[0] < 0)
            c[0] += s->grid.n[0];
    }
    for (int a = 0; a < 3; a++)
        if (c[a] < 0 || c[a] >= h->shape[a])
            return -1;
    return s->offset[axis] + index3(c[0], c[1], c[2], h->shape);
}
void cfd_mixed3d_position(const CfdMixed3d *s, int q, int *axis, double xyz[3], int ijk[3]) {
    int a = q >= s->offset[2] ? 2 : q >= s->offset[1] ? 1 : 0, local = q - s->offset[a], stride = 1;
    const Momentum *h = &s->h[a];
    if (axis)
        *axis = a;
    for (int b = 0; b < 3; b++) {
        int c = (local / stride) % h->shape[b] + (a == b && a != 0);
        if (ijk)
            ijk[b] = c;
        xyz[b] = (c + (a == b ? 0 : .5)) * s->grid.h[b];
        stride *= h->shape[b];
    }
}
double cfd_mixed3d_volume(const CfdMixed3d *s, int q) {
    if (!s->periodic && q < s->h[0].n) {
        int i = q % s->h[0].shape[0];
        if (i == 0 || i == s->grid.n[0])
            return .5 * s->grid.volume;
    }
    return s->grid.volume;
}
bool cfd_mixed3d_set_mass(CfdMixed3d *s, double mass) {
    if (!s || !isfinite(mass) || mass < 0)
        return false;
    if (mass == s->mass)
        return true;
    for (int a = 0; a < 3; a++) {
        Momentum *h = &s->h[a];
        for (int q = 0; q < h->n; q++)
            for (int e = h->row[q]; e < h->row[q + 1]; e++)
                if (h->col[e] == q)
                    h->a[e] += (mass - s->mass) * cfd_mixed3d_volume(s, s->offset[a] + q);
        if (!hierarchy(s, h, a)) {
            s->error = "momentum_mass_rebuild_failed";
            return false;
        }
    }
    s->mass = mass;
    return true;
}
static void transpose(const CfdMixed3d *s, const double *p, double *v) {
    const CfdCartesian3d *g = &s->grid;
    memset(v, 0, (size_t)s->count * sizeof(double));
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int c[3] = {i, j, k}, q = index3(i, j, k, g->n);
                for (int a = 0; a < 3; a++) {
                    int hi[3] = {i, j, k};
                    hi[a]++;
                    int east = cfd_mixed3d_index(s, a, hi[0], hi[1], hi[2]),
                        west = cfd_mixed3d_index(s, a, c[0], c[1], c[2]);
                    if (east >= 0)
                        v[east] += g->area[a] * p[q];
                    if (west >= 0)
                        v[west] -= g->area[a] * p[q];
                }
            }
}
void cfd_mixed3d_divergence(const CfdMixed3d *s, const double *v, double *p) {
    const CfdCartesian3d *g = &s->grid;
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int c[3] = {i, j, k}, q = index3(i, j, k, g->n);
                double sum = 0;
                for (int a = 0; a < 3; a++) {
                    int hi[3] = {i, j, k};
                    hi[a]++;
                    int east = cfd_mixed3d_index(s, a, hi[0], hi[1], hi[2]),
                        west = cfd_mixed3d_index(s, a, c[0], c[1], c[2]);
                    sum += g->area[a] * ((east >= 0 ? v[east] : 0) - (west >= 0 ? v[west] : 0));
                }
                p[q] = sum;
            }
}
static bool hinv(CfdMixed3d *s, const double *b, double *u) {
    for (int a = 0; a < 3; a++)
        if (!inverse(s, &s->h[a], b + s->offset[a], u + s->offset[a], &s->inner_iterations))
            return false;
    return true;
}
static bool schur(CfdMixed3d *s, const double *p, double *out) {
    double *b = s->vwork, *u = b + s->count;
    transpose(s, p, b);
    if (!hinv(s, b, u))
        return false;
    cfd_mixed3d_divergence(s, u, out);
    if (s->periodic)
        out[0] = 0;
    return true;
}
static void precondition(CfdMixed3d *s, const double *r, double *z) {
    cfd_sparse_mg_apply(s->lap.mg, r, z);
    for (int q = 0; q < s->grid.count; q++)
        z[q] = s->mass * z[q] + s->mu / s->grid.volume * r[q];
    if (s->periodic)
        z[0] = 0;
}
bool cfd_mixed3d_solve(CfdMixed3d *s, const double *rhs, double *u, double *p) {
    if (!s || !rhs || !u || !p)
        return false;
    s->error = NULL;
    s->iterations = s->inner_iterations = 0;
    int n = s->grid.count;
    double *r = s->pwork, *d = r + n, *sd = d + n, *b = sd + n, *truth = b + n, *z = truth + n;
    if (s->periodic) {
        double datum = p[0];
        for (int q = 0; q < n; q++)
            p[q] -= datum;
    }
    if (!hinv(s, rhs, s->vwork + 2 * s->count))
        goto fail;
    cfd_mixed3d_divergence(s, s->vwork + 2 * s->count, b);
    for (int q = 0; q < n; q++)
        b[q] = -b[q];
    if (s->periodic)
        b[0] = 0;
    if (!schur(s, p, r))
        goto fail;
    for (int q = 0; q < n; q++)
        r[q] = b[q] - r[q];
    double scale = sqrt(dot(b, b, n));
    if (scale < 1e-30)
        scale = 1;
    double rho = 0;
    bool accepted = false;
    for (int it = 0; it <= 1000; it++) {
        if (!checkpoint(s))
            goto fail;
        if (sqrt(dot(r, r, n)) / scale <= 1e-12 || it == 1000) {
            if (!schur(s, p, truth))
                goto fail;
            for (int q = 0; q < n; q++)
                truth[q] = b[q] - truth[q];
            double rel = sqrt(dot(truth, truth, n)) / scale;
            if (isfinite(rel) && rel <= 3e-12) {
                s->iterations = it;
                accepted = true;
                break;
            }
            if (it == 1000)
                break;
            memcpy(r, truth, (size_t)n * sizeof(double));
            rho = 0;
        }
        precondition(s, r, z);
        double next = dot(r, z, n);
        if (!isfinite(next) || next <= 0)
            goto fail;
        double beta = rho > 0 ? next / rho : 0;
        for (int q = 0; q < n; q++)
            d[q] = z[q] + beta * d[q];
        rho = next;
        if (!schur(s, d, sd))
            goto fail;
        double denom = dot(d, sd, n);
        if (!isfinite(denom) || denom <= 0)
            goto fail;
        double alpha = rho / denom;
        for (int q = 0; q < n; q++) {
            p[q] += alpha * d[q];
            r[q] -= alpha * sd[q];
        }
    }
    if (!accepted) {
        s->error = "pressure_true_residual_not_converged";
        return false;
    }
    transpose(s, p, s->vwork);
    for (int q = 0; q < s->count; q++)
        s->vwork[q] += rhs[q];
    if (!hinv(s, s->vwork, u))
        goto fail;
    double residual = 0, norm = 0;
    for (int a = 0; a < 3; a++) {
        int offset = s->offset[a];
        apply(&s->h[a], u + offset, s->vwork + s->count + offset);
        for (int q = 0; q < s->h[a].n; q++) {
            double b0 = s->vwork[offset + q], v = s->vwork[s->count + offset + q] - b0;
            residual += v * v;
            norm += b0 * b0;
        }
    }
    s->residual = sqrt(residual / fmax(norm, 1e-60));
    if (!isfinite(s->residual) || s->residual > 1e-11) {
        s->error = "complete_momentum_residual_failed";
        return false;
    }
    cfd_mixed3d_divergence(s, u, truth);
    for (int q = 0; q < n; q++)
        if (!isfinite(truth[q]) || fabs(truth[q]) / s->grid.volume >= 1e-8) {
            s->error = "complete_continuity_residual_failed";
            return false;
        }
    if (s->periodic) {
        double mean = 0;
        for (int q = 0; q < n; q++)
            mean += p[q] / n;
        for (int q = 0; q < n; q++)
            p[q] -= mean;
    }
    return true;
fail:
    if (!s->error)
        s->error = "velocity_inverse_failed";
    return false;
}
void cfd_mixed3d_checkpoint(CfdMixed3d *s, CfdMixed3dCheckpoint function, void *context) {
    s->check = function;
    s->context = context;
}
int cfd_mixed3d_iterations(const CfdMixed3d *s) { return s->iterations; }
int cfd_mixed3d_inner_iterations(const CfdMixed3d *s) { return s->inner_iterations; }
double cfd_mixed3d_residual(const CfdMixed3d *s) { return s->residual; }
const char *cfd_mixed3d_error(const CfdMixed3d *s) { return s->error; }

#ifdef CFD_MIXED3D_VERIFY
/* Independent deterministic vectors exercise all components and boundaries.
 * Verification borrows idle workspaces and never enters the published state. */
bool cfd_mixed3d_verify(CfdMixed3d *s) {
    double *v = s->vwork, *bt = v + s->count, *hv = bt + s->count;
    double *p = s->pwork, *bv = p + s->grid.count;
    for (int q = 0; q < s->count; q++)
        v[q] = sin(.37 * (q + 1)) + .2 * cos(.83 * q);
    for (int q = 0; q < s->grid.count; q++)
        p[q] = cos(.71 * (q + 1)) - .3 * sin(.23 * q);
    cfd_mixed3d_divergence(s, v, bv);
    transpose(s, p, bt);
    double left = dot(p, bv, s->grid.count), right = dot(v, bt, s->count);
    if (fabs(left - right) > 1e-12 * fmax(1., fmax(fabs(left), fabs(right))))
        return false;
    for (int a = 0; a < 3; a++) {
        Momentum *h = &s->h[a];
        for (int q = 0; q < h->n; q++)
            for (int e = h->row[q]; e < h->row[q + 1]; e++) {
                int other = h->col[e];
                bool found = false;
                for (int f = h->row[other]; f < h->row[other + 1]; f++)
                    if (h->col[f] == q && fabs(h->a[e] - h->a[f]) < 1e-14) {
                        found = true;
                        break;
                    }
                if (!found)
                    return false;
            }
        apply(h, v + s->offset[a], hv + s->offset[a]);
        if (dot(v + s->offset[a], hv + s->offset[a], h->n) <= 0)
            return false;
    }
    if (s->periodic) {
        for (int q = 0; q < s->grid.count; q++)
            p[q] = 1;
        transpose(s, p, bt);
        if (dot(bt, bt, s->count) > 1e-28)
            return false;
    }
    return true;
}
#endif
