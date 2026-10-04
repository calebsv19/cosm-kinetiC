#include "app/cfd_mac2d_force_check.h"
#include <math.h>
static int index_cell(const CfdMac2D *c, int i, int j) { return j * c->nx + (i + c->nx) % c->nx; }
static int fluid(const CfdMac2D *c, int i, int j) {
    return j >= 0 && j < c->ny && !c->solid[index_cell(c, i, j)];
}
static double u(const CfdMac2D *c, int i, int j, int stride) {
    return c->u[j * stride + (stride == c->nx ? (i + c->nx) % c->nx : i)];
}
static double v(const CfdMac2D *c, int i, int j) { return c->v[index_cell(c, i, j)]; }
static double uc(const CfdMac2D *c, int i, int j, int stride) {
    return .5 * (u(c, i, j, stride) + u(c, i + 1, j, stride));
}
static double p(const CfdMac2D *c, int i, int j) { return c->p[index_cell(c, i, j)]; }
bool cfd_mac2d_force_check_strided(const CfdMac2D *c, int stride, int x0, int y0, int x1, int y1,
                                   CfdMac2DForceCheck *out) {
    if (!c || (stride != c->nx && stride != c->nx + 1) || !c->solid || !out || x0 < 1 ||
        x1 >= c->nx || y0 < 1 || y1 >= c->ny || x0 >= x1 || y0 >= y1)
        return false;
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++)
            if (!fluid(c, i, j))
                if (i < x0 + 2 || i >= x1 - 2 || j < y0 + 2 || j >= y1 - 2)
                    return false;
    CfdMac2DForceCheck r = {0};
    r.higher_order_available = true;
    double dx = c->parameters.length / c->nx, dy = c->parameters.height / c->ny,
           w = c->parameters.width, mu = c->parameters.mu, rho = c->parameters.rho;
    if (!cfd_mac2d_surface_pressure(c, &r.surface_pressure_x_n))
        return false;
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++)
            if (fluid(c, i, j)) {
                for (int side = -1; side <= 1; side += 2) {
                    if (!fluid(c, i + side, j)) {
                        if (!fluid(c, i - side, j))
                            return false;
                        if (fluid(c, i - 2 * side, j))
                            r.quadratic_pressure_x_n +=
                                side *
                                (1.875 * p(c, i, j) - 1.25 * p(c, i - side, j) +
                                 .375 * p(c, i - 2 * side, j)) *
                                dy * w;
                        else
                            r.higher_order_available = false;
                        /* At a stationary no-slip straight wall, incompressibility and
                         * zero tangential wall velocity imply du_normal/dnormal=0.
                         * Retain the reconstructed violation as an error diagnostic. */
                        r.reconstructed_normal_viscous_x_n +=
                            2 * mu * (9 * uc(c, i, j, stride) - uc(c, i - side, j, stride)) /
                            (3 * dx) * dy * w;
                    }
                    if (j + side >= 0 && j + side < c->ny && !fluid(c, i, j + side)) {
                        if (!fluid(c, i, j - side))
                            return false;
                        r.surface_viscous_x_n +=
                            mu * (9 * uc(c, i, j, stride) - uc(c, i, j - side, stride)) / (3 * dy) *
                            dx * w;
                        if (fluid(c, i, j - 2 * side))
                            r.cubic_viscous_x_n +=
                                mu *
                                (225 * uc(c, i, j, stride) - 50 * uc(c, i, j - side, stride) +
                                 9 * uc(c, i, j - 2 * side, stride)) /
                                (60 * dy) * dx * w;
                        else
                            r.higher_order_available = false;
                    }
                }
            }
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++)
            if (fluid(c, i, j)) {
                r.cv_momentum_x_kg_m_s += rho * uc(c, i, j, stride) * dx * dy * w;
                r.cv_drive_x_n += c->parameters.gradient * dx * dy * w;
            }
    for (int side = -1; side <= 1; side += 2) {
        int i = side < 0 ? x0 : x1;
        for (int j = y0; j < y1; j++) {
            r.cv_pressure_x_n -= side * .5 * (p(c, i - 1, j) + p(c, i, j)) * dy * w;
            r.cv_viscous_x_n += side * 2 * mu * (u(c, i + 1, j, stride) - u(c, i - 1, j, stride)) /
                                (2 * dx) * dy * w;
            r.cv_advective_x_n += side * rho * u(c, i, j, stride) * u(c, i, j, stride) * dy * w;
        }
        int j = side < 0 ? y0 : y1;
        for (int x = x0; x < x1; x++) {
            double uf = .5 * (uc(c, x, j - 1, stride) + uc(c, x, j, stride));
            double shear = mu * ((uc(c, x, j, stride) - uc(c, x, j - 1, stride)) / dy +
                                 (v(c, x + 1, j) - v(c, x - 1, j)) / (2 * dx));
            r.cv_viscous_x_n += side * shear * dx * w;
            r.cv_advective_x_n += side * rho * uf * v(c, x, j) * dx * w;
        }
    }
    *out = r;
    return true;
}

bool cfd_mac2d_force_check(const CfdMac2D *c, int x0, int y0, int x1, int y1,
                           CfdMac2DForceCheck *out) {
    return c && cfd_mac2d_force_check_strided(c, c->nx, x0, y0, x1, y1, out);
}
