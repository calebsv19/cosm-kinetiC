#include "app/cfd_pressure_mg.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifndef CFD_MG_SWEEPS
#define CFD_MG_SWEEPS 1
#endif
typedef struct Level {
    int nx, ny, n;
    double *diag, *east, *north, *x, *b, *r;
    struct Level *coarse;
} Level;
struct CfdPressureMG {
    Level *fine;
};
size_t cfd_pressure_mg_storage_bytes(int nx, int ny) {
    size_t bytes = sizeof(CfdPressureMG);
    if (nx < 2 || ny < 2)
        return 0;
    for (;;) {
        bytes += sizeof(Level) + (size_t)nx * ny * 6 * sizeof(double);
        if (nx <= 2 && ny <= 2)
            break;
        nx = (nx + 1) / 2;
        ny = (ny + 1) / 2;
    }
    return bytes;
}
static void destroy(Level *l) {
    if (!l)
        return;
    destroy(l->coarse);
    free(l->diag);
    free(l);
}
static Level *create_level(int nx, int ny) {
    Level *l = calloc(1, sizeof(*l));
    if (!l)
        return NULL;
    l->nx = nx;
    l->ny = ny;
    l->n = nx * ny;
    l->diag = calloc((size_t)l->n * 6, sizeof(double));
    if (!l->diag) {
        free(l);
        return NULL;
    }
    l->east = l->diag + l->n;
    l->north = l->east + l->n;
    l->x = l->north + l->n;
    l->b = l->x + l->n;
    l->r = l->b + l->n;
    return l;
}
static void multiply(Level *l, const double *x, double *y) {
    for (int j = 0; j < l->ny; j++)
        for (int i = 0; i < l->nx; i++) {
            int k = j * l->nx + i;
            double a = l->diag[k] * x[k];
            if (i + 1 < l->nx)
                a -= l->east[k] * x[k + 1];
            if (i > 0)
                a -= l->east[k - 1] * x[k - 1];
            if (j + 1 < l->ny)
                a -= l->north[k] * x[k + l->nx];
            if (j > 0)
                a -= l->north[k - l->nx] * x[k - l->nx];
            y[k] = a;
        }
}
static int parent(Level *l, int i, int j) { return (j / 2) * ((l->nx + 1) / 2) + i / 2; }
static bool coarsen(Level *l) {
    if (l->nx <= 2 && l->ny <= 2)
        return true;
    Level *c = create_level((l->nx + 1) / 2, (l->ny + 1) / 2);
    if (!c)
        return false;
    l->coarse = c;
    /* P is aggregate injection; R=P^T. Assemble A_c=P^T A_f P exactly,
     * retaining boundary anchors and cancelling internal aggregate edges. */
    for (int j = 0; j < l->ny; j++)
        for (int i = 0; i < l->nx; i++) {
            int k = j * l->nx + i, p = parent(l, i, j);
            c->diag[p] += l->diag[k];
            if (i + 1 < l->nx) {
                int q = parent(l, i + 1, j);
                double w = l->east[k];
                if (p == q)
                    c->diag[p] -= 2 * w;
                else
                    c->east[p] += w;
            }
            if (j + 1 < l->ny) {
                int q = parent(l, i, j + 1);
                double w = l->north[k];
                if (p == q)
                    c->diag[p] -= 2 * w;
                else
                    c->north[p] += w;
            }
        }
    return coarsen(c);
}
CfdPressureMG *cfd_pressure_mg_create(int nx, int ny, double dx, double dy,
                                      const unsigned char *solid) {
    if (nx < 2 || ny < 2 || !isfinite(dx) || !isfinite(dy) || dx <= 0 || dy <= 0)
        return NULL;
    CfdPressureMG *m = calloc(1, sizeof(*m));
    if (!m)
        return NULL;
    Level *l = m->fine = create_level(nx, ny);
    if (!l) {
        free(m);
        return NULL;
    }
    double sx = 1 / (dx * dx), sy = 1 / (dy * dy);
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++) {
            int k = j * nx + i;
            if (solid && solid[k]) {
                l->diag[k] = 1;
                continue;
            }
            if (i + 1 == nx)
                l->diag[k] += 2 * sx;
            if (i + 1 < nx && (!solid || !solid[k + 1])) {
                l->east[k] = sx;
                l->diag[k] += sx;
                l->diag[k + 1] += sx;
            }
            if (j + 1 < ny && (!solid || !solid[k + nx])) {
                l->north[k] = sy;
                l->diag[k] += sy;
                l->diag[k + nx] += sy;
            }
        }
    if (!coarsen(l)) {
        cfd_pressure_mg_destroy(m);
        return NULL;
    }
    return m;
}
void cfd_pressure_mg_destroy(CfdPressureMG *m) {
    if (m) {
        destroy(m->fine);
        free(m);
    }
}
static void smooth(Level *l, bool reverse) {
    /* Forward/backward Gauss-Seidel pairing makes the V cycle symmetric. */
    for (int a = 0; a < l->n; a++) {
        int k = reverse ? l->n - 1 - a : a, i = k % l->nx, j = k / l->nx;
        double b = l->b[k];
        if (i + 1 < l->nx)
            b += l->east[k] * l->x[k + 1];
        if (i > 0)
            b += l->east[k - 1] * l->x[k - 1];
        if (j + 1 < l->ny)
            b += l->north[k] * l->x[k + l->nx];
        if (j > 0)
            b += l->north[k - l->nx] * l->x[k - l->nx];
        l->x[k] = b / l->diag[k];
    }
}
static void cycle(Level *l) {
    memset(l->x, 0, (size_t)l->n * sizeof(double));
    if (!l->coarse) {
        /* A symmetric fixed number of sweeps, never a residual-dependent
         * nonlinear preconditioner inside standard conjugate gradients. */
        for (int k = 0; k < 12; k++)
            smooth(l, false);
        for (int k = 0; k < 12; k++)
            smooth(l, true);
        return;
    }
    for (int k = 0; k < CFD_MG_SWEEPS; k++)
        smooth(l, false);
    multiply(l, l->x, l->r);
    Level *c = l->coarse;
    memset(c->b, 0, (size_t)c->n * sizeof(double));
    for (int j = 0; j < l->ny; j++)
        for (int i = 0; i < l->nx; i++) {
            int k = j * l->nx + i;
            c->b[parent(l, i, j)] += l->b[k] - l->r[k];
        }
    cycle(c);
    for (int j = 0; j < l->ny; j++)
        for (int i = 0; i < l->nx; i++)
            l->x[j * l->nx + i] += c->x[parent(l, i, j)];
    for (int k = 0; k < CFD_MG_SWEEPS; k++)
        smooth(l, true);
}
bool cfd_pressure_mg_apply(CfdPressureMG *m, const double *rhs, double *solution) {
    if (!m || !rhs || !solution)
        return false;
    memcpy(m->fine->b, rhs, (size_t)m->fine->n * sizeof(double));
    cycle(m->fine);
    for (int k = 0; k < m->fine->n; k++)
        if (!isfinite(m->fine->x[k]))
            return false;
    memcpy(solution, m->fine->x, (size_t)m->fine->n * sizeof(double));
    return true;
}
