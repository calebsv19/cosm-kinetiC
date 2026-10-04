#include "app/cfd_cartesian3d.h"
#include "app/cfd_sparse_mg.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
bool cfd_cartesian3d_init(CfdCartesian3d *g, const int n[3], const double length[3]) {
    if (!g || !n || !length)
        return false;
    CfdCartesian3d v = {0};
    size_t count = 1;
    for (int a = 0; a < 3; a++) {
        if (n[a] < 4 || n[a] > 256 || !isfinite(length[a]) || length[a] < .001 || length[a] > 1000)
            return false;
        v.n[a] = n[a];
        v.length[a] = length[a];
        v.h[a] = length[a] / n[a];
        count *= (size_t)n[a];
    }
    if (count > 1048576)
        return false;
    v.count = (int)count;
    v.volume = v.h[0] * v.h[1] * v.h[2];
    for (int a = 0; a < 3; a++)
        v.area[a] = v.volume / v.h[a];
    *g = v;
    return true;
}
int cfd_cartesian3d_neighbor(const CfdCartesian3d *g, int q, int axis, int offset) {
    int stride = axis == 0 ? 1 : axis == 1 ? g->n[0] : g->n[0] * g->n[1];
    int c = (q / stride) % g->n[axis];
    int d = (c + offset) % g->n[axis];
    if (d < 0)
        d += g->n[axis];
    return q + (d - c) * stride;
}
void cfd_cartesian3d_position(const CfdCartesian3d *g, int q, int face_axis, double xyz[3]) {
    int stride = 1;
    for (int a = 0; a < 3; a++) {
        xyz[a] = ((q / stride) % g->n[a] + (a == face_axis ? 0 : .5)) * g->h[a];
        stride *= g->n[a];
    }
}
void cfd_cartesian3d_divergence(const CfdCartesian3d *g, const double *v, double *out) {
    int n = g->count;
    for (int q = 0; q < n; q++) {
        out[q] = 0;
        for (int a = 0; a < 3; a++)
            out[q] += (v[a * n + cfd_cartesian3d_neighbor(g, q, a, 1)] - v[a * n + q]) / g->h[a];
    }
}
void cfd_cartesian3d_gradient(const CfdCartesian3d *g, const double *p, double *out) {
    for (int a = 0; a < 3; a++)
        for (int q = 0; q < g->count; q++)
            out[a * g->count + q] = (p[q] - p[cfd_cartesian3d_neighbor(g, q, a, -1)]) / g->h[a];
}
struct CfdCartesian3dLinear {
    int n, *row, *col;
    double *value, *work;
    CfdSparseMg *mg;
};
void cfd_cartesian3d_linear_destroy(CfdCartesian3dLinear *s) {
    if (!s)
        return;
    cfd_sparse_mg_destroy(s->mg);
    cfd_memory_free(s->row);
    cfd_memory_free(s->col);
    cfd_memory_free(s->value);
    cfd_memory_free(s->work);
    cfd_memory_free(s);
}
CfdCartesian3dLinear *cfd_cartesian3d_linear_create(const CfdCartesian3d *g, const bool periodic[3],
                                                    double mass, double viscosity, bool pin) {
    if (!g || !periodic || !isfinite(mass) || mass < 0 || !isfinite(viscosity) || viscosity <= 0 ||
        (pin && (mass != 0 || !periodic[0] || !periodic[1] || !periodic[2])) ||
        (!pin && mass == 0 && periodic[0] && periodic[1] && periodic[2]))
        return NULL;
    int n = g->count;
    CfdCartesian3dLinear *s = cfd_memory_calloc(1, sizeof(*s));
    double *coords = NULL;
    if (!s)
        return NULL;
    s->n = n;
    s->row = cfd_memory_calloc((size_t)n + 1, sizeof(int));
    s->col = cfd_memory_malloc((size_t)7 * n * sizeof(int));
    s->value = cfd_memory_malloc((size_t)7 * n * sizeof(double));
    s->work = cfd_memory_calloc((size_t)5 * n, sizeof(double));
    coords = cfd_memory_malloc((size_t)3 * n * sizeof(double));
    if (!s->row || !s->col || !s->value || !s->work || !coords)
        goto fail;
    int used = 0;
    for (int q = 0; q < n; q++) {
        double xyz[3];
        cfd_cartesian3d_position(g, q, -1, xyz);
        for (int a = 0; a < 3; a++)
            coords[a * n + q] = xyz[a];
        int col[7], count = 1;
        double val[7];
        col[0] = q;
        val[0] = mass;
        if (pin && q == 0)
            val[0] = 1;
        else
            for (int a = 0; a < 3; a++) {
                int stride = a == 0 ? 1 : a == 1 ? g->n[0] : g->n[0] * g->n[1];
                int c = (q / stride) % g->n[a];
                double k = viscosity / (g->h[a] * g->h[a]);
                for (int d = -1; d <= 1; d += 2) {
                    if (!periodic[a] && (c + d < 0 || c + d >= g->n[a]))
                        val[0] += 2 * k;
                    else {
                        int t = cfd_cartesian3d_neighbor(g, q, a, d);
                        val[0] += k;
                        if (!(pin && t == 0)) {
                            col[count] = t;
                            val[count++] = -k;
                        }
                    }
                }
            }
        for (int i = 1; i < count; i++) {
            int c = col[i];
            double v = val[i];
            int j = i;
            while (j > 0 && col[j - 1] > c) {
                col[j] = col[j - 1];
                val[j] = val[j - 1];
                j--;
            }
            col[j] = c;
            val[j] = v;
        }
        for (int i = 0; i < count; i++) {
            s->col[used] = col[i];
            s->value[used++] = val[i];
        }
        s->row[q + 1] = used;
    }
    s->mg = cfd_sparse_mg_create3d(n, s->row, s->col, s->value, coords, coords + n, coords + 2 * n,
                                   2 * g->h[0], 2 * g->h[1], 2 * g->h[2]);
    cfd_memory_free(coords);
    coords = NULL;
    if (!s->mg)
        goto fail;
    return s;
fail:
    cfd_memory_free(coords);
    cfd_cartesian3d_linear_destroy(s);
    return NULL;
}
void cfd_cartesian3d_linear_apply(const CfdCartesian3dLinear *s, const double *x, double *b) {
    for (int q = 0; q < s->n; q++) {
        double v = 0;
        for (int k = s->row[q]; k < s->row[q + 1]; k++)
            v += s->value[k] * x[s->col[k]];
        b[q] = v;
    }
}
static double dot(const double *a, const double *b, int n) {
    double v = 0;
    for (int i = 0; i < n; i++)
        v += a[i] * b[i];
    return v;
}
bool cfd_cartesian3d_linear_solve(CfdCartesian3dLinear *s, const double *b, double *x,
                                  int *iterations, double *relative_residual) {
    int n = s->n;
    double *r = s->work, *z = r + n, *p = z + n, *ap = p + n, *truth = ap + n;
    cfd_cartesian3d_linear_apply(s, x, r);
    for (int q = 0; q < n; q++)
        r[q] = b[q] - r[q];
    double scale = sqrt(dot(b, b, n));
    if (scale < 1e-30)
        scale = 1;
    double rho = 0;
    int it = 0;
    for (; it <= 2000; it++) {
        double rel = sqrt(dot(r, r, n)) / scale;
        if (rel <= 5e-12 || it == 2000) {
            cfd_cartesian3d_linear_apply(s, x, truth);
            for (int q = 0; q < n; q++)
                truth[q] = b[q] - truth[q];
            rel = sqrt(dot(truth, truth, n)) / scale;
            if (iterations)
                *iterations = it;
            if (relative_residual)
                *relative_residual = rel;
            if (isfinite(rel) && rel <= 1e-11)
                return true;
            if (it == 2000)
                return false;
            memcpy(r, truth, (size_t)n * sizeof(double));
            rho = 0;
        }
        cfd_sparse_mg_apply(s->mg, r, z);
        double next = dot(r, z, n);
        if (!isfinite(next) || next <= 0)
            return false;
        double beta = rho > 0 ? next / rho : 0;
        for (int q = 0; q < n; q++)
            p[q] = z[q] + beta * p[q];
        rho = next;
        cfd_cartesian3d_linear_apply(s, p, ap);
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
