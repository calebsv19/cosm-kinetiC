#include "app/cfd_memory.h"
#include "app/cfd_refined_mixed.h"
#include "app/cfd_refined_diffusion.h"
#ifndef CFD_REFINED_VERIFY_ILU
#include "app/cfd_sparse_mg.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define RESTART 60
typedef struct {
    int row, col;
    double value;
} Entry;
typedef struct {
    Entry *e;
    int used, capacity, nv;
    double nu;
} Assembly;
struct CfdRefinedMixed {
    const CfdRefinedMesh *m;
    int n, nv, nz;
    bool solved;
#ifndef CFD_REFINED_VERIFY_ILU
    CfdSparseMg *velocity_mg, *pressure_mg;
    double *pressure_work;
#endif
    int *row, *col, *diag;
    unsigned char *fixed;
    double nu;
    double mass, *base, *a, *lu, *storage, *x, *b, *r, *w, *tmp, *known, *basis;
};
static bool add(Assembly *a, int row, int col, double value) {
    if (a->used == a->capacity) {
        int capacity = a->capacity ? a->capacity * 2 : 1024;
        Entry *p = cfd_memory_realloc(a->e, (size_t)capacity * sizeof(*p));
        if (!p)
            return false;
        a->e = p;
        a->capacity = capacity;
    }
    a->e[a->used++] = (Entry){row, col, value};
    return true;
}
static bool collect(int row, int col, double value, void *context) {
    Assembly *a = context;
    return add(a, row, col, a->nu * value) && add(a, row + a->nv, col + a->nv, a->nu * value);
}
static int compare(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    if (x->row != y->row)
        return x->row < y->row ? -1 : 1;
    return (x->col > y->col) - (x->col < y->col);
}
void cfd_refined_mixed_destroy(CfdRefinedMixed *s) {
    if (!s)
        return;

#ifndef CFD_REFINED_VERIFY_ILU
    cfd_sparse_mg_destroy(s->velocity_mg);
    cfd_sparse_mg_destroy(s->pressure_mg);
    cfd_memory_free(s->pressure_work);
#endif
    cfd_memory_free(s->row);
    cfd_memory_free(s->col);
    cfd_memory_free(s->diag);
    cfd_memory_free(s->fixed);
    cfd_memory_free(s->base);
    cfd_memory_free(s->a);
    cfd_memory_free(s->lu);
    cfd_memory_free(s->storage);
    cfd_memory_free(s);
}
CfdRefinedMixed *cfd_refined_mixed_create(const CfdRefinedMesh *m, double nu) {
    if (!m || !m->cells || !m->faces || !isfinite(nu) || nu <= 0)
        return NULL;
    CfdRefinedMixed *s = cfd_memory_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->m = m;
    s->nu = nu;
    s->nv = m->cell_count + m->face_count;
    s->n = 2 * s->nv + m->cell_count;
    s->mass = -1;
    Assembly a = {.nv = s->nv, .nu = nu};
    CfdRefinedDiffusion *d = cfd_refined_diffusion_create(m, NULL);
    if (!d)
        goto fail;
    bool okay = cfd_refined_diffusion_matrix_visit(d, collect, &a);
    cfd_refined_diffusion_destroy(d);
    if (!okay)
        goto fail;
    for (int f = 0; f < m->face_count; f++) {
        const CfdRefinedFace *face = &m->faces[f];
        int cells[2] = {face->lo, face->hi};
        int velocity = face->axis * s->nv + m->cell_count + f;
        for (int k = 0; k < 2; k++)
            if (cells[k] >= 0) {
                int pressure = 2 * s->nv + cells[k];
                double value = (k == 0 ? -1 : 1) * face->area;
                if (!add(&a, pressure, velocity, value) || !add(&a, velocity, pressure, value))
                    goto fail;
            }
        /* Zero pressure graph entries enrich the ILU pattern without altering
         * the saddle matrix; elimination can retain neighbor Schur entries. */
        if (cells[0] >= 0 && cells[1] >= 0)
            if (!add(&a, 2 * s->nv + cells[0], 2 * s->nv + cells[1], 0) ||
                !add(&a, 2 * s->nv + cells[1], 2 * s->nv + cells[0], 0))
                goto fail;
    }
    for (int i = 0; i < s->n; i++)
        if (!add(&a, i, i, 0))
            goto fail;
    qsort(a.e, a.used, sizeof(*a.e), compare);
    int count = 0;
    for (int i = 0; i < a.used; i++) {
        if (count && a.e[count - 1].row == a.e[i].row && a.e[count - 1].col == a.e[i].col)
            a.e[count - 1].value += a.e[i].value;
        else
            a.e[count++] = a.e[i];
    }
    s->nz = count;
    s->row = cfd_memory_calloc((size_t)s->n + 1, sizeof(int));
    s->col = cfd_memory_malloc((size_t)count * sizeof(int));
    s->diag = cfd_memory_malloc((size_t)s->n * sizeof(int));
    s->fixed = cfd_memory_calloc(s->n, 1);
    s->base = cfd_memory_malloc((size_t)count * sizeof(double));
    s->a = cfd_memory_malloc((size_t)count * sizeof(double));
#ifdef CFD_REFINED_VERIFY_ILU
    s->lu = cfd_memory_malloc((size_t)count * sizeof(double));
    if (!s->lu)
        goto fail;
#endif
    s->storage = cfd_memory_calloc((size_t)(RESTART + 7) * s->n, sizeof(double));
    if (!s->row || !s->col || !s->diag || !s->fixed || !s->base || !s->a || !s->storage)
        goto fail;
    for (int i = 0; i < count; i++) {
        s->row[a.e[i].row + 1]++;
        s->col[i] = a.e[i].col;
        s->base[i] = a.e[i].value;
        if (a.e[i].row == a.e[i].col)
            s->diag[a.e[i].row] = i;
    }
    for (int i = 0; i < s->n; i++)
        s->row[i + 1] += s->row[i];
    for (int f = 0; f < m->face_count; f++) {
        const CfdRefinedFace *p = &m->faces[f];
        bool boundary = p->lo < 0 || p->hi < 0;
        bool outlet = p->axis == 0 && p->hi < 0 && fabs(p->cx - m->nx * m->dx) < 1e-10 * m->dx;
        if (boundary && !outlet) {
            s->fixed[m->cell_count + f] = 1;
            s->fixed[s->nv + m->cell_count + f] = 1;
        }
    }
    s->x = s->storage;
    s->b = s->x + s->n;
    s->r = s->b + s->n;
    s->w = s->r + s->n;
    s->tmp = s->w + s->n;
    s->known = s->tmp + s->n;
    s->basis = s->known + s->n;
    cfd_memory_free(a.e);
    return s;
fail:
    cfd_memory_free(a.e);
    cfd_refined_mixed_destroy(s);
    return NULL;
}
#ifndef CFD_REFINED_VERIFY_ILU
static bool prepare_pressure_multilevel(CfdRefinedMixed *s) {
    int n = s->m->cell_count, offset = 2 * s->nv, nz = 0;
    int *row = cfd_memory_calloc((size_t)n + 1, sizeof(int)),
        *col = cfd_memory_malloc((size_t)(n + 2 * s->m->face_count) * sizeof(int));
    double *a = cfd_memory_calloc((size_t)n + 2 * s->m->face_count, sizeof(double)),
           *xy = cfd_memory_malloc((size_t)2 * n * sizeof(double));
    if (!row || !col || !a || !xy) {
        cfd_memory_free(row);
        cfd_memory_free(col);
        cfd_memory_free(a);
        cfd_memory_free(xy);
        return false;
    }
    for (int i = 0; i < n; i++) {
        for (int k = s->row[offset + i]; k < s->row[offset + i + 1]; k++)
            if (s->col[k] >= offset)
                col[nz++] = s->col[k] - offset;
        row[i + 1] = nz;
        xy[i] = s->m->cells[i].cx;
        xy[n + i] = s->m->cells[i].cy;
    }
    for (int f = 0; f < s->m->face_count; f++) {
        const CfdRefinedFace *face = &s->m->faces[f];
        if (s->fixed[face->axis * s->nv + n + f])
            continue;
        int cells[2] = {face->lo, face->hi};
        double distance = 0;
        for (int j = 0; j < 2; j++)
            if (cells[j] >= 0) {
                const CfdRefinedCell *c = &s->m->cells[cells[j]];
                distance += fabs(face->axis == 0 ? face->cx - c->cx : face->cy - c->cy);
            }
        double weight = face->area / distance;
        for (int j = 0; j < 2; j++)
            if (cells[j] >= 0) {
                int i = cells[j], other = cells[1 - j];
                for (int k = row[i]; k < row[i + 1]; k++) {
                    if (col[k] == i)
                        a[k] += weight;
                    else if (col[k] == other)
                        a[k] -= weight;
                }
            }
    }
    s->pressure_mg =
        cfd_sparse_mg_create(n, row, col, a, xy, xy + n, 4 * s->m->dx / s->m->lattice_scale,
                             4 * s->m->dy / s->m->lattice_scale);
    s->pressure_work = cfd_memory_malloc((size_t)2 * n * sizeof(double));
    cfd_memory_free(row);
    cfd_memory_free(col);
    cfd_memory_free(a);
    cfd_memory_free(xy);
    if (!s->pressure_mg || !s->pressure_work) {
        cfd_sparse_mg_destroy(s->pressure_mg);
        s->pressure_mg = NULL;
        cfd_memory_free(s->pressure_work);
        s->pressure_work = NULL;
        return false;
    }
    return true;
}
static bool prepare_multilevel(CfdRefinedMixed *s) {
    int n = s->nv, nz = 0;
    int *row = cfd_memory_calloc((size_t)n + 1, sizeof(int)), *col = cfd_memory_malloc((size_t)s->nz * sizeof(int));
    double *a = cfd_memory_malloc((size_t)s->nz * sizeof(double)),
           *xy = cfd_memory_malloc((size_t)2 * n * sizeof(double));
    if (!row || !col || !a || !xy) {
        cfd_memory_free(row);
        cfd_memory_free(col);
        cfd_memory_free(a);
        cfd_memory_free(xy);
        return false;
    }
    for (int i = 0; i < n; i++) {
        for (int k = s->row[i]; k < s->row[i + 1]; k++)
            if (s->col[k] < n) {
                col[nz] = s->col[k];
                a[nz++] = s->a[k];
            }
        row[i + 1] = nz;
        if (i < s->m->cell_count) {
            xy[i] = s->m->cells[i].cx;
            xy[n + i] = s->m->cells[i].cy;
        } else {
            xy[i] = s->m->faces[i - s->m->cell_count].cx;
            xy[n + i] = s->m->faces[i - s->m->cell_count].cy;
        }
    }
    cfd_sparse_mg_destroy(s->velocity_mg);
    s->velocity_mg =
        cfd_sparse_mg_create(n, row, col, a, xy, xy + n, 2 * s->m->dx / s->m->lattice_scale,
                             2 * s->m->dy / s->m->lattice_scale);
    cfd_memory_free(row);
    cfd_memory_free(col);
    cfd_memory_free(a);
    cfd_memory_free(xy);
    return s->velocity_mg && (s->pressure_mg || prepare_pressure_multilevel(s));
}
#endif
static bool prepare(CfdRefinedMixed *s, double mass) {
    if (mass == s->mass)
        return true;
    s->mass = -1; /* Invalidate before any factorization that can fail. */
    memcpy(s->a, s->base, (size_t)s->nz * sizeof(double));
    for (int c = 0; c < s->m->cell_count; c++) {
        double value = mass * s->m->cells[c].volume;
        s->a[s->diag[c]] += value;
        s->a[s->diag[s->nv + c]] += value;
    }
    for (int i = 0; i < s->n; i++)
        for (int k = s->row[i]; k < s->row[i + 1]; k++)
            if (s->fixed[i] || s->fixed[s->col[k]])
                s->a[k] = i == s->col[k] ? 1 : 0;
#ifndef CFD_REFINED_VERIFY_ILU
    if (!prepare_multilevel(s))
        return false;
#else
    memcpy(s->lu, s->a, (size_t)s->nz * sizeof(double));
    /* ILU(0) on the enriched graph. No numerical pivot replacement: a bad
     * factorization is rejected instead of silently changing the operator. */
    for (int i = 0; i < s->n; i++) {
        for (int k = s->row[i]; k < s->diag[i]; k++) {
            int j = s->col[k];
            double pivot = s->lu[s->diag[j]];
            if (!isfinite(pivot) || fabs(pivot) < 1e-30)
                return false;
            s->lu[k] /= pivot;
            double factor = s->lu[k];
            int pos = k + 1;
            for (int p = s->diag[j] + 1; p < s->row[j + 1]; p++) {
                while (pos < s->row[i + 1] && s->col[pos] < s->col[p])
                    pos++;
                if (pos < s->row[i + 1] && s->col[pos] == s->col[p])
                    s->lu[pos] -= factor * s->lu[p];
            }
        }
        if (!isfinite(s->lu[s->diag[i]]) || fabs(s->lu[s->diag[i]]) < 1e-30)
            return false;
    }
#endif
    s->mass = mass;
    return true;
}
static void apply(const CfdRefinedMixed *s, const double *x, double *y) {
    for (int i = 0; i < s->n; i++) {
        double value = 0;
        for (int k = s->row[i]; k < s->row[i + 1]; k++)
            value += s->a[k] * x[s->col[k]];
        y[i] = value;
    }
}
static void precondition(const CfdRefinedMixed *s, const double *x, double *y) {
#ifndef CFD_REFINED_VERIFY_ILU
    cfd_sparse_mg_apply(s->velocity_mg, x, y);
    cfd_sparse_mg_apply(s->velocity_mg, x + s->nv, y + s->nv);
    int nc = s->m->cell_count;
    double *rp = s->pressure_work, *zp = rp + nc;
    for (int c = 0; c < nc; c++) {
        int i = 2 * s->nv + c;
        double r = x[i];
        for (int k = s->row[i]; k < s->row[i + 1]; k++)
            if (s->col[k] < 2 * s->nv)
                r -= s->a[k] * y[s->col[k]];
        rp[c] = r;
    }
    /* Approximate S^{-1} = nu M_p^{-1} + mass L_p^{-1}.
     * L_p is a preconditioner only: the physical coupled matrix is unchanged.
     * Pressure block is negative in this symmetric saddle-point convention. */
    if (s->mass > 0)
        cfd_sparse_mg_apply(s->pressure_mg, rp, zp);
    for (int c = 0; c < nc; c++)
        y[2 * s->nv + c] =
            -s->nu * rp[c] / s->m->cells[c].volume - (s->mass > 0 ? s->mass * zp[c] : 0);
#else
    for (int i = 0; i < s->n; i++) {
        double value = x[i];
        for (int k = s->row[i]; k < s->diag[i]; k++)
            value -= s->lu[k] * y[s->col[k]];
        y[i] = value;
    }
    for (int i = s->n - 1; i >= 0; i--) {
        double value = y[i];
        for (int k = s->diag[i] + 1; k < s->row[i + 1]; k++)
            value -= s->lu[k] * y[s->col[k]];
        y[i] = value / s->lu[s->diag[i]];
    }
#endif
}

static double dot(const double *a, const double *b, int n) {
    double v = 0;
    for (int i = 0; i < n; i++)
        v += a[i] * b[i];
    return v;
}
static bool gmres(CfdRefinedMixed *s, CfdRefinedMixedReport *report) {
    int n = s->n;
    double normb = sqrt(dot(s->b, s->b, n)), tol = 1e-11 * fmax(normb, 1e-12);
    for (int restart = 0; restart < 100; restart++) {
        apply(s, s->x, s->tmp);
        for (int i = 0; i < n; i++)
            s->r[i] = s->b[i] - s->tmp[i];
        double residual = sqrt(dot(s->r, s->r, n));
        report->relative_residual = residual / fmax(normb, 1e-30);
        if (residual <= tol)
            return true;
        precondition(s, s->r, s->w);
        double beta = sqrt(dot(s->w, s->w, n));
        if (!isfinite(beta) || beta <= 0)
            return false;
        double h[RESTART + 1][RESTART] = {{0}}, cs[RESTART], sn[RESTART], g[RESTART + 1] = {0},
                           y[RESTART];
        g[0] = beta;
        for (int i = 0; i < n; i++)
            s->basis[i] = s->w[i] / beta;
        int used = 0;
        for (int j = 0; j < RESTART; j++) {
            apply(s, s->basis + (size_t)j * n, s->tmp);
            precondition(s, s->tmp, s->w);
            for (int pass = 0; pass < 2; pass++)
                for (int k = 0; k <= j; k++) {
                    double *v = s->basis + (size_t)k * n, t = dot(s->w, v, n);
                    h[k][j] += t;
                    for (int i = 0; i < n; i++)
                        s->w[i] -= t * v[i];
                }
            h[j + 1][j] = sqrt(dot(s->w, s->w, n));
            double next = h[j + 1][j];
            if (next > 1e-30)
                for (int i = 0; i < n; i++)
                    s->basis[(size_t)(j + 1) * n + i] = s->w[i] / next;
            for (int k = 0; k < j; k++) {
                double t = cs[k] * h[k][j] + sn[k] * h[k + 1][j];
                h[k + 1][j] = -sn[k] * h[k][j] + cs[k] * h[k + 1][j];
                h[k][j] = t;
            }
            double radius = hypot(h[j][j], h[j + 1][j]);
            if (!isfinite(radius) || radius <= 0)
                return false;
            cs[j] = h[j][j] / radius;
            sn[j] = h[j + 1][j] / radius;
            h[j][j] = radius;
            g[j + 1] = -sn[j] * g[j];
            g[j] *= cs[j];
            used = j + 1;
            report->iterations++;
#ifndef CFD_REFINED_VERIFY_RESTART_ONLY
            /* A left-preconditioned residual is not the acceptance norm. Check
             * the actual candidate periodically rather than always completing
             * a restart after the physical residual has already converged.
             * known is free scratch after boundary elimination; x stays intact
             * on an unsuccessful check so the Arnoldi basis remains valid. */
            if (used % 10 == 0 && used < RESTART) {
                for (int row = used - 1; row >= 0; row--) {
                    double value = g[row];
                    for (int col = row + 1; col < used; col++)
                        value -= h[row][col] * y[col];
                    y[row] = value / h[row][row];
                }
                memcpy(s->known, s->x, (size_t)n * sizeof(double));
                for (int col = 0; col < used; col++)
                    for (int i = 0; i < n; i++)
                        s->known[i] += y[col] * s->basis[(size_t)col * n + i];
                apply(s, s->known, s->tmp);
                for (int i = 0; i < n; i++)
                    s->r[i] = s->b[i] - s->tmp[i];
                double actual = sqrt(dot(s->r, s->r, n));
                if (actual <= tol) {
                    memcpy(s->x, s->known, (size_t)n * sizeof(double));
                    report->relative_residual = actual / fmax(normb, 1e-30);
                    return true;
                }
            }
#endif
            if (fabs(g[j + 1]) <= 1e-12 * beta || next <= 1e-30)
                break;
        }
        for (int i = used - 1; i >= 0; i--) {
            double t = g[i];
            for (int j = i + 1; j < used; j++)
                t -= h[i][j] * y[j];
            y[i] = t / h[i][i];
        }
        for (int j = 0; j < used; j++)
            for (int i = 0; i < n; i++)
                s->x[i] += y[j] * s->basis[(size_t)j * n + i];
    }
    return false;
}
bool cfd_refined_mixed_solve(CfdRefinedMixed *s, double mass, const double *rhs_u,
                             const double *rhs_v, const double *boundary_u,
                             const double *boundary_v, double *u, double *v, double *face_u,
                             double *face_v, double *pressure, CfdRefinedMixedReport *report) {
    if (!s || !rhs_u || !rhs_v || !boundary_u || !boundary_v || !u || !v || !face_u || !face_v ||
        !pressure || !report || !isfinite(mass) || mass < 0)
        return false;
    s->solved = false;
    *report = (CfdRefinedMixedReport){0};
    if (!prepare(s, mass))
        return false;
    const CfdRefinedMesh *m = s->m;
    int nc = m->cell_count, nf = m->face_count;
    memset(s->b, 0, (size_t)s->n * sizeof(double));
    memset(s->known, 0, (size_t)s->n * sizeof(double));
    for (int c = 0; c < nc; c++) {
        if (!isfinite(rhs_u[c]) || !isfinite(rhs_v[c]))
            return false;
        s->b[c] = rhs_u[c] * m->cells[c].volume;
        s->b[s->nv + c] = rhs_v[c] * m->cells[c].volume;
    }
    for (int f = 0; f < nf; f++)
        if (s->fixed[nc + f]) {
            if (!isfinite(boundary_u[f]) || !isfinite(boundary_v[f]))
                return false;
            s->known[nc + f] = boundary_u[f];
            s->known[s->nv + nc + f] = boundary_v[f];
        }
    for (int i = 0; i < s->n; i++) {
        if (s->fixed[i]) {
            s->b[i] = s->known[i];
            s->x[i] = s->known[i];
        } else
            for (int k = s->row[i]; k < s->row[i + 1]; k++)
                if (s->fixed[s->col[k]])
                    s->b[i] -= s->base[k] * s->known[s->col[k]];
    }
    if (!gmres(s, report))
        return false;
    memcpy(u, s->x, (size_t)nc * sizeof(double));
    memcpy(v, s->x + s->nv, (size_t)nc * sizeof(double));
    memcpy(face_u, s->x + nc, (size_t)nf * sizeof(double));
    memcpy(face_v, s->x + s->nv + nc, (size_t)nf * sizeof(double));
    memcpy(pressure, s->x + 2 * s->nv, (size_t)nc * sizeof(double));
    memset(s->r, 0, (size_t)nc * sizeof(double));
    for (int f = 0; f < nf; f++) {
        const CfdRefinedFace *p = &m->faces[f];
        double flux = (p->axis == 0 ? face_u[f] : face_v[f]) * p->area;
        if (p->lo >= 0)
            s->r[p->lo] += flux;
        if (p->hi >= 0)
            s->r[p->hi] -= flux;
    }
    for (int c = 0; c < nc; c++)
        report->divergence = fmax(report->divergence, fabs(s->r[c]) / m->cells[c].volume);
    s->solved = true;
    report->storage_bytes =
        sizeof(*s) + ((size_t)2 * s->n + 1 + s->nz) * sizeof(int) + (size_t)s->n +
        ((size_t)(s->lu ? 3 : 2) * s->nz + (size_t)(RESTART + 7) * s->n) * sizeof(double);
#ifndef CFD_REFINED_VERIFY_ILU
    report->storage_bytes += cfd_sparse_mg_bytes(s->velocity_mg) +
                             cfd_sparse_mg_bytes(s->pressure_mg) +
                             (size_t)2 * s->m->cell_count * sizeof(double);
#endif
    return true;
}

bool cfd_refined_mixed_boundary_force(const CfdRefinedMixed *s, int kind, double rho, double span,
                                      CfdRefinedForce *out) {
    if (!s || !s->solved || !out || !isfinite(rho) || rho <= 0 || !isfinite(span) || span <= 0 ||
        kind < -1 || kind > CFD_REFINED_SOLID)
        return false;
    *out = (CfdRefinedForce){0};
    const CfdRefinedMesh *m = s->m;
    for (int f = 0; f < m->face_count; f++) {
        const CfdRefinedFace *face = &m->faces[f];
        if (face->boundary == CFD_REFINED_INTERIOR || (kind >= 0 && kind != face->boundary))
            continue;
        int cell = face->lo >= 0 ? face->lo : face->hi;
        double normal = face->lo >= 0 ? 1 : -1;
        out->pressure[face->axis] += rho * span * s->x[2 * s->nv + cell] * normal * face->area;
        for (int axis = 0; axis < 2; axis++) {
            int row = axis * s->nv + m->cell_count + f;
            double viscous = 0;
            for (int k = s->row[row]; k < s->row[row + 1]; k++)
                if (s->col[k] < 2 * s->nv)
                    viscous += s->base[k] * s->x[s->col[k]];
            out->viscous[axis] -= rho * span * viscous;
        }
    }
    for (int axis = 0; axis < 2; axis++)
        out->total[axis] = out->pressure[axis] + out->viscous[axis];
    return true;
}

bool cfd_refined_mixed_energy(CfdRefinedMixed *s, double rho, double span, const double *ax,
                              const double *ay, CfdRefinedEnergy *out) {
    if (!s || !s->solved || !out || !isfinite(rho) || rho <= 0 || !isfinite(span) || span <= 0)
        return false;
    const CfdRefinedMesh *m = s->m;
    int nc = m->cell_count;
    double *ux = s->r, *uy = s->w, *vx = s->tmp, *vy = s->known;
    memset(ux, 0, (size_t)nc * sizeof(double));
    memset(uy, 0, (size_t)nc * sizeof(double));
    memset(vx, 0, (size_t)nc * sizeof(double));
    memset(vy, 0, (size_t)nc * sizeof(double));
    *out = (CfdRefinedEnergy){0};
    for (int f = 0; f < m->face_count; f++) {
        const CfdRefinedFace *face = &m->faces[f];
        int cells[2] = {face->lo, face->hi};
        double u = s->x[nc + f], v = s->x[s->nv + nc + f];
        for (int k = 0; k < 2; k++)
            if (cells[k] >= 0) {
                int c = cells[k];
                double factor = (k == 0 ? 1 : -1) * face->area / m->cells[c].volume;
                (face->axis == 0 ? ux : uy)[c] += factor * u;
                (face->axis == 0 ? vx : vy)[c] += factor * v;
            }
    }
    double gradient_dissipation = 0;
    for (int c = 0; c < nc; c++) {
        double u = s->x[c], v = s->x[s->nv + c], volume = m->cells[c].volume,
               force_x = ax ? ax[c] : 0, force_y = ay ? ay[c] : 0;
        if (!isfinite(force_x) || !isfinite(force_y))
            return false;
        out->kinetic_j += .5 * rho * span * volume * (u * u + v * v);
        out->body_force_power_w += rho * span * volume * (force_x * u + force_y * v);
        out->strain_dissipation_w +=
            rho * span * s->nu * volume *
            (2 * ux[c] * ux[c] + 2 * vy[c] * vy[c] + (uy[c] + vx[c]) * (uy[c] + vx[c]));
        gradient_dissipation += rho * span * s->nu * volume *
                                (ux[c] * ux[c] + uy[c] * uy[c] + vx[c] * vx[c] + vy[c] * vy[c]);
    }
    for (int i = 0; i < 2 * s->nv; i++) {
        double value = 0;
        for (int k = s->row[i]; k < s->row[i + 1]; k++)
            if (s->col[k] < 2 * s->nv)
                value += s->base[k] * s->x[s->col[k]];
        out->discrete_diffusion_w += rho * span * s->x[i] * value;
    }
    for (int f = 0; f < m->face_count; f++) {
        const CfdRefinedFace *face = &m->faces[f];
        if (face->boundary == CFD_REFINED_INTERIOR)
            continue;
        int c = face->lo >= 0 ? face->lo : face->hi;
        double normal = face->lo >= 0 ? 1 : -1;
        double u = s->x[nc + f], v = s->x[s->nv + nc + f];
        for (int axis = 0; axis < 2; axis++) {
            int row = axis * s->nv + nc + f;
            double reaction = 0;
            for (int k = s->row[row]; k < s->row[row + 1]; k++)
                reaction += s->base[k] * s->x[s->col[k]];
            out->weak_boundary_power_w += rho * span * (axis == 0 ? u : v) * reaction;
        }
        double cross = face->axis == 0 ? ux[c] * u + uy[c] * v : vx[c] * u + vy[c] * v;
        out->stress_boundary_power_w += rho * span * s->nu * normal * face->area * cross;
        out->outward_kinetic_flux_w +=
            .5 * rho * span * (u * u + v * v) * normal * face->area * (face->axis == 0 ? u : v);
    }
    out->stress_boundary_power_w += out->weak_boundary_power_w;
    out->stabilization_w = out->discrete_diffusion_w - gradient_dissipation;
    return true;
}
