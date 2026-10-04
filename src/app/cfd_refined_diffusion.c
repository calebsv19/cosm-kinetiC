#include "app/cfd_memory.h"
#include "app/cfd_refined_diffusion.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* Hybrid finite-volume energy: V|G|^2 + sum_f A_f/d_f R_f^2,
 * G=sum_f A_f(p_f-p_c)n_f/V, R_f=p_f-p_c-G.(x_f-x_c).
 * See Eymard/Gallouet/Herbin, arXiv:0801.1430, hybrid interfaces.
 * Shared face unknowns impose flux continuity; residual stabilization vanishes
 * for affine fields, including nonorthogonal coarse/fine neighbor centers. */
typedef struct {
    int count, id[9];
    double a[9][9];
} Local;
struct CfdRefinedDiffusion {
    const CfdRefinedMesh *mesh;
    int n;
    Local *local;
    unsigned char *fixed;
    double *storage, *diag, *b, *x, *r, *z, *p, *ap, *lift;
};
void cfd_refined_diffusion_destroy(CfdRefinedDiffusion *d) {
    if (!d)
        return;
    cfd_memory_free(d->local);
    cfd_memory_free(d->fixed);
    cfd_memory_free(d->storage);
    cfd_memory_free(d);
}
CfdRefinedDiffusion *cfd_refined_diffusion_create(const CfdRefinedMesh *m,
                                                  const unsigned char *dirichlet) {
    if (!m || !m->cells || !m->faces)
        return NULL;
    CfdRefinedDiffusion *d = cfd_memory_calloc(1, sizeof(*d));
    if (!d)
        return NULL;
    d->mesh = m;
    d->n = m->cell_count + m->face_count;
    d->local = cfd_memory_calloc(m->cell_count, sizeof(*d->local));
    d->fixed = cfd_memory_calloc(d->n, 1);
    d->storage = cfd_memory_calloc((size_t)8 * d->n, sizeof(double));
    if (!d->local || !d->fixed || !d->storage) {
        cfd_refined_diffusion_destroy(d);
        return NULL;
    }
    d->diag = d->storage;
    d->b = d->diag + d->n;
    d->x = d->b + d->n;
    d->r = d->x + d->n;
    d->z = d->r + d->n;
    d->p = d->z + d->n;
    d->ap = d->p + d->n;
    d->lift = d->ap + d->n;
    for (int c = 0; c < m->cell_count; c++) {
        d->local[c].count = 1;
        d->local[c].id[0] = c;
    }
    for (int f = 0; f < m->face_count; f++) {
        int neighbors[2] = {m->faces[f].lo, m->faces[f].hi};
        d->fixed[m->cell_count + f] =
            (neighbors[0] < 0 || neighbors[1] < 0) && (!dirichlet || dirichlet[f]);
        for (int s = 0; s < 2; s++)
            if (neighbors[s] >= 0) {
                Local *l = &d->local[neighbors[s]];
                if (l->count >= 9) {
                    cfd_refined_diffusion_destroy(d);
                    return NULL;
                }
                l->id[l->count++] = m->cell_count + f;
            }
    }
    int anchors = 0;
    for (int f = 0; f < m->face_count; f++)
        anchors += d->fixed[m->cell_count + f] != 0;
    if (!anchors) {
        cfd_refined_diffusion_destroy(d);
        return NULL;
    }
    for (int c = 0; c < m->cell_count; c++) {
        Local *l = &d->local[c];
        const CfdRefinedCell *cell = &m->cells[c];
        double gx[9] = {0}, gy[9] = {0};
        for (int k = 1; k < l->count; k++) {
            const CfdRefinedFace *f = &m->faces[l->id[k] - m->cell_count];
            double signed_area = (f->lo == c ? 1 : -1) * f->area;
            (f->axis == 0 ? gx : gy)[k] = signed_area / cell->volume;
            gx[0] -= gx[k];
            gy[0] -= gy[k];
        }
        for (int i = 0; i < l->count; i++)
            for (int j = 0; j < l->count; j++)
                l->a[i][j] = cell->volume * (gx[i] * gx[j] + gy[i] * gy[j]);
        for (int k = 1; k < l->count; k++) {
            const CfdRefinedFace *f = &m->faces[l->id[k] - m->cell_count];
            double rx = f->cx - cell->cx, ry = f->cy - cell->cy;
            double w = f->area / fabs(f->axis == 0 ? rx : ry), residual[9];
            for (int i = 0; i < l->count; i++)
                residual[i] = (i == k) - (i == 0) - gx[i] * rx - gy[i] * ry;
            for (int i = 0; i < l->count; i++)
                for (int j = 0; j < l->count; j++)
                    l->a[i][j] += w * residual[i] * residual[j];
        }
        for (int i = 0; i < l->count; i++)
            d->diag[l->id[i]] += l->a[i][i];
    }
    for (int i = 0; i < d->n; i++)
        if (d->fixed[i])
            d->diag[i] = 1;
    return d;
}
static void apply(const CfdRefinedDiffusion *d, const double *x, double *y, bool eliminate,
                  double mass, double diffusivity) {
    memset(y, 0, (size_t)d->n * sizeof(*y));
    for (int c = 0; c < d->mesh->cell_count; c++) {
        const Local *l = &d->local[c];
        for (int i = 0; i < l->count; i++) {
            if (eliminate && d->fixed[l->id[i]])
                continue;
            for (int j = 0; j < l->count; j++)
                if (!eliminate || !d->fixed[l->id[j]])
                    y[l->id[i]] += diffusivity * l->a[i][j] * x[l->id[j]];
        }
    }
    for (int c = 0; c < d->mesh->cell_count; c++)
        y[c] += mass * d->mesh->cells[c].volume * x[c];
    if (eliminate)
        for (int i = 0; i < d->n; i++)
            if (d->fixed[i])
                y[i] = x[i];
}
static double dot(const double *a, const double *b, int n) {
    double s = 0;
    for (int i = 0; i < n; i++)
        s += a[i] * b[i];
    return s;
}
bool cfd_refined_helmholtz_solve(CfdRefinedDiffusion *d, double mass, double diffusivity,
                                 const double *source, const double *boundary, double *cell_p,
                                 double *face_p, double *flux, CfdRefinedSolve *report) {
    if (!d || !source || !boundary || !cell_p || !face_p || !flux || !report || !isfinite(mass) ||
        mass < 0 || !isfinite(diffusivity) || diffusivity <= 0)
        return false;
    const CfdRefinedMesh *m = d->mesh;
    *report = (CfdRefinedSolve){0};
    memset(d->lift, 0, (size_t)d->n * sizeof(double));
    for (int f = 0; f < m->face_count; f++)
        if (d->fixed[m->cell_count + f]) {
            if (!isfinite(boundary[f]))
                return false;
            d->lift[m->cell_count + f] = boundary[f];
        }
    apply(d, d->lift, d->ap, false, mass, diffusivity);
    for (int i = 0; i < d->n; i++) {
        double rhs = i < m->cell_count ? source[i] * m->cells[i].volume : 0;
        if (i >= m->cell_count && !d->fixed[i]) {
            int fi = i - m->cell_count;
            if (m->faces[fi].lo < 0 || m->faces[fi].hi < 0)
                rhs = -boundary[fi] * m->faces[fi].area;
        }
        if (!isfinite(rhs))
            return false;
        d->b[i] = d->fixed[i] ? 0 : rhs - d->ap[i];
        d->x[i] = 0;
        d->r[i] = d->b[i];
        d->z[i] = d->r[i] / (d->fixed[i] ? 1
                                         : diffusivity * d->diag[i] +
                                               (i < m->cell_count ? mass * m->cells[i].volume : 0));
        d->p[i] = d->z[i];
    }
    double rz = dot(d->r, d->z, d->n), bnorm = sqrt(dot(d->b, d->b, d->n));
    double tol = 1e-12 * fmax(1, bnorm);
    for (int iteration = 0; iteration < 10000; iteration++) {
        if (sqrt(dot(d->r, d->r, d->n)) <= tol)
            break;
        apply(d, d->p, d->ap, true, mass, diffusivity);
        double denom = dot(d->p, d->ap, d->n);
        if (!(denom > 0) || !isfinite(denom))
            return false;
        double alpha = rz / denom;
        for (int i = 0; i < d->n; i++) {
            d->x[i] += alpha * d->p[i];
            d->r[i] -= alpha * d->ap[i];
            d->z[i] =
                d->r[i] / (d->fixed[i] ? 1
                                       : diffusivity * d->diag[i] +
                                             (i < m->cell_count ? mass * m->cells[i].volume : 0));
        }
        double next = dot(d->r, d->z, d->n), beta = next / rz;
        rz = next;
        for (int i = 0; i < d->n; i++)
            d->p[i] = d->z[i] + beta * d->p[i];
        report->iterations = iteration + 1;
    }
    apply(d, d->x, d->ap, true, mass, diffusivity);
    for (int i = 0; i < d->n; i++)
        d->r[i] = d->b[i] - d->ap[i];
    report->residual = sqrt(dot(d->r, d->r, d->n));
    if (!isfinite(report->residual) || report->residual > 4 * tol)
        return false;
    for (int i = 0; i < d->n; i++)
        d->x[i] += d->lift[i];
    memcpy(cell_p, d->x, (size_t)m->cell_count * sizeof(double));
    memcpy(face_p, d->x + m->cell_count, (size_t)m->face_count * sizeof(double));
    memset(flux, 0, (size_t)m->face_count * sizeof(double));
    memset(d->r, 0, (size_t)d->n * sizeof(double));
    for (int c = 0; c < m->cell_count; c++) {
        const Local *l = &d->local[c];
        for (int i = 1; i < l->count; i++) {
            int fi = l->id[i] - m->cell_count;
            const CfdRefinedFace *f = &m->faces[fi];
            double outward = 0;
            for (int j = 0; j < l->count; j++)
                outward -= diffusivity * l->a[i][j] * d->x[l->id[j]];
            d->r[fi] += outward;
            flux[fi] += (f->lo == c ? 1 : -1) * outward / f->area;
        }
    }
    for (int f = 0; f < m->face_count; f++)
        if (m->faces[f].lo >= 0 && m->faces[f].hi >= 0) {
            flux[f] *= .5;
            report->flux_mismatch = fmax(report->flux_mismatch, fabs(d->r[f]));
        }
    return true;
}

bool cfd_refined_diffusion_solve(CfdRefinedDiffusion *d, const double *source,
                                 const double *boundary, double *cell_p, double *face_p,
                                 double *flux, CfdRefinedSolve *report) {
    return cfd_refined_helmholtz_solve(d, 0, 1, source, boundary, cell_p, face_p, flux, report);
}

bool cfd_refined_diffusion_matrix_visit(const CfdRefinedDiffusion *d, CfdRefinedMatrixEntry entry,
                                        void *context) {
    if (!d || !entry)
        return false;
    for (int c = 0; c < d->mesh->cell_count; c++) {
        const Local *l = &d->local[c];
        for (int i = 0; i < l->count; i++)
            for (int j = 0; j < l->count; j++)
                if (l->a[i][j] != 0 && !entry(l->id[i], l->id[j], l->a[i][j], context))
                    return false;
    }
    return true;
}
