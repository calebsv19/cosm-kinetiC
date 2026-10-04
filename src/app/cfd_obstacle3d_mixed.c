#include "app/cfd_obstacle3d.h"
#include "app/cfd_sparse_mg.h"
#include <math.h>
#include <string.h>
typedef struct {
    int n, shape[3], *row, *col;
    double *a, *work;
    CfdSparseMg *mg;
} Momentum;
struct CfdObstacleMixed3d {
    CfdCartesian3d grid;
    Momentum h[3];
    int offset[3], count, cells, iterations, inner_iterations;
    int *cell_map, *cell_flat, *face_map[3], *face_flat[3];
    int face_shape[3][3], lo[3], hi[3];
    double mu, mass, residual;
    double *vwork, *pwork;

    const char *error;
    CfdMixed3dCheckpoint check;
    void *context;
};
static bool checkpoint(CfdObstacleMixed3d *s) {
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
static bool inverse(CfdObstacleMixed3d *s, Momentum *h, const double *b, double *x, int *total) {
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
void cfd_obstacle_mixed3d_destroy(CfdObstacleMixed3d *s) {
    if (!s)
        return;
    for (int a = 0; a < 3; a++)
        destroy_matrix(&s->h[a]);

    cfd_memory_free(s->cell_map);
    cfd_memory_free(s->cell_flat);
    for (int a = 0; a < 3; a++) {
        cfd_memory_free(s->face_map[a]);
        cfd_memory_free(s->face_flat[a]);
    }
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
static int fluid(const CfdObstacleMixed3d *s, const int c[3]) {
    for (int a = 0; a < 3; a++)
        if (c[a] < 0 || c[a] >= s->grid.n[a])
            return -1;
    return s->cell_map[index3(c[0], c[1], c[2], s->grid.n)];
}
int cfd_obstacle_mixed3d_cell(const CfdObstacleMixed3d *s, int i, int j, int k) {
    int c[3] = {i, j, k};
    return fluid(s, c);
}
int cfd_obstacle_mixed3d_index(const CfdObstacleMixed3d *s, int axis, int i, int j, int k) {
    int c[3] = {i, j, k};
    for (int a = 0; a < 3; a++)
        if (c[a] < 0 || c[a] >= s->face_shape[axis][a])
            return -1;
    return s->face_map[axis][index3(i, j, k, s->face_shape[axis])];
}
void cfd_obstacle_mixed3d_position(const CfdObstacleMixed3d *s, int q, int *axis, double xyz[3],
                                   int ijk[3]) {
    int a = q < s->offset[0] + s->h[0].n ? 0 : q < s->offset[1] + s->h[1].n ? 1 : 2;
    int flat = s->face_flat[a][q - s->offset[a]], stride = 1;
    if (axis)
        *axis = a;
    for (int b = 0; b < 3; b++) {
        int c = (flat / stride) % s->face_shape[a][b];
        if (ijk)
            ijk[b] = c;
        xyz[b] = (c + (a == b ? 0 : .5)) * s->grid.h[b];
        stride *= s->face_shape[a][b];
    }
}
double cfd_obstacle_mixed3d_volume(const CfdObstacleMixed3d *s, int q) {
    int a, c[3];
    double x[3];
    cfd_obstacle_mixed3d_position(s, q, &a, x, c);
    return s->grid.volume * (a == 0 && (c[0] == 0 || c[0] == s->grid.n[0]) ? .5 : 1);
}
static bool hierarchy(CfdObstacleMixed3d *s, Momentum *h, int axis) {
    double *c = cfd_memory_malloc((size_t)3 * h->n * sizeof(double));
    if (!c)
        return false;
    for (int q = 0; q < h->n; q++) {
        double xyz[3];
        if (axis >= 0)
            cfd_obstacle_mixed3d_position(s, s->offset[axis] + q, NULL, xyz, NULL);
        else {
            int flat = s->cell_flat[q], stride = 1;
            for (int a = 0; a < 3; a++) {
                xyz[a] = ((flat / stride) % s->grid.n[a] + .5) * s->grid.h[a];
                stride *= s->grid.n[a];
            }
        }
        for (int a = 0; a < 3; a++)
            c[a * h->n + q] = xyz[a];
    }
    h->mg = cfd_sparse_mg_create3d(h->n, h->row, h->col, h->a, c, c + h->n, c + 2 * h->n,
                                   2 * s->grid.h[0], 2 * s->grid.h[1], 2 * s->grid.h[2]);
    cfd_memory_free(c);
    return h->mg != NULL;
}
static bool build(CfdObstacleMixed3d *s, int axis) {
    Momentum *h = &s->h[axis];
    const CfdCartesian3d *g = &s->grid;
    if (!allocate_matrix(h))
        return false;
    int used = 0;
    for (int q = 0; q < h->n; q++) {
        int c[3], col[7] = {q}, count = 1;
        double xyz[3], val[7] = {0};
        cfd_obstacle_mixed3d_position(s, s->offset[axis] + q, NULL, xyz, c);
        double vol = cfd_obstacle_mixed3d_volume(s, s->offset[axis] + q);
        for (int a = 0; a < 3; a++)
            for (int d = -1; d <= 1; d += 2) {
                int t[3] = {c[0], c[1], c[2]};
                t[a] += d;
                int other = cfd_obstacle_mixed3d_index(s, axis, t[0], t[1], t[2]);
                double coeff = s->mu * vol / (g->h[a] * g->h[a]);
                if (axis == 0 && a == 0)
                    coeff = s->mu * g->area[0] / g->h[0];
                if (other >= 0) {
                    val[0] += coeff;
                    col[count] = other - s->offset[axis];
                    val[count++] = -coeff;
                } else if (!(a == 0 && (t[0] < 0 || t[0] >= s->face_shape[axis][0])))
                    val[0] += (a == axis ? 1 : 2) * coeff;
            }
        append(h, q, &used, col, val, count);
    }
    return hierarchy(s, h, axis);
}
CfdObstacleMixed3d *cfd_obstacle_mixed3d_create(const CfdCartesian3d *g, double mu, const int lo[3],
                                                const int hi[3]) {
    if (!g || !lo || !hi || !isfinite(mu) || mu <= 0)
        return NULL;
    for (int a = 0; a < 3; a++)
        if (lo[a] < 1 || hi[a] >= g->n[a] || hi[a] <= lo[a])
            return NULL;
    CfdObstacleMixed3d *s = cfd_memory_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->grid = *g;
    s->mu = mu;
    memcpy(s->lo, lo, sizeof(s->lo));
    memcpy(s->hi, hi, sizeof(s->hi));
    s->cell_map = cfd_memory_malloc((size_t)g->count * sizeof(int));
    s->cell_flat = cfd_memory_malloc((size_t)g->count * sizeof(int));
    if (!s->cell_map || !s->cell_flat)
        goto fail;
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int q = index3(i, j, k, g->n);
                bool solid =
                    i >= lo[0] && i < hi[0] && j >= lo[1] && j < hi[1] && k >= lo[2] && k < hi[2];
                s->cell_map[q] = solid ? -1 : s->cells;
                if (!solid)
                    s->cell_flat[s->cells++] = q;
            }
    for (int a = 0; a < 3; a++) {
        int size = 1;
        for (int b = 0; b < 3; b++) {
            s->face_shape[a][b] = g->n[b] + (a == b);
            size *= s->face_shape[a][b];
        }
        s->face_map[a] = cfd_memory_malloc((size_t)size * sizeof(int));
        s->face_flat[a] = cfd_memory_malloc((size_t)size * sizeof(int));
        if (!s->face_map[a] || !s->face_flat[a])
            goto fail;
        s->offset[a] = s->count;
        for (int k = 0; k < s->face_shape[a][2]; k++)
            for (int j = 0; j < s->face_shape[a][1]; j++)
                for (int i = 0; i < s->face_shape[a][0]; i++) {
                    int q = index3(i, j, k, s->face_shape[a]), c[3] = {i, j, k},
                        lower[3] = {i, j, k};
                    lower[a]--;
                    bool active = fluid(s, c) >= 0 && fluid(s, lower) >= 0;
                    if (a == 0 && (i == 0 || i == g->n[0]))
                        active = fluid(s, i == 0 ? c : lower) >= 0;
                    s->face_map[a][q] = active ? s->count : -1;
                    if (active) {
                        s->face_flat[a][s->h[a].n++] = q;
                        s->count++;
                    }
                }
        if (!build(s, a))
            goto fail;
    }

    s->vwork = cfd_memory_calloc((size_t)3 * s->count, sizeof(double));
    s->pwork = cfd_memory_calloc((size_t)6 * s->cells, sizeof(double));
    if (!s->vwork || !s->pwork)
        goto fail;
    return s;
fail:
    cfd_obstacle_mixed3d_destroy(s);
    return NULL;
}
int cfd_obstacle_mixed3d_count(const CfdObstacleMixed3d *s) { return s->count; }
int cfd_obstacle_mixed3d_cells(const CfdObstacleMixed3d *s) { return s->cells; }
static void transpose(const CfdObstacleMixed3d *s, const double *p, double *v) {
    memset(v, 0, (size_t)s->count * sizeof(double));
    const CfdCartesian3d *g = &s->grid;
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int cell = cfd_obstacle_mixed3d_cell(s, i, j, k);
                if (cell < 0)
                    continue;
                int c[3] = {i, j, k};
                for (int a = 0; a < 3; a++)
                    for (int d = 0; d <= 1; d++) {
                        int t[3] = {c[0], c[1], c[2]};
                        t[a] += d;
                        int q = cfd_obstacle_mixed3d_index(s, a, t[0], t[1], t[2]);
                        if (q >= 0)
                            v[q] += (d ? 1 : -1) * g->area[a] * p[cell];
                    }
            }
}
void cfd_obstacle_mixed3d_divergence(const CfdObstacleMixed3d *s, const double *v, double *p) {
    const CfdCartesian3d *g = &s->grid;
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int cell = cfd_obstacle_mixed3d_cell(s, i, j, k);
                if (cell < 0)
                    continue;
                double value = 0;
                int c[3] = {i, j, k};
                for (int a = 0; a < 3; a++)
                    for (int d = 0; d <= 1; d++) {
                        int t[3] = {c[0], c[1], c[2]};
                        t[a] += d;
                        int q = cfd_obstacle_mixed3d_index(s, a, t[0], t[1], t[2]);
                        if (q >= 0)
                            value += (d ? 1 : -1) * g->area[a] * v[q];
                    }
                p[cell] = value;
            }
}
static bool hinv(CfdObstacleMixed3d *s, const double *b, double *u) {
    for (int a = 0; a < 3; a++)
        if (!inverse(s, &s->h[a], b + s->offset[a], u + s->offset[a], &s->inner_iterations))
            return false;
    return true;
}
static bool schur(CfdObstacleMixed3d *s, const double *p, double *out) {
    double *b = s->vwork, *u = b + s->count;
    transpose(s, p, b);
    if (!hinv(s, b, u))
        return false;
    cfd_obstacle_mixed3d_divergence(s, u, out);

    return true;
}
static void precondition(CfdObstacleMixed3d *s, const double *r, double *z) {

    for (int q = 0; q < s->cells; q++)
        z[q] = s->mu / s->grid.volume * r[q];
}
bool cfd_obstacle_mixed3d_solve(CfdObstacleMixed3d *s, const double *rhs, double *u, double *p) {
    if (!s || !rhs || !u || !p)
        return false;
    s->error = NULL;
    s->iterations = s->inner_iterations = 0;
    int n = s->cells;
    double *r = s->pwork, *d = r + n, *sd = d + n, *b = sd + n, *truth = b + n, *z = truth + n;
    if (!hinv(s, rhs, s->vwork + 2 * s->count))
        goto fail;
    cfd_obstacle_mixed3d_divergence(s, s->vwork + 2 * s->count, b);
    for (int q = 0; q < n; q++)
        b[q] = -b[q];

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
    cfd_obstacle_mixed3d_divergence(s, u, truth);
    for (int q = 0; q < n; q++)
        if (!isfinite(truth[q]) || fabs(truth[q]) / s->grid.volume >= 1e-8) {
            s->error = "complete_continuity_residual_failed";
            return false;
        }
    return true;
fail:
    if (!s->error)
        s->error = "velocity_inverse_failed";
    return false;
}
void cfd_obstacle_mixed3d_checkpoint(CfdObstacleMixed3d *s, CfdMixed3dCheckpoint function,
                                     void *context) {
    s->check = function;
    s->context = context;
}
int cfd_obstacle_mixed3d_iterations(const CfdObstacleMixed3d *s) { return s->iterations; }
int cfd_obstacle_mixed3d_inner_iterations(const CfdObstacleMixed3d *s) {
    return s->inner_iterations;
}
double cfd_obstacle_mixed3d_residual(const CfdObstacleMixed3d *s) { return s->residual; }
const char *cfd_obstacle_mixed3d_error(const CfdObstacleMixed3d *s) { return s->error; }

#ifdef CFD_MIXED3D_VERIFY
/* Independent deterministic vectors exercise all components and boundaries.
 * Verification borrows idle workspaces and never enters the published state. */
bool cfd_obstacle_mixed3d_verify(CfdObstacleMixed3d *s) {
    double *v = s->vwork, *bt = v + s->count, *hv = bt + s->count;
    double *p = s->pwork, *bv = p + s->cells;
    for (int q = 0; q < s->count; q++)
        v[q] = sin(.37 * (q + 1)) + .2 * cos(.83 * q);
    for (int q = 0; q < s->cells; q++)
        p[q] = cos(.71 * (q + 1)) - .3 * sin(.23 * q);
    cfd_obstacle_mixed3d_divergence(s, v, bv);
    transpose(s, p, bt);
    double left = dot(p, bv, s->cells), right = dot(v, bt, s->count);
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
    return true;
}
#endif

double cfd_obstacle_mixed3d_work(CfdObstacleMixed3d *s, const double *u) {
    double value = 0;
    for (int a = 0; a < 3; a++) {
        apply(&s->h[a], u + s->offset[a], s->vwork + s->offset[a]);
        value += dot(u + s->offset[a], s->vwork + s->offset[a], s->h[a].n);
    }
    return value;
}
void cfd_obstacle_mixed3d_reaction(const CfdObstacleMixed3d *s, const double *u, const double *p,
                                   double body_pressure[3], double body_viscous[3],
                                   double walls[4][3]) {
    const CfdCartesian3d *g = &s->grid;
    memset(body_pressure, 0, 3 * sizeof(double));
    memset(body_viscous, 0, 3 * sizeof(double));
    memset(walls, 0, 12 * sizeof(double));
    for (int a = 0; a < 3; a++)
        for (int side = 0; side < 2; side++) {
            int c[3], direction = side ? 1 : -1;
            for (int k = s->lo[2]; k < s->hi[2]; k++)
                for (int j = s->lo[1]; j < s->hi[1]; j++)
                    for (int i = s->lo[0]; i < s->hi[0]; i++) {
                        c[0] = i;
                        c[1] = j;
                        c[2] = k;
                        if (c[a] != s->lo[a])
                            continue;
                        c[a] = side ? s->hi[a] : s->lo[a] - 1;
                        int q = fluid(s, c);
                        body_pressure[a] -= direction * p[q] * g->area[a];
                    }
        }
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int c[3] = {i, j, k}, cell = fluid(s, c);
                if (cell < 0)
                    continue;
                for (int a = 1; a < 3; a++)
                    for (int side = 0; side < 2; side++)
                        if (c[a] == (side ? g->n[a] - 1 : 0))
                            walls[2 * (a - 1) + side][a] -= (side ? -1 : 1) * p[cell] * g->area[a];
            }
    for (int q = 0; q < s->count; q++) {
        int component, c[3];
        double xyz[3];
        cfd_obstacle_mixed3d_position(s, q, &component, xyz, c);
        double vol = cfd_obstacle_mixed3d_volume(s, q);
        for (int a = 0; a < 3; a++)
            for (int d = -1; d <= 1; d += 2) {
                int t[3] = {c[0], c[1], c[2]};
                t[a] += d;
                if (cfd_obstacle_mixed3d_index(s, component, t[0], t[1], t[2]) >= 0)
                    continue;
                if (a == 0 && (t[0] < 0 || t[0] >= s->face_shape[component][0]))
                    continue;
                double coeff = s->mu * vol / (g->h[a] * g->h[a]);
                if (component == 0 && a == 0)
                    coeff = s->mu * g->area[0] / g->h[0];
                coeff *= a == component ? 1 : 2;
                double at[3] = {xyz[0], xyz[1], xyz[2]};
                at[a] += d * g->h[a] * (a == component ? 1 : .5);
                bool body = true;
                for (int b = 0; b < 3; b++)
                    if (at[b] < s->lo[b] * g->h[b] - 1e-10 || at[b] > s->hi[b] * g->h[b] + 1e-10)
                        body = false;
                if (body)
                    body_viscous[component] += coeff * u[q];
                else if (a > 0)
                    walls[2 * (a - 1) + (d > 0)][component] += coeff * u[q];
            }
    }
}
