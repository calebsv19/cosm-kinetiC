#include "app/cfd_obstacle3d_box.h"
#include <math.h>
#include <string.h>
#include <time.h>

bool cfd_obstacle3d_box_init(CfdObstacle3d *s, const int n[3], const double length[3],
                             double rho, double mu, double flow,
                             const double lower_m[3], const double upper_m[3]) {
    if (!s)
        return false;
    memset(s, 0, sizeof(*s));
    clock_t start = clock();
    if (!lower_m || !upper_m || !cfd_cartesian3d_init(&s->grid, n, length) ||
        fabs(length[1] - 2) > 1e-12 || fabs(length[2] - 2) > 1e-12 ||
        !isfinite(rho) || rho <= 0 || !isfinite(mu) || mu <= 0 ||
        !isfinite(flow) || flow <= 0)
        return false;
    double maximum_extent = 0;
    for (int a = 0; a < 3; a++) {
        if (!isfinite(lower_m[a]) || !isfinite(upper_m[a]) ||
            upper_m[a] <= lower_m[a])
            return false;
        double lo = lower_m[a] / s->grid.h[a], hi = upper_m[a] / s->grid.h[a];
        if (!isfinite(lo) || !isfinite(hi) || lo < 2 || hi > n[a] - 2 ||
            fabs(lo - round(lo)) > 1e-9 || fabs(hi - round(hi)) > 1e-9 ||
            round(hi) - round(lo) < 2)
            return false;
        s->lo[a] = (int)round(lo);
        s->hi[a] = (int)round(hi);
        maximum_extent = fmax(maximum_extent, upper_m[a] - lower_m[a]);
    }
    double reynolds = rho * flow * maximum_extent / (4 * mu);
    if (!isfinite(reynolds) || reynolds > .1)
        return false;
    s->rho = rho;
    s->mu = mu;
    s->requested_flow = flow;
    s->center_x = .5 * (lower_m[0] + upper_m[0]);
    s->mixed = cfd_obstacle_mixed3d_create(&s->grid, mu, s->lo, s->hi);
    if (!s->mixed)
        return false;
    s->count = cfd_obstacle_mixed3d_count(s->mixed);
    s->cells = cfd_obstacle_mixed3d_cells(s->mixed);
    s->u = cfd_memory_calloc(s->count, sizeof(double));
    s->p = cfd_memory_calloc(s->cells, sizeof(double));
    s->candidate_u = cfd_memory_calloc(s->count, sizeof(double));
    s->candidate_p = cfd_memory_calloc(s->cells, sizeof(double));
    s->rhs = cfd_memory_calloc(s->count, sizeof(double));
    s->reconstruction_planes =
        cfd_memory_calloc((size_t)9 * (2 * n[0] + 1) * (2 * n[1] + 1), sizeof(double));
    if (!s->u || !s->p || !s->candidate_u || !s->candidate_p || !s->rhs ||
        !s->reconstruction_planes) {
        cfd_obstacle3d_destroy(s);
        return false;
    }
    s->setup_cpu_ms = 1000. * (clock() - start) / CLOCKS_PER_SEC;
    return true;
}
