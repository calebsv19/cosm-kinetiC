/* Stationary solid topology and pressure correction for the evolving plume.
 * Solid pressure rows are identity; fluid/solid faces carry homogeneous Neumann.
 * No prescribed force/traction or thermal exchange is inferred from this mask. */
#include "app/cfd_open_atmosphere3d.h"
#include <math.h>
#include <string.h>

bool cfd_open_atmosphere3d_box(CfdOpenAtmosphere3d *s, const int lo[3], const int hi[3]) {
    if (!s || !s->storage || s->steps || s->solid_box || !lo || !hi || !s->ground) return false;
    for (int a = 0; a < 3; a++)
        if (lo[a] < 1 || hi[a] <= lo[a] || hi[a] >= s->grid.n[a]) return false;
    if(!s->solid_work)s->solid_work = cfd_memory_calloc((size_t)5 * s->grid.count, sizeof(double));
    if (!s->solid_work) return false;
    memcpy(s->solid_lo, lo, sizeof(s->solid_lo));
    memcpy(s->solid_hi, hi, sizeof(s->solid_hi));
    s->solid_box = true;
    return true;
}

bool cfd_open_atmosphere3d_solid(const CfdOpenAtmosphere3d *s, int q) {
    if (!s->solid_box || q < 0 || q >= s->grid.count) return false;
    int p[3] = {q % s->grid.n[0], (q / s->grid.n[0]) % s->grid.n[1], q / s->plane};
    for (int a = 0; a < 3; a++) if (p[a] < s->solid_lo[a] || p[a] >= s->solid_hi[a]) return false;
    return true;
}

bool cfd_open_atmosphere3d_blocked(const CfdOpenAtmosphere3d *s, int a, int i, int j, int k) {
    if (!s->solid_box || k < 0 || k >= s->grid.n[2]) return false;
    int nx = s->grid.n[0], ny = s->grid.n[1];
    i = (i % nx + nx) % nx; j = (j % ny + ny) % ny;
    int q = (k * ny + j) * nx + i;
    int left = a == 2 ? q - s->plane : cfd_cartesian3d_neighbor(&s->grid, q, a, -1);
    return cfd_open_atmosphere3d_solid(s, q) || cfd_open_atmosphere3d_solid(s, left);
}

static double row(const CfdOpenAtmosphere3d *s, const double *x, int q, double *diagonal) {
    if (cfd_open_atmosphere3d_solid(s, q)) { *diagonal = 1; return x[q]; }
    double value = 0; *diagonal = 0;
    for (int a = 0; a < 3; a++) for (int side = -1; side <= 1; side += 2) {
        double w = 1 / (s->grid.h[a] * s->grid.h[a]);
        int other;
        int stride=(int[]){1,s->grid.n[0],s->plane}[a],coord=(q/stride)%s->grid.n[a];
        if(s->lateral_open && a<2 && ((side<0 && coord==0) || (side>0 && coord==s->grid.n[a]-1))) {value+=2*w*x[q];*diagonal+=2*w;continue;}
        if (a == 2 && ((side < 0 && q < s->plane) || (side > 0 && q >= s->grid.count - s->plane))) {
            if (side > 0 || !s->ground) { value += 2 * w * x[q]; *diagonal += 2 * w; }
            continue;
        }
        other = a == 2 ? q + side * s->plane : cfd_cartesian3d_neighbor(&s->grid, q, a, side);
        if (!cfd_open_atmosphere3d_solid(s, other)) { value += w * (x[q] - x[other]); *diagonal += w; }
    }
    return value;
}

static double dot(const double *a, const double *b, int n) {
    double sum = 0; for (int q = 0; q < n; q++) sum += a[q] * b[q]; return sum;
}

bool cfd_open_atmosphere3d_box_pressure(CfdOpenAtmosphere3d *s, double *residual) {
    int n = s->grid.count;
    double *r = s->solid_work, *z = r + n, *p = z + n, *ap = p + n, *diag = ap + n;
    double norm = sqrt(dot(s->rhs, s->rhs, n));
    /* A zero-RHS solve has a unique zero solution through the open top. */
    if (norm == 0) { memset(s->candidate_pressure, 0, (size_t)n * sizeof(double)); *residual = 0; return true; }
    if(s->cache_projection && !s->solid_projection) {
        s->solid_projection=cfd_open_atmosphere3d_box_preconditioner(s);
        if(!s->solid_projection)return false;
    }
    for (int q = 0; q < n; q++) {
        r[q] = s->rhs[q] - row(s, s->candidate_pressure, q, diag + q);
        if (!(diag[q] > 0)) return false;
        z[q] = r[q] / diag[q]; p[q] = z[q];
    }
    if(s->solid_projection){cfd_sparse_mg_apply(s->solid_projection,r,z);memcpy(p,z,(size_t)n*sizeof(double));}
    double rz = dot(r, z, n);
    for (int iteration = 0; iteration < 4000; iteration++) {
        if (sqrt(dot(r, r, n)) <= 1e-12 * norm) break;
        for (int q = 0; q < n; q++) {
            if(s->solid_projection){ap[q]=0;for(int j=s->solid_row[q];j<s->solid_row[q+1];j++)ap[q]+=s->solid_values[j]*p[s->solid_col[j]];}
            else {double unused;ap[q]=row(s,p,q,&unused);}
        }
        double pap = dot(p, ap, n);
        if (!isfinite(pap) || pap <= 0 || !isfinite(rz)) return false;
        double alpha = rz / pap;
        for (int q = 0; q < n; q++) { s->candidate_pressure[q] += alpha * p[q]; r[q] -= alpha * ap[q]; z[q] = r[q] / diag[q]; }
        if(s->solid_projection)cfd_sparse_mg_apply(s->solid_projection,r,z);
        double next = dot(r, z, n), beta = next / rz;
        for (int q = 0; q < n; q++) p[q] = z[q] + beta * p[q];
        rz = next;
    }
    /* Acceptance uses the true operator residual, independent of CG recurrence. */
    for (int q = 0; q < n; q++) { double unused; r[q] = s->rhs[q] - row(s, s->candidate_pressure, q, &unused); }
    *residual = sqrt(dot(r, r, n)) / norm;
    return isfinite(*residual) && *residual < 1e-11;
}
