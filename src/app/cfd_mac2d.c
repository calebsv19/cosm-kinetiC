#include "app/cfd_mac2d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static int index_u(const CfdMac2D *c, int x, int y) { return y * c->nx + (x + c->nx) % c->nx; }
static bool fluid_at(const CfdMac2D *c, int x, int y) {
    return y >= 0 && y < c->ny && (!c->solid || !c->solid[index_u(c, x, y)]);
}
static bool open_u(const CfdMac2D *c, int x, int y) {
    return fluid_at(c, x - 1, y) && fluid_at(c, x, y);
}
static bool open_v(const CfdMac2D *c, int x, int y) {
    return fluid_at(c, x, y - 1) && fluid_at(c, x, y);
}
static double u_at(const CfdMac2D *c, const double *u, int x, int y) {
    if (y < 0)
        return 2 * c->parameters.wall_bottom - u[index_u(c, x, -y - 1)];
    if (y >= c->ny)
        return 2 * c->parameters.wall_top - u[index_u(c, x, 2 * c->ny - y - 1)];
    return u[index_u(c, x, y)];
}
static double v_at(const CfdMac2D *c, const double *v, int x, int y) {
    if (y < 0)
        return -v[index_u(c, x, -y)];
    if (y > c->ny)
        return -v[index_u(c, x, 2 * c->ny - y)];
    return v[index_u(c, x, y)];
}
static double face_flux(double qm, double q0, double q1, double qp, double speed) {
    (void)qm;
    (void)qp;
#ifdef CFD_MAC2D_VERIFY_LIMITED
    double a = q0 - qm, b = q1 - q0, d = qp - q1;
    double sl = a * b > 0 ? copysign(fmin(fabs(a), fabs(b)), a) : 0;
    double sr = b * d > 0 ? copysign(fmin(fabs(b), fabs(d)), b) : 0;
    return speed * (speed >= 0 ? q0 + .5 * sl : q1 - .5 * sr);
#endif
    /* Diagnostic build only: isolate donor-cell dissipation without promoting
     * unbounded centered transport into the agent/runtime model. */
#ifdef CFD_MAC2D_VERIFY_CENTERED
    return .5 * speed * (q0 + q1);
#else
    return .5 * speed * (q0 + q1) - .5 * fabs(speed) * (q1 - q0);
#endif
}
static double energy(const CfdMac2D *c) {
    double sum = 0;
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++)
            sum += pow(c->u[index_u(c, i, j)], 2);
    for (int j = 1; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++)
            sum += pow(c->v[index_u(c, i, j)], 2);
    return .5 * c->parameters.rho * c->parameters.length * c->parameters.height *
           c->parameters.width / (c->nx * c->ny) * sum;
}
static double divergence_at(const CfdMac2D *c, int i, int j) {
    if (!fluid_at(c, i, j))
        return 0;
    return (c->u[index_u(c, i + 1, j)] - c->u[index_u(c, i, j)]) * c->nx / c->parameters.length +
           (c->v[index_u(c, i, j + 1)] - c->v[index_u(c, i, j)]) * c->ny / c->parameters.height;
}
double cfd_mac2d_divergence(const CfdMac2D *c) {
    double max = 0;
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++)
            max = fmax(max, fabs(divergence_at(c, i, j)));
    return max;
}
static void apply_a(const CfdMac2D *c, const double *p, double *a) {
    double sx = pow(c->nx / c->parameters.length, 2), sy = pow(c->ny / c->parameters.height, 2);
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++) {
            int k = index_u(c, i, j);
            a[k] = 0;
            if (!fluid_at(c, i, j))
                continue;
            if (open_u(c, i, j))
                a[k] += (p[k] - p[index_u(c, i - 1, j)]) * sx;
            if (open_u(c, i + 1, j))
                a[k] += (p[k] - p[index_u(c, i + 1, j)]) * sx;
            if (open_v(c, i, j))
                a[k] += (p[k] - p[index_u(c, i, j - 1)]) * sy;
            if (open_v(c, i, j + 1))
                a[k] += (p[k] - p[index_u(c, i, j + 1)]) * sy;
        }
}
/* Solve -L(phi)=-D(u*) with periodic X and homogeneous Neumann Y.
 * Face correction is u=u*-grad(phi); physical split pressure is rho*phi/dt.
 * The constant pressure nullspace is removed explicitly. */
bool cfd_mac2d_project(CfdMac2D *c, double dt) {
    if (!c || !c->u || !isfinite(dt) || dt <= 0)
        return false;
    int n = c->nx * c->ny;
    /* Never silently discard normal momentum at an impermeable boundary. */
    if (c->solid) {
        for (int j = 0; j < c->ny; j++)
            for (int i = 0; i < c->nx; i++)
                if (!open_u(c, i, j) && c->u[index_u(c, i, j)] != 0)
                    return false;
        for (int j = 0; j <= c->ny; j++)
            for (int i = 0; i < c->nx; i++)
                if (!open_v(c, i, j) && c->v[index_u(c, i, j)] != 0)
                    return false;
    }
    double rr = 0, mean = 0, peak = 0, before = energy(c);
    c->divergence_before = cfd_mac2d_divergence(c);
    c->pressure_iterations = 0;
    memset(c->p, 0, n * sizeof(double));
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++) {
            int k = index_u(c, i, j);
            c->r[k] = -divergence_at(c, i, j);
            mean += c->r[k];
            peak = fmax(peak, fabs(c->r[k]));
        }
    mean /= c->fluid_cells;
    if (fabs(mean) > 1e-10 * fmax(1, peak))
        return false;
    for (int k = 0; k < n; k++) {
        if (!c->solid || !c->solid[k])
            c->r[k] -= mean;
        c->d[k] = c->r[k];
        rr += c->r[k] * c->r[k];
    }
    double tolerance = fmax(1e-11, peak * 1e-9);
    for (int it = 0; it < 2048 && sqrt(rr) > tolerance; it++) {
        apply_a(c, c->d, c->a);
        double da = 0;
        for (int k = 0; k < n; k++)
            da += c->d[k] * c->a[k];
        if (!isfinite(da) || da <= 0)
            return false;
        double alpha = rr / da, next = 0;
        for (int k = 0; k < n; k++) {
            c->p[k] += alpha * c->d[k];
            c->r[k] -= alpha * c->a[k];
            next += c->r[k] * c->r[k];
        }
        for (int k = 0; k < n; k++)
            c->d[k] = c->r[k] + next / rr * c->d[k];
        rr = next;
        c->pressure_iterations = it + 1;
    }
    /* Remove the nullspace, then recompute the true residual before correction. */
    mean = 0;
    for (int k = 0; k < n; k++)
        mean += c->p[k];
    mean /= c->fluid_cells;
    for (int k = 0; k < n; k++)
        if (!c->solid || !c->solid[k])
            c->p[k] -= mean;
    apply_a(c, c->p, c->a);
    c->pressure_residual = 0;
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++) {
            int k = index_u(c, i, j);
            c->pressure_residual =
                fmax(c->pressure_residual, fabs(c->a[k] + divergence_at(c, i, j)));
        }
    if (!isfinite(c->pressure_residual) || c->pressure_residual > 4 * tolerance)
        return false;
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++) {
            int k = index_u(c, i, j);
            if (!open_u(c, i, j))
                continue;
            c->u[k] -= (c->p[k] - c->p[index_u(c, i - 1, j)]) * c->nx / c->parameters.length;
        }
    for (int j = 1; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++) {
            int k = index_u(c, i, j);
            if (!open_v(c, i, j))
                continue;
            c->v[k] -= (c->p[k] - c->p[index_u(c, i, j - 1)]) * c->ny / c->parameters.height;
        }
    for (int k = 0; k < n; k++)
        c->p[k] *= c->parameters.rho / dt;
    c->obstacle_pressure_force_x_n = 0;
    if (c->solid)
        for (int j = 0; j < c->ny; j++)
            for (int i = 0; i < c->nx; i++) {
                if (!fluid_at(c, i, j))
                    continue;
                double area = c->parameters.height / c->ny * c->parameters.width;
                if (!fluid_at(c, i + 1, j))
                    c->obstacle_pressure_force_x_n += c->p[index_u(c, i, j)] * area;
                if (!fluid_at(c, i - 1, j))
                    c->obstacle_pressure_force_x_n -= c->p[index_u(c, i, j)] * area;
            }
    c->surface_pressure_valid =
        cfd_mac2d_surface_pressure(c, &c->obstacle_surface_pressure_force_x_n);
    c->divergence_after = cfd_mac2d_divergence(c);
    c->projection_energy_change = energy(c) - before;
    return isfinite(c->divergence_after) && c->divergence_after < 8 * tolerance;
}
bool cfd_mac2d_surface_pressure(const CfdMac2D *c, double *force_x_n) {
    if (!c || !c->p || !force_x_n)
        return false;
    double total = 0, area = c->parameters.height / c->ny * c->parameters.width;
    if (c->solid)
        for (int j = 0; j < c->ny; j++)
            for (int i = 0; i < c->nx; i++) {
                if (!fluid_at(c, i, j))
                    continue;
                for (int direction = -1; direction <= 1; direction += 2) {
                    if (fluid_at(c, i + direction, j))
                        continue;
                    if (!fluid_at(c, i - direction, j))
                        return false;
                    /* Wall lies half a cell beyond the nearest pressure sample.
                     * Linear exact, second-order for a smooth pressure field. */
                    double wall =
                        1.5 * c->p[index_u(c, i, j)] - .5 * c->p[index_u(c, i - direction, j)];
                    total += direction * wall * area;
                }
            }
    if (!isfinite(total))
        return false;
    *force_x_n = total;
    return true;
}
bool cfd_mac2d_init(CfdMac2D *c, int nx, int ny, const double d[3], double rho, double mu, double g,
                    double bottom, double top, double perturbation) {
    if (!c || nx < 4 || nx > 64 || ny < 4 || ny > 64 || !isfinite(perturbation) ||
        fabs(perturbation) > 1)
        return false;
    memset(c, 0, sizeof(*c));
    if (!cfd_channel_init(&c->parameters, ny, d, rho, mu, g, bottom, top))
        return false;
    if (rho * d[1] *
            (fmax(fabs(bottom), fabs(top)) + fabs(g) * d[1] * d[1] / (8 * mu) +
             fabs(perturbation)) /
            mu >
        100)
        return false;
    c->nx = nx;
    c->ny = ny;
    c->fluid_cells = nx * ny;
    int n = nx * ny, nv = nx * (ny + 1);
    c->u = calloc(n, sizeof(double));
    c->v = calloc(nv, sizeof(double));
    c->p = calloc(n, sizeof(double));
    c->ut = calloc(n, sizeof(double));
    c->vt = calloc(nv, sizeof(double));
    c->r = calloc(n, sizeof(double));
    c->d = calloc(n, sizeof(double));
    c->a = calloc(n, sizeof(double));
    if (!c->u || !c->v || !c->p || !c->ut || !c->vt || !c->r || !c->d || !c->a) {
        cfd_mac2d_destroy(c);
        return false;
    }
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++)
            c->u[index_u(c, i, j)] = perturbation * sin(6.283185307179586 * i / nx) *
                                     sin(3.141592653589793 * (j + .5) / ny);
    c->divergence_after = cfd_mac2d_divergence(c);
    return true;
}
bool cfd_mac2d_set_obstacle(CfdMac2D *c, int x0, int y0, int x1, int y1) {
    if (!c || !c->u || c->solid || c->time != 0 || x0 < 1 || x1 >= c->nx || y0 < 0 || y1 > c->ny ||
        x0 >= x1 || y0 >= y1)
        return false;
    /* This rectangle cannot split the periodic fluid into disconnected pieces.
     * Reject nonzero initialization rather than lose unmeasured wall impulse. */
    for (int k = 0; k < c->nx * c->ny; k++)
        if (c->u[k] != 0)
            return false;
    for (int k = 0; k < c->nx * (c->ny + 1); k++)
        if (c->v[k] != 0)
            return false;
    c->solid = calloc(c->nx * c->ny, 1);
    if (!c->solid)
        return false;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++)
            c->solid[index_u(c, i, j)] = 1;
    c->fluid_cells = c->nx * c->ny - (x1 - x0) * (y1 - y0);
    return true;
}
void cfd_mac2d_destroy(CfdMac2D *c) {
    if (!c)
        return;
    free(c->solid);
    free(c->u);
    free(c->v);
    free(c->p);
    free(c->ut);
    free(c->vt);
    free(c->r);
    free(c->d);
    free(c->a);
    memset(c, 0, sizeof(*c));
}
bool cfd_mac2d_step(CfdMac2D *c, double dt) { return cfd_mac2d_step_forced(c, dt, NULL, NULL); }
bool cfd_mac2d_step_forced(CfdMac2D *c, double dt, CfdMac2DAcceleration force, void *context) {
    if (!c || !c->u || !isfinite(dt) || dt <= 0 || dt > .1)
        return false;
    if (c->solid) {
        for (int j = 0; j < c->ny; j++)
            for (int i = 0; i < c->nx; i++)
                if (!open_u(c, i, j) && c->u[index_u(c, i, j)] != 0)
                    return false;
        for (int j = 0; j <= c->ny; j++)
            for (int i = 0; i < c->nx; i++)
                if (!open_v(c, i, j) && c->v[index_u(c, i, j)] != 0)
                    return false;
    }
    double dx = c->parameters.length / c->nx, dy = c->parameters.height / c->ny,
           nu = c->parameters.mu / c->parameters.rho;
    double initial_momentum = 0, impulse = 0, remaining = dt,
           area = c->parameters.length * c->parameters.width;
    int n = c->nx * c->ny, nv = c->nx * (c->ny + 1), iterations = 0;
    double max_before = 0, max_after = 0, max_residual = 0, projection_loss = 0;
    double obstacle_viscous_impulse = 0, obstacle_pressure_impulse = 0,
           surface_pressure_impulse = 0, outer_impulse = 0, drive_impulse = 0;
    for (int k = 0; k < n; k++)
        initial_momentum += c->u[k] * c->parameters.rho * dx * dy * c->parameters.width;
    c->substeps = 0;
    c->max_acceleration = 0;
    while (remaining > dt * 1e-12) {
        double umax = fmax(fabs(c->parameters.wall_bottom), fabs(c->parameters.wall_top)), vmax = 0;
        for (int k = 0; k < n; k++) {
            if (!isfinite(c->u[k]))
                return false;
            umax = fmax(umax, fabs(c->u[k]));
        }
        for (int k = 0; k < nv; k++) {
            if (!isfinite(c->v[k]))
                return false;
            vmax = fmax(vmax, fabs(c->v[k]));
        }
        double rate = umax / dx + vmax / dy + 2 * nu * (1 / (dx * dx) + 1 / (dy * dy)) +
                      sqrt(fabs(c->parameters.gradient / c->parameters.rho) / dx);
        double ds = fmin(remaining, .4 / fmax(rate, 1e-12));
        if (++c->substeps > 1024 || !isfinite(ds) || ds <= 0)
            return false;
        double wall = 0, viscous_reaction = 0;
        for (int i = 0; i < c->nx; i++) {
            if (open_u(c, i, 0))
                wall += 2 * c->parameters.mu *
                        (c->parameters.wall_bottom - c->u[index_u(c, i, 0)]) / dy / c->nx;
            if (open_u(c, i, c->ny - 1))
                wall += 2 * c->parameters.mu *
                        (c->parameters.wall_top - c->u[index_u(c, i, c->ny - 1)]) / dy / c->nx;
        }
        impulse += ds * wall * area;
        outer_impulse += ds * wall * area;
        for (int j = 0; j < c->ny; j++)
            for (int i = 0; i < c->nx; i++) {
                int k = index_u(c, i, j);
                if (!open_u(c, i, j)) {
                    c->ut[k] = 0;
                    continue;
                }
                double q = c->u[k], l = u_at(c, c->u, i - 1, j), r = u_at(c, c->u, i + 1, j),
                       b = u_at(c, c->u, i, j - 1), t = u_at(c, c->u, i, j + 1);
                double vb = .5 * (v_at(c, c->v, i - 1, j) + v_at(c, c->v, i, j)),
                       vt = .5 * (v_at(c, c->v, i - 1, j + 1) + v_at(c, c->v, i, j + 1));
                double advection = (face_flux(l, q, r, u_at(c, c->u, i + 2, j), .5 * (q + r)) -
                                    face_flux(u_at(c, c->u, i - 2, j), l, q, r, .5 * (l + q))) /
                                       dx +
                                   (face_flux(b, q, t, u_at(c, c->u, i, j + 2), vt) -
                                    face_flux(u_at(c, c->u, i, j - 2), b, q, t, vb)) /
                                       dy;
                if (c->solid) {
                    bool left = open_u(c, i - 1, j), right = open_u(c, i + 1, j);
                    bool bottom = open_u(c, i, j - 1), top = open_u(c, i, j + 1);
                    /* Shared donor fluxes between active dual volumes; zero
                     * advective flux at blocked dual boundaries. */
                    double fr =
                        right ? .5 * (q + r) * .5 * (q + r) - .5 * fabs(.5 * (q + r)) * (r - q) : 0;
                    double fl =
                        left ? .5 * (l + q) * .5 * (l + q) - .5 * fabs(.5 * (l + q)) * (q - l) : 0;
                    double ft = top ? .5 * vt * (q + t) - .5 * fabs(vt) * (t - q) : 0;
                    double fb = bottom ? .5 * vb * (b + q) - .5 * fabs(vb) * (q - b) : 0;
                    advection = (fr - fl) / dx + (ft - fb) / dy;
                    double removed = 0;
                    if (!left) {
                        l = 0;
                        removed -= q / (dx * dx);
                    }
                    if (!right) {
                        r = 0;
                        removed -= q / (dx * dx);
                    }
                    /* A corner dual face is only partly covered by wall.
                     * On its wall half use the half-cell no-slip distance;
                     * on its exposed half use the full-cell distance to the
                     * constrained normal face. Full walls retain ghost -q. */
                    if (!bottom && j > 0) {
                        double coverage =
                            .5 * (!fluid_at(c, i - 1, j - 1) + !fluid_at(c, i, j - 1));
                        b = -coverage * q;
                        removed -= (1 + coverage) * q / (dy * dy);
                    }
                    if (!top && j + 1 < c->ny) {
                        double coverage =
                            .5 * (!fluid_at(c, i - 1, j + 1) + !fluid_at(c, i, j + 1));
                        t = -coverage * q;
                        removed -= (1 + coverage) * q / (dy * dy);
                    }
                    viscous_reaction -= c->parameters.mu * dx * dy * c->parameters.width * removed;
                }
                double drive = c->parameters.gradient * dx * dy * c->parameters.width;
                impulse += ds * drive;
                drive_impulse += ds * drive;
                double acceleration =
                    force ? force(context, 0, i * dx, (j + .5) * dy, c->time + dt - remaining) : 0;
                if (!isfinite(acceleration))
                    return false;
                impulse += ds * acceleration * c->parameters.rho * dx * dy * c->parameters.width;
                c->ut[k] =
                    q + ds * (-advection +
                              nu * ((l - 2 * q + r) / (dx * dx) + (b - 2 * q + t) / (dy * dy)) +
                              c->parameters.gradient / c->parameters.rho + acceleration);
            }
        memset(c->vt, 0, nv * sizeof(double));
        for (int j = 1; j < c->ny; j++)
            for (int i = 0; i < c->nx; i++) {
                int k = index_u(c, i, j);
                if (!open_v(c, i, j))
                    continue;
                double q = c->v[k], l = v_at(c, c->v, i - 1, j), r = v_at(c, c->v, i + 1, j),
                       b = v_at(c, c->v, i, j - 1), t = v_at(c, c->v, i, j + 1);
                double ul = .5 * (u_at(c, c->u, i, j - 1) + u_at(c, c->u, i, j)),
                       ur = .5 * (u_at(c, c->u, i + 1, j - 1) + u_at(c, c->u, i + 1, j));
                double advection = (face_flux(l, q, r, v_at(c, c->v, i + 2, j), ur) -
                                    face_flux(v_at(c, c->v, i - 2, j), l, q, r, ul)) /
                                       dx +
                                   (face_flux(b, q, t, v_at(c, c->v, i, j + 2), .5 * (q + t)) -
                                    face_flux(v_at(c, c->v, i, j - 2), b, q, t, .5 * (b + q))) /
                                       dy;
                if (c->solid) {
                    bool left = open_v(c, i - 1, j), right = open_v(c, i + 1, j);
                    bool bottom = open_v(c, i, j - 1), top = open_v(c, i, j + 1);
                    double fr = right ? .5 * ur * (q + r) - .5 * fabs(ur) * (r - q) : 0;
                    double fl = left ? .5 * ul * (l + q) - .5 * fabs(ul) * (q - l) : 0;
                    double ft =
                        top ? .5 * (q + t) * .5 * (q + t) - .5 * fabs(.5 * (q + t)) * (t - q) : 0;
                    double fb =
                        bottom ? .5 * (b + q) * .5 * (b + q) - .5 * fabs(.5 * (b + q)) * (q - b)
                               : 0;
                    advection = (fr - fl) / dx + (ft - fb) / dy;
                    if (!left) {
                        double coverage =
                            .5 * (!fluid_at(c, i - 1, j - 1) + !fluid_at(c, i - 1, j));
                        l = -coverage * q;
                    }
                    if (!right) {
                        double coverage =
                            .5 * (!fluid_at(c, i + 1, j - 1) + !fluid_at(c, i + 1, j));
                        r = -coverage * q;
                    }
                    if (!bottom)
                        b = 0;
                    if (!top)
                        t = 0;
                }
                double acceleration =
                    force ? force(context, 1, (i + .5) * dx, j * dy, c->time + dt - remaining) : 0;
                if (!isfinite(acceleration))
                    return false;
                c->vt[k] =
                    q + ds * (acceleration - advection +
                              nu * ((l - 2 * q + r) / (dx * dx) + (b - 2 * q + t) / (dy * dy)));
            }
        for (int k = 0; k < n; k++)
            c->max_acceleration = fmax(c->max_acceleration, fabs(c->ut[k] - c->u[k]) / ds);
        memcpy(c->u, c->ut, n * sizeof(double));
        memcpy(c->v, c->vt, nv * sizeof(double));
        if (!cfd_mac2d_project(c, ds))
            return false;
        obstacle_viscous_impulse += ds * viscous_reaction;
        obstacle_pressure_impulse += ds * c->obstacle_pressure_force_x_n;
        if (!c->surface_pressure_valid)
            return false;
        surface_pressure_impulse += ds * c->obstacle_surface_pressure_force_x_n;
        impulse -= ds * (viscous_reaction + c->obstacle_pressure_force_x_n);
        iterations += c->pressure_iterations;
        max_before = fmax(max_before, c->divergence_before);
        max_after = fmax(max_after, c->divergence_after);
        max_residual = fmax(max_residual, c->pressure_residual);
        projection_loss += c->projection_energy_change;
        remaining -= ds;
    }
    double momentum = 0;
    for (int k = 0; k < n; k++)
        momentum += c->u[k] * c->parameters.rho * dx * dy * c->parameters.width;
    c->momentum_residual = (momentum - initial_momentum - impulse) / dt;
    c->obstacle_viscous_force_x_n = obstacle_viscous_impulse / dt;
    c->obstacle_mean_pressure_force_x_n = obstacle_pressure_impulse / dt;
    c->outer_wall_force_on_fluid_x_n = outer_impulse / dt;
    c->mean_drive_force_x_n = drive_impulse / dt;
    c->obstacle_mean_surface_pressure_force_x_n = surface_pressure_impulse / dt;
    c->continuum_drive_force_x_n =
        c->parameters.gradient * c->fluid_cells * dx * dy * c->parameters.width;
    c->unresolved_drive_force_x_n = c->continuum_drive_force_x_n - c->mean_drive_force_x_n;
    c->time += dt;
    c->pressure_iterations = iterations;
    c->divergence_before = max_before;
    c->divergence_after = max_after;
    c->pressure_residual = max_residual;
    c->projection_energy_change = projection_loss;
    return isfinite(c->momentum_residual);
}
void cfd_mac2d_values(const CfdMac2D *c, double x, double y, double out[11]) {
    double dx = c->parameters.length / c->nx, dy = c->parameters.height / c->ny;
    int i = (int)fmin(c->nx - 1, fmax(0, floor(x / dx))),
        j = (int)fmin(c->ny - 1, fmax(0, floor(y / dy)));
    if (!fluid_at(c, i, j)) {
        memset(out, 0, 11 * sizeof(double));
        out[2] = 1;
        return;
    }
    double u = .5 * (u_at(c, c->u, i, j) + u_at(c, c->u, i + 1, j)),
           v = .5 * (v_at(c, c->v, i, j) + v_at(c, c->v, i, j + 1));
    double uy = (u_at(c, c->u, i, j + 1) + u_at(c, c->u, i + 1, j + 1) - u_at(c, c->u, i, j - 1) -
                 u_at(c, c->u, i + 1, j - 1)) /
                (4 * dy);
    double vx = (v_at(c, c->v, i + 1, j) + v_at(c, c->v, i + 1, j + 1) - v_at(c, c->v, i - 1, j) -
                 v_at(c, c->v, i - 1, j + 1)) /
                (4 * dx);
    if (y <= 0 || y >= c->parameters.height) {
        u = y <= 0 ? c->parameters.wall_bottom : c->parameters.wall_top;
        v = 0;
        uy = y <= 0 ? 2 * (.5 * (c->u[index_u(c, i, 0)] + c->u[index_u(c, i + 1, 0)]) - u) / dy
                    : 2 *
                          (u - .5 * (c->u[index_u(c, i, c->ny - 1)] +
                                     c->u[index_u(c, i + 1, c->ny - 1)])) /
                          dy;
        vx = 0;
    }
    double a[11] = {hypot(u, v),
                    0,
                    0,
                    u,
                    v,
                    0,
                    0,
                    divergence_at(c, i, j),
                    fabs(vx - uy),
                    c->p[index_u(c, i, j)] +
                        (c->solid ? 0 : c->parameters.gradient * (c->parameters.length - x)),
                    c->parameters.mu * (uy + vx)};
    memcpy(out, a, sizeof(a));
}
