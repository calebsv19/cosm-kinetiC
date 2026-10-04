#include "app/cfd_open3d.h"
#include "app/cfd_duct3d.h"
#include "app/cfd_sparse_mg.h"
#include <math.h>
#include <string.h>
#include <time.h>

typedef struct {
    int n, shape[3], *row, *col;
    double *a, *work;
    CfdSparseMg *mg;
} Momentum;
struct CfdOpen3dImpl {
    Momentum h[3];
    int offset[3], count;
    double *u, *f, *vwork, *pwork, *inlet;
};
static double dot(const double *a, const double *b, int n) {
    double v = 0;
    for (int q = 0; q < n; q++)
        v += a[q] * b[q];
    return v;
}
static int index3(int i, int j, int k, const int n[3]) { return (k * n[1] + j) * n[0] + i; }
/* Analytic average of the continuous Fourier reference on a Y/Z face patch.
 * This is an inlet boundary, never a pressure or interior solution assignment. */
static double inlet_response(double h, double w, double y, double z, double dy, double dz) {
    const double pi = 3.14159265358979323846;
    double sum = 0;
    for (int t = 0; t < 512; t++) {
        double n = 2 * t + 1, k = n * pi / h, a = k * dy / 2;
        double sinavg = sin(k * y) * sin(a) / a;
        double ratio = (exp(-k * (z - dz / 2)) - exp(-k * (z + dz / 2)) +
                        exp(-k * (w - z - dz / 2)) - exp(-k * (w - z + dz / 2))) /
                       (k * dz * (1 + exp(-k * w)));
        sum += sinavg * (1 - ratio) / (n * n * n);
    }
    return 4 * h * h / (pi * pi * pi) * sum;
}
static void apply(const Momentum *h, const double *x, double *b) {
    for (int q = 0; q < h->n; q++) {
        double v = 0;
        for (int e = h->row[q]; e < h->row[q + 1]; e++)
            v += h->a[e] * x[h->col[e]];
        b[q] = v;
    }
}
static bool inverse(Momentum *h, const double *b, double *x, int *total) {
    int n = h->n;
    double *r = h->work, *z = r + n, *p = z + n, *ap = p + n, *truth = ap + n;
    memset(x, 0, (size_t)n * sizeof(double));
    memcpy(r, b, (size_t)n * sizeof(double));
    double scale = sqrt(dot(b, b, n)), rho = 0;
    if (scale == 0)
        return true;
    for (int it = 0; it <= 1500; it++) {
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
static bool build(CfdOpen3d *s, int axis) {
    CfdOpen3dImpl *m = s->impl;
    Momentum *h = &m->h[axis];
    const CfdCartesian3d *g = &s->grid;
    for (int a = 0; a < 3; a++)
        h->shape[a] = g->n[a] - (a == axis && axis != 0);
    h->n = h->shape[0] * h->shape[1] * h->shape[2];
    h->row = cfd_memory_calloc((size_t)h->n + 1, sizeof(int));
    h->col = cfd_memory_malloc((size_t)7 * h->n * sizeof(int));
    h->a = cfd_memory_malloc((size_t)7 * h->n * sizeof(double));
    h->work = cfd_memory_calloc((size_t)5 * h->n, sizeof(double));
    double *coords = cfd_memory_malloc((size_t)3 * h->n * sizeof(double));
    if (!h->row || !h->col || !h->a || !h->work || !coords) {
        cfd_memory_free(coords);
        return false;
    }
    int used = 0;
    for (int k = 0; k < h->shape[2]; k++)
        for (int j = 0; j < h->shape[1]; j++)
            for (int i = 0; i < h->shape[0]; i++) {
                int c[3] = {i, j, k}, q = index3(i, j, k, h->shape), col[7] = {q}, count = 1;
                double val[7] = {0};
                for (int a = 0; a < 3; a++)
                    coords[a * h->n + q] = (c[a] + (a == axis ? 1 : .5)) * g->h[a];
                double vol = g->volume * (axis == 0 && i == g->n[0] - 1 ? .5 : 1);
                for (int a = 0; a < 3; a++)
                    for (int side = -1; side <= 1; side += 2) {
                        double coeff = s->viscosity * vol / (g->h[a] * g->h[a]);
                        /* The outlet-normal dual volume is half-sized, but its interior
                         * X connection crosses a full dx: identical conductance both ways. */
                        if (axis == 0 && a == 0)
                            coeff = s->viscosity * g->area[0] / g->h[0];
                        if (c[a] + side >= 0 && c[a] + side < h->shape[a]) {
                            int stride = a == 0   ? 1
                                         : a == 1 ? h->shape[0]
                                                  : h->shape[0] * h->shape[1];
                            val[0] += coeff;
                            col[count] = q + side * stride;
                            val[count++] = -coeff;
                        } else {
                            if (a == 0 && side == 1)
                                continue; /* natural vector traction */
                            double factor = a == axis ? 1 : 2;
                            val[0] += factor * coeff;
                            if (axis == 0 && a == 0 && side == -1)
                                m->f[m->offset[axis] + q] += coeff * m->inlet[k * g->n[1] + j];
                        }
                    }
                if (axis == 0 && i == g->n[0] - 1)
                    m->f[m->offset[axis] + q] -= s->outlet_datum_pa * g->area[0];
                for (int a = 1; a < count; a++) {
                    int c0 = col[a], b = a;
                    double v = val[a];
                    while (b > 0 && col[b - 1] > c0) {
                        col[b] = col[b - 1];
                        val[b] = val[b - 1];
                        b--;
                    }
                    col[b] = c0;
                    val[b] = v;
                }
                for (int a = 0; a < count; a++) {
                    h->col[used] = col[a];
                    h->a[used++] = val[a];
                }
                h->row[q + 1] = used;
            }
    h->mg = cfd_sparse_mg_create3d(h->n, h->row, h->col, h->a, coords, coords + h->n,
                                   coords + 2 * h->n, 2 * g->h[0], 2 * g->h[1], 2 * g->h[2]);
    cfd_memory_free(coords);
    return h->mg != NULL;
}
void cfd_open3d_destroy(CfdOpen3d *s) {
    if (!s)
        return;
    if (s->impl) {
        for (int a = 0; a < 3; a++) {
            Momentum *h = &s->impl->h[a];
            cfd_sparse_mg_destroy(h->mg);
            cfd_memory_free(h->row);
            cfd_memory_free(h->col);
            cfd_memory_free(h->a);
            cfd_memory_free(h->work);
        }
        cfd_memory_free(s->impl->u);
        cfd_memory_free(s->impl->f);
        cfd_memory_free(s->impl->vwork);
        cfd_memory_free(s->impl->pwork);
        cfd_memory_free(s->impl->inlet);
        cfd_memory_free(s->impl);
    }
    cfd_memory_free(s->velocity);
    cfd_memory_free(s->pressure);
    cfd_memory_free(s->outlet_velocity);
    s->impl = NULL;
    s->velocity = s->pressure = s->outlet_velocity = NULL;
    s->solved = false;
}
bool cfd_open3d_init(CfdOpen3d *s, const int n[3], const double length[3], double rho, double mu,
                     double flow, double datum) {
    if (!s)
        return false;
    memset(s, 0, sizeof(*s));
    clock_t start = clock();
    if (!cfd_cartesian3d_init(&s->grid, n, length) || !isfinite(rho) || rho <= 0 || !isfinite(mu) ||
        mu <= 0 || !isfinite(flow) || flow <= 0 || !isfinite(datum))
        return false;
    s->density = rho;
    s->viscosity = mu;
    s->requested_flow = flow;
    s->outlet_datum_pa = datum;
    s->impl = cfd_memory_calloc(1, sizeof(*s->impl));
    if (!s->impl)
        return false;
    CfdOpen3dImpl *m = s->impl;
    int N = s->grid.count;
    m->offset[1] = N;
    m->offset[2] = N + n[0] * (n[1] - 1) * n[2];
    m->count = m->offset[2] + n[0] * n[1] * (n[2] - 1);
    m->u = cfd_memory_calloc(m->count, sizeof(double));
    m->f = cfd_memory_calloc(m->count, sizeof(double));
    m->vwork = cfd_memory_calloc((size_t)2 * m->count, sizeof(double));
    m->pwork = cfd_memory_calloc((size_t)5 * N, sizeof(double));
    m->inlet = cfd_memory_malloc((size_t)n[1] * n[2] * sizeof(double));
    s->velocity = cfd_memory_calloc((size_t)3 * N, sizeof(double));
    s->pressure = cfd_memory_calloc(N, sizeof(double));
    s->outlet_velocity = cfd_memory_calloc((size_t)n[1] * n[2], sizeof(double));
    if (!m->u || !m->f || !m->vwork || !m->pwork || !m->inlet || !s->velocity || !s->pressure ||
        !s->outlet_velocity)
        goto fail;
    double G =
        mu * flow / (length[1] * length[2] * cfd_duct3d_reference_mean(length[1], length[2], 512));
    for (int k = 0; k < n[2]; k++)
        for (int j = 0; j < n[1]; j++)
            m->inlet[k * n[1] + j] =
                G / mu *
                inlet_response(length[1], length[2], (j + .5) * s->grid.h[1],
                               (k + .5) * s->grid.h[2], s->grid.h[1], s->grid.h[2]);
    for (int a = 0; a < 3; a++)
        if (!build(s, a))
            goto fail;
    s->setup_cpu_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    return true;
fail:
    cfd_open3d_destroy(s);
    return false;
}
/* Integrated B and its EXACT transpose: no separately reconstructed pressure gradient. */
static void transpose(const CfdOpen3d *s, const double *p, double *v) {
    const CfdCartesian3d *g = &s->grid;
    const CfdOpen3dImpl *m = s->impl;
    memset(v, 0, (size_t)m->count * sizeof(double));
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int q = index3(i, j, k, g->n);
                v[index3(i, j, k, m->h[0].shape)] += g->area[0] * p[q];
                if (i > 0)
                    v[index3(i - 1, j, k, m->h[0].shape)] -= g->area[0] * p[q];
                if (j < g->n[1] - 1)
                    v[m->offset[1] + index3(i, j, k, m->h[1].shape)] += g->area[1] * p[q];
                if (j > 0)
                    v[m->offset[1] + index3(i, j - 1, k, m->h[1].shape)] -= g->area[1] * p[q];
                if (k < g->n[2] - 1)
                    v[m->offset[2] + index3(i, j, k, m->h[2].shape)] += g->area[2] * p[q];
                if (k > 0)
                    v[m->offset[2] + index3(i, j, k - 1, m->h[2].shape)] -= g->area[2] * p[q];
            }
}
static void divergence(const CfdOpen3d *s, const double *v, double *p) {
    const CfdCartesian3d *g = &s->grid;
    const CfdOpen3dImpl *m = s->impl;
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                double sum = g->area[0] * v[index3(i, j, k, m->h[0].shape)];
                if (i > 0)
                    sum -= g->area[0] * v[index3(i - 1, j, k, m->h[0].shape)];
                if (j < g->n[1] - 1)
                    sum += g->area[1] * v[m->offset[1] + index3(i, j, k, m->h[1].shape)];
                if (j > 0)
                    sum -= g->area[1] * v[m->offset[1] + index3(i, j - 1, k, m->h[1].shape)];
                if (k < g->n[2] - 1)
                    sum += g->area[2] * v[m->offset[2] + index3(i, j, k, m->h[2].shape)];
                if (k > 0)
                    sum -= g->area[2] * v[m->offset[2] + index3(i, j, k - 1, m->h[2].shape)];
                p[index3(i, j, k, g->n)] = sum;
            }
}
static bool hinv(CfdOpen3d *s, const double *b, double *u) {
    for (int a = 0; a < 3; a++)
        if (!inverse(&s->impl->h[a], b + s->impl->offset[a], u + s->impl->offset[a],
                     &s->velocity_iterations))
            return false;
    return true;
}
static bool schur(CfdOpen3d *s, const double *p, double *out) {
    double *b = s->impl->vwork, *u = b + s->impl->count;
    transpose(s, p, b);
    if (!hinv(s, b, u))
        return false;
    divergence(s, u, out);
    return true;
}
double cfd_open3d_face(const CfdOpen3d *s, int axis, int i, int j, int k) {
    const CfdCartesian3d *g = &s->grid;
    const CfdOpen3dImpl *m = s->impl;
    int c[3] = {i, j, k};
    if (axis == 0 && i == 0)
        return m->inlet[k * g->n[1] + j];
    if (axis > 0 && (c[axis] == 0 || c[axis] == g->n[axis]))
        return 0;
    c[axis]--;
    return m->u[m->offset[axis] + index3(c[0], c[1], c[2], m->h[axis].shape)];
}
static double center(const CfdOpen3d *s, int axis, int i, int j, int k) {
    int c[3] = {i, j, k}, plus[3] = {i, j, k};
    plus[axis]++;
    return .5 * (cfd_open3d_face(s, axis, c[0], c[1], c[2]) +
                 cfd_open3d_face(s, axis, plus[0], plus[1], plus[2]));
}
void cfd_open3d_derivatives(const CfdOpen3d *s, int i, int j, int k, double d[3][3]) {
    int c[3] = {i, j, k};
    const CfdCartesian3d *g = &s->grid;
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) {
            if (a == b) {
                int hi[3] = {i, j, k};
                hi[b]++;
                d[a][b] =
                    (cfd_open3d_face(s, a, hi[0], hi[1], hi[2]) - cfd_open3d_face(s, a, i, j, k)) /
                    g->h[b];
                continue;
            }
            int lo[3] = {i, j, k}, hi[3] = {i, j, k};
            lo[b]--;
            hi[b]++;
            double uc = center(s, a, i, j, k), um, up;
            if (c[b] == 0)
                um = b == 0 ? 2 * (a == 0 ? s->impl->inlet[k * g->n[1] + j] : 0) - uc : -uc;
            else
                um = center(s, a, lo[0], lo[1], lo[2]);
            if (c[b] == g->n[b] - 1)
                up = b == 0 ? uc : -uc;
            else
                up = center(s, a, hi[0], hi[1], hi[2]);
            d[a][b] = (up - um) / (2 * g->h[b]);
        }
}
static void observe(CfdOpen3d *s) {
    const CfdCartesian3d *g = &s->grid;
    CfdOpen3dImpl *m = s->impl;
    double error = 0, norm = 0,
           refG = s->viscosity * s->requested_flow /
                  (g->length[1] * g->length[2] *
                   cfd_duct3d_reference_mean(g->length[1], g->length[2], 512));
    double pfirst = 0, psecond = 0, plast = 0, pprev = 0, xsum = 0, psum = 0, xx = 0, xp = 0,
           fitn = 0;
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++) {
            double exact = m->inlet[k * g->n[1] + j];
            s->inlet_flow += exact * g->area[0];
            s->outlet_flow += cfd_open3d_face(s, 0, g->n[0], j, k) * g->area[0];
            for (int i = 0; i < g->n[0]; i++) {
                int q = index3(i, j, k, g->n);
                double u = center(s, 0, i, j, k), d[3][3];
                /* Error uses the actual staggered faces and dual-volume quadrature. */
                double uf = cfd_open3d_face(s, 0, i + 1, j, k), weight = i == g->n[0] - 1 ? .5 : 1;
                error += weight * (uf - exact) * (uf - exact) * g->volume;
                norm += exact * exact * g->volume;
                if (j < g->n[1] - 1) {
                    double v = cfd_open3d_face(s, 1, i, j + 1, k);
                    error += v * v * g->volume;
                }
                if (k < g->n[2] - 1) {
                    double w = cfd_open3d_face(s, 2, i, j, k + 1);
                    error += w * w * g->volume;
                }
                cfd_open3d_derivatives(s, i, j, k, d);
                for (int a = 0; a < 3; a++)
                    for (int b = 0; b < 3; b++) {
                        double strain = .5 * (d[a][b] + d[b][a]);
                        s->dissipation_w += 2 * s->viscosity * strain * strain * g->volume;
                    }
                s->max_divergence = fmax(s->max_divergence, fabs(d[0][0] + d[1][1] + d[2][2]));
                if (j == 0)
                    s->wall_force_n[0] += s->viscosity * 2 * u / g->h[1] * g->area[1];
                if (j == g->n[1] - 1)
                    s->wall_force_n[1] += s->viscosity * 2 * u / g->h[1] * g->area[1];
                if (k == 0)
                    s->wall_force_n[2] += s->viscosity * 2 * u / g->h[2] * g->area[2];
                if (k == g->n[2] - 1)
                    s->wall_force_n[3] += s->viscosity * 2 * u / g->h[2] * g->area[2];
                double p = s->pressure[q], x = (i + .5) * g->h[0];
                if (i == 0)
                    pfirst += p;
                if (i == 1)
                    psecond += p;
                if (i == g->n[0] - 1)
                    plast += p;
                if (i == g->n[0] - 2)
                    pprev += p;
                if (x >= 1 && x <= 3) {
                    xsum += x;
                    psum += p;
                    xx += x * x;
                    xp += x * p;
                    fitn++;
                }
                /* Independently reconstructed physical symmetric stress work.
                 * Inlet/outlet transverse velocities have zero inlet value / natural
                 * derivative. Normal strain and solved pressure use one-sided traces. */
                if (i == 0) {
                    double pnext = s->pressure[index3(1, j, k, g->n)];
                    double pin = (3 * p - pnext) / 2;
                    double ux = (cfd_open3d_face(s, 0, 1, j, k) - exact) / g->h[0];
                    s->boundary_power_w += (pin - 2 * s->viscosity * ux) * exact * g->area[0];
                }
                if (i == g->n[0] - 1) {
                    double pout = (3 * p - s->pressure[index3(i - 1, j, k, g->n)]) / 2;
                    double uout = cfd_open3d_face(s, 0, g->n[0], j, k), ux = d[0][0];
                    double vout = center(s, 1, i, j, k), wout = center(s, 2, i, j, k);
                    s->boundary_power_w +=
                        ((-pout + 2 * s->viscosity * ux) * uout + s->viscosity * d[0][1] * vout +
                         s->viscosity * d[0][2] * wout) *
                        g->area[0];
                }
            }
        }
    double count = g->n[1] * g->n[2];
    s->pressure_in_pa = (3 * pfirst - psecond) / (2 * count);
    s->pressure_out_pa = (3 * plast - pprev) / (2 * count);
    s->pressure_drop_pa = s->pressure_in_pa - s->pressure_out_pa;
    s->upstream_gradient_pa_m =
        fitn > 1 ? -(xp - xsum * psum / fitn) / (xx - xsum * xsum / fitn) : NAN;
    s->velocity_relative_l2 = sqrt(error / norm);
    s->pressure_relative_error =
        fabs(s->pressure_drop_pa - refG * g->length[0]) / (refG * g->length[0]);
    s->energy_relative_error = fabs(s->dissipation_w - refG * g->length[0] * s->requested_flow) /
                               (refG * g->length[0] * s->requested_flow);
    s->energy_imbalance = fabs(s->boundary_power_w - s->dissipation_w) / s->dissipation_w;
    s->flux_relative_error = fabs(s->inlet_flow - s->outlet_flow) / s->inlet_flow;
    double walls[4];
    cfd_duct3d_reference_walls(g->length[1], g->length[2], walls, 4096);
    for (int a = 0; a < 4; a++)
        s->wall_relative_error[a] = fabs(s->wall_force_n[a] - refG * g->length[0] * walls[a]) /
                                    (refG * g->length[0] * walls[a]);
    /* Matrix work remains separate from physical strain. Add prescribed inlet
     * edges and transverse inlet half-dual wall terms to recover full |grad u|. */
    double *temp = m->vwork;
    for (int a = 0; a < 3; a++) {
        Momentum *h = &m->h[a];
        apply(h, m->u + m->offset[a], temp);
        s->discrete_diffusion_w += dot(m->u + m->offset[a], temp, h->n);
    }
    double coeff = s->viscosity * g->area[0] / g->h[0];
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++) {
            double uin = m->inlet[k * g->n[1] + j], u1 = cfd_open3d_face(s, 0, 1, j, k);
            s->discrete_diffusion_w += coeff * (uin * uin - 2 * uin * u1);
            /* Complete the prescribed half inlet slab's Y/Z gradient work. */
            for (int a = 1; a < 3; a++) {
                int c = a == 1 ? j : k, st = a == 1 ? 1 : g->n[1];
                double edge = s->viscosity * .5 * g->volume / (g->h[a] * g->h[a]);
                if (c < g->n[a] - 1) {
                    double du = m->inlet[k * g->n[1] + j + st] - uin;
                    s->discrete_diffusion_w += edge * du * du;
                }
                if (c == 0 || c == g->n[a] - 1)
                    s->discrete_diffusion_w += 2 * edge * uin * uin;
            }
        }
}
bool cfd_open3d_solve(CfdOpen3d *s) {
    if (!s || !s->impl)
        return false;
    if (s->solved)
        return true;
    clock_t start = clock();
    CfdOpen3dImpl *m = s->impl;
    const CfdCartesian3d *g = &s->grid;
    int n = g->count;
    double *r = m->pwork, *d = r + n, *sd = d + n, *b = sd + n, *truth = b + n;
    if (!hinv(s, m->f, m->u))
        return false;
    divergence(s, m->u, b);
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int q = index3(i, j, k, g->n);
                b[q] = (i == 0 ? g->area[0] * m->inlet[k * g->n[1] + j] : 0) - b[q];
            }
    memcpy(r, b, (size_t)n * sizeof(double));
    memcpy(d, r, (size_t)n * sizeof(double));
    double scale = sqrt(dot(b, b, n)), rr = dot(r, r, n);
    if (scale < 1e-30)
        return false;
    bool converged = false;
    for (int it = 0; it <= 1500; it++) {
        if (sqrt(rr) / scale < 2e-13 || it == 1500) {
            if (!schur(s, s->pressure, truth))
                return false;
            for (int q = 0; q < n; q++)
                truth[q] = b[q] - truth[q];
            double rel = sqrt(dot(truth, truth, n)) / scale;
            if (isfinite(rel) && rel <= 8e-13) {
                s->iterations = it;
                converged = true;
                break;
            }
            if (it == 1500)
                break;
            memcpy(r, truth, (size_t)n * sizeof(double));
            memcpy(d, r, (size_t)n * sizeof(double));
            rr = dot(r, r, n);
        }
        if (!schur(s, d, sd))
            return false;
        double denom = dot(d, sd, n);
        if (!isfinite(denom) || denom <= 0)
            return false;
        double alpha = rr / denom;
        for (int q = 0; q < n; q++) {
            s->pressure[q] += alpha * d[q];
            r[q] -= alpha * sd[q];
        }
        double next = dot(r, r, n), beta = next / rr;
        for (int q = 0; q < n; q++)
            d[q] = r[q] + beta * d[q];
        rr = next;
    }
    if (!converged)
        return false;
    transpose(s, s->pressure, m->vwork);
    for (int q = 0; q < m->count; q++)
        m->vwork[q] += m->f[q];
    if (!hinv(s, m->vwork, m->u))
        return false;
    double res = 0, den = 0;
    for (int a = 0; a < 3; a++) {
        int offset = m->offset[a];
        apply(&m->h[a], m->u + offset, m->vwork + m->count + offset);
        for (int q = 0; q < m->h[a].n; q++) {
            double b0 = m->vwork[offset + q], v = m->vwork[m->count + offset + q] - b0;
            res += v * v;
            den += b0 * b0;
        }
    }
    s->relative_residual = sqrt(res / fmax(den, 1e-60));
    if (!isfinite(s->relative_residual) || s->relative_residual > 1e-11)
        return false;
    for (int k = 0; k < g->n[2]; k++)
        for (int j = 0; j < g->n[1]; j++)
            for (int i = 0; i < g->n[0]; i++) {
                int q = index3(i, j, k, g->n);
                for (int a = 0; a < 3; a++)
                    s->velocity[a * n + q] = cfd_open3d_face(s, a, i, j, k);
                if (i == 0)
                    s->outlet_velocity[k * g->n[1] + j] = cfd_open3d_face(s, 0, g->n[0], j, k);
            }
    observe(s);
    if (!isfinite(s->max_divergence) || s->max_divergence >= 1e-8 || s->flux_relative_error > 1e-10)
        return false;
    s->solve_cpu_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    s->solved = true;
    return true;
}
bool cfd_open3d_gate(const CfdOpen3d *s) {
    if (!s || !s->solved)
        return false;
    bool ok = s->velocity_relative_l2 <= .01 && s->pressure_relative_error <= .01 &&
              s->energy_relative_error <= .02 && s->energy_imbalance <= .02 &&
              fabs(s->inlet_flow - s->requested_flow) / s->requested_flow <= .01 &&
              fabs(s->outlet_flow - s->requested_flow) / s->requested_flow <= .01 &&
              s->relative_residual <= 1e-11 && s->max_divergence < 1e-8 &&
              s->flux_relative_error <= 1e-10;
    for (int a = 0; a < 4; a++)
        ok = ok && s->wall_relative_error[a] <= .02;
    return ok;
}

#ifdef CFD_OPEN3D_VERIFY
/* Tests use non-solution vectors, so the duct's nearly one-component answer
 * cannot conceal an incorrect transverse B, missing outlet or asymmetric H. */
bool cfd_open3d_verify_operators(CfdOpen3d *s) {
    CfdOpen3dImpl *m = s->impl;
    int n = s->grid.count;
    double *v = m->vwork, *bt = v + m->count, *p = m->pwork, *bv = p + n;
    for (int q = 0; q < m->count; q++)
        v[q] = sin(.73 * q) + cos(.17 * q);
    for (int q = 0; q < n; q++)
        p[q] = sin(.29 * q) + cos(.41 * q);
    divergence(s, v, bv);
    transpose(s, p, bt);
    double left = dot(p, bv, n), right = dot(v, bt, m->count);
    if (fabs(left - right) > 1e-12 * fmax(1, fabs(left)))
        return false;
    for (int a = 0; a < 3; a++) {
        Momentum *h = &m->h[a];
        for (int q = 0; q < h->n; q++) {
            int last = -1;
            double diagonal = 0;
            for (int e = h->row[q]; e < h->row[q + 1]; e++) {
                int j = h->col[e];
                if (j <= last)
                    return false;
                last = j;
                if (j == q)
                    diagonal = h->a[e];
                else {
                    double reverse = 0;
                    for (int k = h->row[j]; k < h->row[j + 1]; k++)
                        if (h->col[k] == q)
                            reverse = h->a[k];
                    if (fabs(h->a[e] - reverse) > 1e-14 * fmax(1, fabs(h->a[e])))
                        return false;
                }
            }
            if (diagonal <= 0)
                return false;
        }
        apply(h, v + m->offset[a], bt + m->offset[a]);
        if (dot(v + m->offset[a], bt + m->offset[a], h->n) <= 0)
            return false;
    }
    memset(m->pwork, 0, (size_t)5 * n * sizeof(double));
    memset(m->vwork, 0, (size_t)2 * m->count * sizeof(double));
    return true;
}
#endif
