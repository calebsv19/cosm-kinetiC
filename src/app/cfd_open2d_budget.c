#include "app/cfd_open2d_budget.h"
static double u(const CfdOpen2D *c, int i, int j) {
    if (j < 0)
        return -2 * u(c, i, 0) + u(c, i, 1) / 3;
    if (j >= c->ny)
        return -2 * u(c, i, c->ny - 1) + u(c, i, c->ny - 2) / 3;
    return c->u[j * (c->nx + 1) + i];
}
static double uc(const CfdOpen2D *c, int i, int j) { return .5 * (u(c, i, j) + u(c, i + 1, j)); }
static double vc(const CfdOpen2D *c, int i, int j) {
    if (i < 0)
        return -vc(c, 0, j);
    if (i >= c->nx)
        return (u(c, c->nx, j) < 0 ? -1 : 1) * vc(c, c->nx - 1, j);
    return .5 * (c->v[j * c->nx + i] + c->v[(j + 1) * c->nx + i]);
}
static double cross_sample(const CfdOpen2D *c, int i, int j, int di, int dj, bool x_velocity) {
    int ni = i + di, nj = j + dj;
    double (*value)(const CfdOpen2D *, int, int) = x_velocity ? uc : vc;
    if (ni >= 0 && ni < c->nx && nj >= 0 && nj < c->ny && c->solid && c->solid[nj * c->nx + ni])
        return -2 * value(c, i, j) + value(c, i - di, j - dj) / 3;
    return value(c, ni, nj);
}
static bool physical_budget(const CfdOpen2D *c, CfdOpen2DBudget *out) {
    if (!c || !c->u || !out)
        return false;
    CfdOpen2DBudget a = {0};
    double dx = c->length / c->nx, dy = c->height / c->ny, dv = dx * dy * c->width;
    for (int j = 0; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++) {
            if (c->solid && c->solid[j * c->nx + i])
                continue;
            double x = uc(c, i, j), y = vc(c, i, j);
            a.momentum_x_kg_m_s += c->rho * x * dv;
            a.kinetic_energy_j += .5 * c->rho * (x * x + y * y) * dv;
            double ux = (u(c, i + 1, j) - u(c, i, j)) / dx;
            double vy = (c->v[(j + 1) * c->nx + i] - c->v[j * c->nx + i]) / dy;
            double uy =
                (cross_sample(c, i, j, 0, 1, true) - cross_sample(c, i, j, 0, -1, true)) / (2 * dy);
            double vx = (cross_sample(c, i, j, 1, 0, false) - cross_sample(c, i, j, -1, 0, false)) /
                        (2 * dx);
            a.dissipation_w += c->mu * (2 * (ux * ux + vy * vy) + (uy + vx) * (uy + vx)) * dv;
        }
    for (int i = 0; i < c->nx; i++) {
        double bottom = (9 * uc(c, i, 0) - uc(c, i, 1)) / (3 * dy);
        double top = (9 * uc(c, i, c->ny - 1) - uc(c, i, c->ny - 2)) / (3 * dy);
        a.wall_force_x_n -= c->mu * (bottom + top) * dx * c->width;
    }
    for (int j = 0; j < c->ny; j++)
        for (int side = -1; side <= 1; side += 2) {
            int i = side < 0 ? 0 : c->nx;
            double normal = u(c, i, j), tangent = side < 0 || normal < 0 ? 0 : vc(c, c->nx - 1, j);
            double pressure = side < 0 ? 1.5 * c->p[j * c->nx] - .5 * c->p[j * c->nx + 1] : 0;
            double ux = side < 0 ? (u(c, 1, j) - normal) / dx : (normal - u(c, c->nx - 1, j)) / dx;
            double uy = (u(c, i, j + 1) - u(c, i, j - 1)) / (2 * dy);
            double vx = side < 0     ? 2 * vc(c, 0, j) / dx
                        : normal < 0 ? -2 * vc(c, c->nx - 1, j) / dx
                                     : 0;
            double area = dy * c->width;
            a.pressure_force_x_n -= side * pressure * area;
            a.normal_viscous_force_x_n += side * 2 * c->mu * ux * area;
            a.outward_momentum_flux_n += side * c->rho * normal * normal * area;
            a.pressure_work_w -= side * pressure * normal * area;
            a.viscous_work_w += side * c->mu * (2 * ux * normal + (uy + vx) * tangent) * area;
            a.outward_kinetic_flux_w +=
                side * .5 * c->rho * (normal * normal + tangent * tangent) * normal * area;
        }
    *out = a;
    return true;
}

bool cfd_open2d_budget(const CfdOpen2D *c, CfdOpen2DBudget *out) {
    return c && !c->solid && physical_budget(c, out);
}
bool cfd_open2d_energy(const CfdOpen2D *c, CfdOpen2DEnergy *out) {
    CfdOpen2DBudget b;
    if (!out || !physical_budget(c, &b))
        return false;
    *out = (CfdOpen2DEnergy){b.kinetic_energy_j, b.pressure_work_w, b.viscous_work_w,
                             b.outward_kinetic_flux_w, b.dissipation_w};
    return true;
}
