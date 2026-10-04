#include "app/cfd_memory.h"
#include "app/cfd_refined_transport.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct CfdRefinedTransport {
    const CfdRefinedMesh *m;
    double *store, *xx, *xy, *yy, *gx, *gy, *low, *high, *limit, *rate;
};
void cfd_refined_transport_destroy(CfdRefinedTransport *t) {
    if (t) {
        cfd_memory_free(t->store);
        cfd_memory_free(t);
    }
}
static void displacement(const CfdRefinedMesh *m, int c, int neighbor, const CfdRefinedFace *f,
                         double *dx, double *dy, double *weight) {
    *dx = (neighbor >= 0 ? m->cells[neighbor].cx : f->cx) - m->cells[c].cx;
    *dy = (neighbor >= 0 ? m->cells[neighbor].cy : f->cy) - m->cells[c].cy;
    *weight = f->area / (*dx * *dx + *dy * *dy);
}
CfdRefinedTransport *cfd_refined_transport_create(const CfdRefinedMesh *m) {
    if (!m || !m->cells || !m->faces)
        return NULL;
    CfdRefinedTransport *t = cfd_memory_calloc(1, sizeof(*t));
    if (!t)
        return NULL;
    t->m = m;
    t->store = cfd_memory_calloc((size_t)9 * m->cell_count, sizeof(double));
    if (!t->store) {
        cfd_memory_free(t);
        return NULL;
    }
    t->xx = t->store;
    t->xy = t->xx + m->cell_count;
    t->yy = t->xy + m->cell_count;
    t->gx = t->yy + m->cell_count;
    t->gy = t->gx + m->cell_count;
    t->low = t->gy + m->cell_count;
    t->high = t->low + m->cell_count;
    t->limit = t->high + m->cell_count;
    t->rate = t->limit + m->cell_count;
    for (int fi = 0; fi < m->face_count; fi++) {
        const CfdRefinedFace *f = &m->faces[fi];
        int cells[2] = {f->lo, f->hi};
        for (int k = 0; k < 2; k++)
            if (cells[k] >= 0) {
                int c = cells[k];
                double x, y, w;
                displacement(m, c, cells[1 - k], f, &x, &y, &w);
                t->xx[c] += w * x * x;
                t->xy[c] += w * x * y;
                t->yy[c] += w * y * y;
            }
    }
    for (int c = 0; c < m->cell_count; c++) {
        double det = t->xx[c] * t->yy[c] - t->xy[c] * t->xy[c];
        if (!(det > 0) || !isfinite(det)) {
            cfd_refined_transport_destroy(t);
            return NULL;
        }
        double a = t->xx[c];
        t->xx[c] = t->yy[c] / det;
        t->yy[c] = a / det;
        t->xy[c] = -t->xy[c] / det;
    }
    return t;
}
bool cfd_refined_transport_rhs(CfdRefinedTransport *t, const double *q, const double *velocity,
                               const double *boundary, double *rhs, double *max_rate,
                               double *outward_flux) {
    if (!t || !q || !velocity || !boundary || !rhs || !max_rate || !outward_flux)
        return false;
    const CfdRefinedMesh *m = t->m;
    memset(t->gx, 0, (size_t)2 * m->cell_count * sizeof(double));
    memset(t->rate, 0, (size_t)m->cell_count * sizeof(double));
    memset(rhs, 0, (size_t)m->cell_count * sizeof(double));
    for (int c = 0; c < m->cell_count; c++) {
        if (!isfinite(q[c]))
            return false;
        t->low[c] = t->high[c] = q[c];
        t->limit[c] = 1;
    }
    for (int fi = 0; fi < m->face_count; fi++) {
        const CfdRefinedFace *f = &m->faces[fi];
        int cells[2] = {f->lo, f->hi};
        if (!isfinite(velocity[fi]) || ((f->lo < 0 || f->hi < 0) && !isfinite(boundary[fi])))
            return false;
        for (int k = 0; k < 2; k++)
            if (cells[k] >= 0) {
                int c = cells[k], other = cells[1 - k];
                double sample = other >= 0 ? q[other] : boundary[fi], x, y, w;
                displacement(m, c, other, f, &x, &y, &w);
                t->gx[c] += w * x * (sample - q[c]);
                t->gy[c] += w * y * (sample - q[c]);
                t->low[c] = fmin(t->low[c], sample);
                t->high[c] = fmax(t->high[c], sample);
            }
    }
    for (int c = 0; c < m->cell_count; c++) {
        double x = t->gx[c], y = t->gy[c];
        t->gx[c] = t->xx[c] * x + t->xy[c] * y;
        t->gy[c] = t->xy[c] * x + t->yy[c] * y;
    }
    /* Barth-Jespersen face bounds on the least-squares reconstruction. */
    for (int fi = 0; fi < m->face_count; fi++) {
        const CfdRefinedFace *f = &m->faces[fi];
        int cells[2] = {f->lo, f->hi};
        for (int k = 0; k < 2; k++)
            if (cells[k] >= 0) {
                int c = cells[k];
                double delta =
                    t->gx[c] * (f->cx - m->cells[c].cx) + t->gy[c] * (f->cy - m->cells[c].cy);
                if (delta > 0)
                    t->limit[c] = fmin(t->limit[c], (t->high[c] - q[c]) / delta);
                if (delta < 0)
                    t->limit[c] = fmin(t->limit[c], (t->low[c] - q[c]) / delta);
            }
    }
    *max_rate = 0;
    *outward_flux = 0;
    for (int fi = 0; fi < m->face_count; fi++) {
        const CfdRefinedFace *f = &m->faces[fi];
        int up = velocity[fi] >= 0 ? f->lo : f->hi;
        double face_q = up < 0 ? boundary[fi]
                               : q[up] + t->limit[up] * (t->gx[up] * (f->cx - m->cells[up].cx) +
                                                         t->gy[up] * (f->cy - m->cells[up].cy));
        double volume_flux = velocity[fi] * f->area, flux = volume_flux * face_q;
        if (f->lo >= 0) {
            rhs[f->lo] -= flux;
            t->rate[f->lo] += fmax(0, volume_flux);
        } else
            *outward_flux -= flux;
        if (f->hi >= 0) {
            rhs[f->hi] += flux;
            t->rate[f->hi] += fmax(0, -volume_flux);
        } else
            *outward_flux += flux;
    }
    for (int c = 0; c < m->cell_count; c++) {
        rhs[c] /= m->cells[c].volume;
        *max_rate = fmax(*max_rate, t->rate[c] / m->cells[c].volume);
    }
    return true;
}
