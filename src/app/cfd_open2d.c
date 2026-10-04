#include "app/cfd_open2d.h"
#include "app/cfd_momentum_flux.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double timestamp(void) {
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return t.tv_sec + 1e-9 * t.tv_nsec;
}
#ifndef CFD_OPEN2D_GRID_LIMIT
#define CFD_OPEN2D_GRID_LIMIT 64
#endif
static int U(const CfdOpen2D *c, int i, int j) { return j * (c->nx + 1) + i; }
static int P(const CfdOpen2D *c, int i, int j) { return j * c->nx + i; }
static bool solid(const CfdOpen2D *c, int i, int j) {
    return c->solid && i >= 0 && i < c->nx && j >= 0 && j < c->ny && c->solid[P(c, i, j)];
}
static bool blocked_u(const CfdOpen2D *c, int i, int j) {
    return solid(c, i - 1, j) || solid(c, i, j);
}
static bool blocked_v(const CfdOpen2D *c, int i, int j) {
    return solid(c, i, j - 1) || solid(c, i, j);
}
static double uv(const CfdOpen2D *c, int i, int j) {
    if (j < 0)
        return -uv(c, i, -j - 1);
    if (j >= c->ny)
        return -uv(c, i, 2 * c->ny - j - 1);
    if (i < 0)
        return 2 * uv(c, 0, j) - uv(c, -i, j);
    if (i > c->nx)
        return uv(c, c->nx, j);
    return c->u[U(c, i, j)];
}
static double vv(const CfdOpen2D *c, int i, int j) {
    if (j < 0)
        return -vv(c, i, -j);
    if (j > c->ny)
        return -vv(c, i, 2 * c->ny - j);
    if (i < 0)
        return -vv(c, -i - 1, j);
    if (i >= c->nx) {
        double speed = .5 * (uv(c, c->nx, j - 1) + uv(c, c->nx, j));
        return speed < 0 ? -vv(c, 2 * c->nx - i - 1, j) : vv(c, c->nx - 1, j);
    }
    return c->v[P(c, i, j)];
}
static double flux(double qm, double a, double b, double qp, double s) {
#if defined(CFD_OPEN2D_VERIFY_CENTERED)
    (void)qm;
    (void)qp;
    return .5 * s * (a + b);
#elif defined(CFD_OPEN2D_VERIFY_DONOR)
    (void)qm;
    (void)qp;
    return .5 * s * (a + b) - .5 * fabs(s) * (b - a);
#else
    return cfd_limited_momentum_flux(qm, a, b, qp, s);
#endif
}
static double divcell(const CfdOpen2D *c, int i, int j) {
    if (solid(c, i, j))
        return 0;
    return (c->u[U(c, i + 1, j)] - c->u[U(c, i, j)]) * c->nx / c->length +
           (c->v[P(c, i, j + 1)] - c->v[P(c, i, j)]) * c->ny / c->height;
}
static void apply(const CfdOpen2D *c, const double *p, double *a) {
    double sx = pow(c->nx / c->length, 2), sy = pow(c->ny / c->height, 2);
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++) {
            int k = P(c, i, j);
            if (solid(c, i, j)) {
                a[k] = p[k];
                continue;
            }
            a[k] = 0;
            if (i > 0 && !solid(c, i - 1, j))
                a[k] += (p[k] - p[k - 1]) * sx;
            if (i + 1 < c->nx && !solid(c, i + 1, j))
                a[k] += (p[k] - p[k + 1]) * sx;
            else if (i + 1 == c->nx)
                a[k] += 2 * p[k] * sx;
            if (j > 0 && !solid(c, i, j - 1))
                a[k] += (p[k] - p[k - c->nx]) * sy;
            if (j + 1 < c->ny && !solid(c, i, j + 1))
                a[k] += (p[k] - p[k + c->nx]) * sy;
        }
}
static bool project(CfdOpen2D *c, double dt) {
    int n = c->nx * c->ny;
    double rr = 0;
#ifndef CFD_OPEN2D_VERIFY_UNPRECONDITIONED
    if (!c->pressure_mg)
        c->pressure_mg =
            cfd_pressure_mg_create(c->nx, c->ny, c->length / c->nx, c->height / c->ny, c->solid);
    if (!c->pressure_mg)
        return false;
#endif
#ifdef CFD_OPEN2D_VERIFY_COLD_PRESSURE
    memset(c->p, 0, n * sizeof(double));
#else
    /* Previous physical pressure is a guess only. Solve the true new residual
     * and retain the independent final residual/divergence checks. */
    for (int k = 0; k < n; k++) {
        if (!isfinite(c->p[k]))
            return false;
        c->p[k] *= dt / c->rho;
    }
#endif
    apply(c, c->p, c->a);
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++) {
            int k = P(c, i, j);
            c->r[k] = c->d[k] = -divcell(c, i, j) - c->a[k];
            rr += c->r[k] * c->r[k];
        }
#ifndef CFD_OPEN2D_VERIFY_UNPRECONDITIONED
    if (rr > 1e-20) {
        if (!cfd_pressure_mg_apply(c->pressure_mg, c->r, c->z))
            return false;
    } else
        memcpy(c->z, c->r, n * sizeof(double));
#else
    memcpy(c->z, c->r, n * sizeof(double));
#endif
    double rz = 0;
    for (int k = 0; k < n; k++) {
        c->d[k] = c->z[k];
        rz += c->r[k] * c->z[k];
    }
    c->iterations = 0;
    for (int it = 0; it < 4096 && sqrt(rr) > 1e-10; it++) {
        apply(c, c->d, c->a);
        double da = 0;
        for (int k = 0; k < n; k++)
            da += c->d[k] * c->a[k];
        if (!isfinite(da) || da <= 0)
            return false;
        double alpha = rz / da, next = 0;
        for (int k = 0; k < n; k++) {
            c->p[k] += alpha * c->d[k];
            c->r[k] -= alpha * c->a[k];
            next += c->r[k] * c->r[k];
        }
#ifndef CFD_OPEN2D_VERIFY_UNPRECONDITIONED
        if (!cfd_pressure_mg_apply(c->pressure_mg, c->r, c->z))
            return false;
#else
        memcpy(c->z, c->r, n * sizeof(double));
#endif
        double next_rz = 0;
        for (int k = 0; k < n; k++)
            next_rz += c->r[k] * c->z[k];
        if (!isfinite(next_rz) || rz <= 0)
            return false;
        for (int k = 0; k < n; k++)
            c->d[k] = c->z[k] + next_rz / rz * c->d[k];
        rr = next;
        rz = next_rz;
        c->iterations = it + 1;
    }
    apply(c, c->p, c->a);
    c->residual = 0;
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++)
            c->residual = fmax(c->residual, fabs(c->a[P(c, i, j)] + divcell(c, i, j)));
    if (!isfinite(c->residual) || c->residual > 1e-8)
        return false;
    for (int j = 0; j < c->ny; j++) {
        for (int i = 1; i < c->nx; i++)
            if (!blocked_u(c, i, j))
                c->u[U(c, i, j)] -= (c->p[P(c, i, j)] - c->p[P(c, i - 1, j)]) * c->nx / c->length;
        c->u[U(c, c->nx, j)] += 2 * c->p[P(c, c->nx - 1, j)] * c->nx / c->length;
    }
    for (int j = 1; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++)
            if (!blocked_v(c, i, j))
                c->v[P(c, i, j)] -= (c->p[P(c, i, j)] - c->p[P(c, i, j - 1)]) * c->ny / c->height;
    for (int k = 0; k < n; k++)
        c->p[k] *= c->rho / dt;
    c->divergence = 0;
    c->inlet_flux = c->outlet_flux = c->inlet_pressure = c->outlet_backflow = 0;
    for (int j = 0; j < c->ny; j++) {
        c->inlet_flux += c->u[U(c, 0, j)] * c->height / c->ny * c->width;
        c->outlet_flux += c->u[U(c, c->nx, j)] * c->height / c->ny * c->width;
        c->outlet_backflow += fmax(0, -c->u[U(c, c->nx, j)]) * c->height / c->ny * c->width;
        c->inlet_pressure += (1.5 * c->p[P(c, 0, j)] - .5 * c->p[P(c, 1, j)]) / c->ny;
        for (int i = 0; i < c->nx; i++)
            c->divergence = fmax(c->divergence, fabs(divcell(c, i, j)));
    }
    return c->divergence < 1e-8;
}
bool cfd_open2d_init(CfdOpen2D *c, int nx, int ny, double length, double height, double width,
                     double rho, double mu, double inlet_mean) {
    if (!c || nx < 4 || nx > CFD_OPEN2D_GRID_LIMIT || ny < 4 || ny > CFD_OPEN2D_GRID_LIMIT ||
        !isfinite(length) || !isfinite(height) || !isfinite(width) || !isfinite(rho) ||
        !isfinite(mu) || !isfinite(inlet_mean) || length < .1 || height < .1 || width <= 0 ||
        rho <= 0 || mu <= 0 || inlet_mean <= 0 || rho * height * inlet_mean / mu > 100)
        return false;
    memset(c, 0, sizeof(*c));
    c->nx = nx;
    c->ny = ny;
    c->length = length;
    c->height = height;
    c->width = width;
    c->rho = rho;
    c->mu = mu;
    c->inlet_mean = inlet_mean;
    int n = nx * ny, nu = (nx + 1) * ny, nv = nx * (ny + 1);
    c->u = calloc(nu, sizeof(double));
    c->ut = calloc(nu, sizeof(double));
    c->v = calloc(nv, sizeof(double));
    c->vt = calloc(nv, sizeof(double));
    c->p = calloc(n, sizeof(double));
    c->r = calloc(n, sizeof(double));
    c->d = calloc(n, sizeof(double));
    c->a = calloc(n, sizeof(double));
    c->z = calloc(n, sizeof(double));
    if (!c->u || !c->ut || !c->v || !c->vt || !c->p || !c->r || !c->d || !c->a || !c->z) {
        cfd_open2d_destroy(c);
        return false;
    }
    /* Exact prescribed cell-centre parabola. Its discrete flux is disclosed,
     * not silently renormalized to the continuous mean. */
    for (int j = 0; j < ny; j++) {
        double y = (j + .5) / ny;
        for (int i = 0; i <= nx; i++)
            c->u[U(c, i, j)] = 6 * inlet_mean * y * (1 - y);
    }
    return true;
}
bool cfd_open2d_set_obstacle(CfdOpen2D *c, int x0, int y0, int x1, int y1) {
    if (!c || !c->u || c->solid || c->time != 0 || x0 < 2 || y0 < 2 || x1 > c->nx - 2 ||
        y1 > c->ny - 2 || x0 >= x1 || y0 >= y1)
        return false;
    c->solid = calloc(c->nx * c->ny, 1);
    if (!c->solid)
        return false;
    c->obstacle_x0 = x0;
    c->obstacle_y0 = y0;
    c->obstacle_x1 = x1;
    c->obstacle_y1 = y1;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++)
            c->solid[P(c, i, j)] = 1;
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i <= c->nx; i++)
            if (blocked_u(c, i, j))
                c->u[U(c, i, j)] = 0;
    for (int j = 0; j <= c->ny; j++)
        for (int i = 0; i < c->nx; i++)
            if (blocked_v(c, i, j))
                c->v[P(c, i, j)] = 0;
    return true;
}
void cfd_open2d_destroy(CfdOpen2D *c) {
    if (!c)
        return;
    free(c->u);
    free(c->ut);
    free(c->v);
    free(c->vt);
    free(c->p);
    free(c->r);
    free(c->d);
    free(c->a);
    free(c->solid);
    free(c->z);
    cfd_pressure_mg_destroy(c->pressure_mg);
    memset(c, 0, sizeof(*c));
}
bool cfd_open2d_step(CfdOpen2D *c, double dt) {
    if (!c || !c->u || !isfinite(dt) || dt <= 0)
        return false;
    double begin = timestamp();
    double dx = c->length / c->nx, dy = c->height / c->ny, nu = c->mu / c->rho, um = 0, vm = 0;
    for (int k = 0; k < (c->nx + 1) * c->ny; k++) {
        if (!isfinite(c->u[k]))
            return false;
        um = fmax(um, fabs(c->u[k]));
    }
    for (int k = 0; k < c->nx * (c->ny + 1); k++) {
        if (!isfinite(c->v[k]))
            return false;
        vm = fmax(vm, fabs(c->v[k]));
    }
    if (dt * (um / dx + vm / dy + 2 * nu * (1 / (dx * dx) + 1 / (dy * dy))) > .4)
        return false;
    memcpy(c->ut, c->u, (c->nx + 1) * c->ny * sizeof(double));
    memset(c->vt, 0, c->nx * (c->ny + 1) * sizeof(double));
    for (int j = 0; j < c->ny; j++)
        for (int i = 1; i < c->nx; i++) {
            if (blocked_u(c, i, j)) {
                c->ut[U(c, i, j)] = 0;
                continue;
            }
            double q = uv(c, i, j), l = uv(c, i - 1, j), r = uv(c, i + 1, j), b = uv(c, i, j - 1),
                   t = uv(c, i, j + 1);
            double vb = .5 * (vv(c, i - 1, j) + vv(c, i, j)),
                   vt = .5 * (vv(c, i - 1, j + 1) + vv(c, i, j + 1));
            double fr = flux(l, q, r, uv(c, i + 2, j), .5 * (q + r));
            double fl = flux(uv(c, i - 2, j), l, q, r, .5 * (l + q));
            double ft = flux(b, q, t, uv(c, i, j + 2), vt);
            double fb = flux(uv(c, i, j - 2), b, q, t, vb);
            double adv = (fr - fl) / dx + (ft - fb) / dy;
            if (c->solid) {
                bool left = blocked_u(c, i - 1, j), right = blocked_u(c, i + 1, j);
                bool bottom = blocked_u(c, i, j - 1), top = blocked_u(c, i, j + 1);
                adv = ((right ? 0 : fr) - (left ? 0 : fl)) / dx +
                      ((top ? 0 : ft) - (bottom ? 0 : fb)) / dy;
                if (left)
                    l = 0;
                if (right)
                    r = 0;
                if (bottom)
                    b = -.5 * (solid(c, i - 1, j - 1) + solid(c, i, j - 1)) * q;
                if (top)
                    t = -.5 * (solid(c, i - 1, j + 1) + solid(c, i, j + 1)) * q;
            }
            /* Integrate wall shear using the same quadratic no-slip derivative
             * as the physical budget: (9*u0-u1)/(3*dy). The equivalent ghost
             * -2*u0+u1/3 preserves a quadratic channel profile; odd reflection
             * introduces an O(dy^2) offset and an O(dy) reconstructed shear error.
             * Apply only to viscous diffusion after advective flux evaluation. */
            if (j == 0)
                b = -2 * q + t / 3;
            if (j + 1 == c->ny)
                t = -2 * q + b / 3;
            c->ut[U(c, i, j)] =
                q + dt * (-adv + nu * ((l - 2 * q + r) / (dx * dx) + (b - 2 * q + t) / (dy * dy)));
        }
    for (int j = 0; j < c->ny; j++) {
#ifndef CFD_OPEN2D_VERIFY_COPY_OUTLET
        /* Half-dual-volume momentum predictor at the pressure face.
         * Right normal diffusive flux is zero; the left flux spans dx. Signed
         * advective exit flux uses the boundary velocity. Projection still
         * solves the outlet normal velocity through the half-cell pressure BC. */
        int i = c->nx;
        double q = uv(c, i, j), l = uv(c, i - 1, j), b = uv(c, i, j - 1), t = uv(c, i, j + 1);
        double fl = flux(uv(c, i - 2, j), l, q, q, .5 * (l + q));
        double vb = vv(c, i, j), vt = vv(c, i, j + 1);
        double ft = flux(b, q, t, uv(c, i, j + 2), vt);
        double fb = flux(uv(c, i, j - 2), b, q, t, vb);
        double adv = 2 * (q * q - fl) / dx + (ft - fb) / dy;
        if (j == 0)
            b = -2 * q + t / 3;
        if (j + 1 == c->ny)
            t = -2 * q + b / 3;
        c->ut[U(c, i, j)] =
            q + dt * (-adv + nu * (-2 * (q - l) / (dx * dx) + (b - 2 * q + t) / (dy * dy)));
#else
        c->ut[U(c, c->nx, j)] = c->ut[U(c, c->nx - 1, j)];
#endif
    }
    for (int j = 1; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++) {
            if (blocked_v(c, i, j))
                continue;
            double q = vv(c, i, j), l = vv(c, i - 1, j), r = vv(c, i + 1, j), b = vv(c, i, j - 1),
                   t = vv(c, i, j + 1);
            double ul = .5 * (uv(c, i, j - 1) + uv(c, i, j)),
                   ur = .5 * (uv(c, i + 1, j - 1) + uv(c, i + 1, j));
            double fr = flux(l, q, r, vv(c, i + 2, j), ur);
            double fl = flux(vv(c, i - 2, j), l, q, r, ul);
            double ft = flux(b, q, t, vv(c, i, j + 2), .5 * (q + t));
            double fb = flux(vv(c, i, j - 2), b, q, t, .5 * (b + q));
            double adv = (fr - fl) / dx + (ft - fb) / dy;
            if (c->solid) {
                bool left = blocked_v(c, i - 1, j), right = blocked_v(c, i + 1, j);
                bool bottom = blocked_v(c, i, j - 1), top = blocked_v(c, i, j + 1);
                adv = ((right ? 0 : fr) - (left ? 0 : fl)) / dx +
                      ((top ? 0 : ft) - (bottom ? 0 : fb)) / dy;
                if (left)
                    l = -.5 * (solid(c, i - 1, j - 1) + solid(c, i - 1, j)) * q;
                if (right)
                    r = -.5 * (solid(c, i + 1, j - 1) + solid(c, i + 1, j)) * q;
                if (bottom)
                    b = 0;
                if (top)
                    t = 0;
            }
            c->vt[P(c, i, j)] =
                q + dt * (-adv + nu * ((l - 2 * q + r) / (dx * dx) + (b - 2 * q + t) / (dy * dy)));
        }
    memcpy(c->u, c->ut, (c->nx + 1) * c->ny * sizeof(double));
    memcpy(c->v, c->vt, c->nx * (c->ny + 1) * sizeof(double));
    double projection_begin = timestamp();
    c->predictor_ms = fmax(0, projection_begin - begin) * 1000;
    bool projected = project(c, dt);
    c->pressure_ms = fmax(0, timestamp() - projection_begin) * 1000;
    if (!projected)
        return false;
    c->time += dt;
    return true;
}
