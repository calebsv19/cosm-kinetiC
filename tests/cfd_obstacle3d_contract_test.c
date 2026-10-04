#include "app/cfd_obstacle3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool cancel(void *p) {
    (void)p;
    return false;
}
static double checksum(const double *a, int n) {
    double v = 0;
    for (int q = 0; q < n; q++)
        v += a[q] * (q % 29 + 1);
    return v;
}
int main(void) {
    int n[3] = {16, 8, 8};
    double length[3] = {4, 2, 2};
    CfdMemoryBudget budget = {0};
    budget.limit_bytes = 64 * 1024 * 1024;
    CfdMemoryBudget *old = cfd_memory_scope(&budget);
    CfdObstacle3d s;
    const double invalid[] = {NAN, INFINITY, -INFINITY, 0, -1};
    for (int q = 0; q < 5; q++) {
        assert(!cfd_obstacle3d_init(&s, n, length, invalid[q], .1, .008, 2));
        cfd_obstacle3d_destroy(&s);
        assert(!budget.live_bytes);
        assert(!cfd_obstacle3d_init(&s, n, length, 1, invalid[q], .008, 2));
        cfd_obstacle3d_destroy(&s);
        assert(!budget.live_bytes);
        assert(!cfd_obstacle3d_init(&s, n, length, 1, .1, invalid[q], 2));
        cfd_obstacle3d_destroy(&s);
        assert(!budget.live_bytes);
    }
    assert(cfd_obstacle3d_init(&s, n, length, 1, .1, .008, 2));
    assert(cfd_obstacle_mixed3d_verify(s.mixed));
    assert(cfd_obstacle3d_solve(&s));
    assert(s.relative_residual <= 1e-11 && s.max_divergence < 1e-8 &&
           s.discrete_energy_imbalance < 1e-9 && s.flux_error < 1e-9);
    assert(fabs(s.discrete_momentum_residual[0]) / (4 * s.inlet_pressure) < 1e-9);
    double velocity = checksum(s.u, s.count), pressure = checksum(s.p, s.cells), fp[3];
    memcpy(fp, s.pressure_force, sizeof(fp));
    size_t allocations = budget.successful_allocations, peak = budget.peak_bytes;
    assert(cfd_obstacle3d_solve(&s));
    assert(budget.successful_allocations == allocations);
    assert(checksum(s.u, s.count) == velocity && checksum(s.p, s.cells) == pressure);
    for (int q = 0; q < s.cells; q++)
        s.candidate_p[q] = s.p[q] + 7;
    cfd_obstacle3d_measure(&s, s.u, s.candidate_p);
    for (int a = 0; a < 3; a++)
        assert(fabs(s.pressure_force[a] - fp[a]) < 1e-12);
    cfd_obstacle3d_measure(&s, s.u, s.p);
    double accepted_pressure = s.inlet_pressure, accepted_energy = s.physical_dissipation;
    s.requested_flow = NAN;
    assert(!cfd_obstacle3d_solve(&s));
    assert(!strcmp(s.error, "invalid_requested_flow"));
    assert(checksum(s.u, s.count) == velocity && checksum(s.p, s.cells) == pressure);
    assert(s.inlet_pressure == accepted_pressure && s.physical_dissipation == accepted_energy);
    /* Deliberately bypass scene admission to exercise post-scaling rejection. */
    s.requested_flow = 1e12;
    assert(!cfd_obstacle3d_solve(&s));
    assert(!strcmp(s.error, "scaled_continuity_residual_failed"));
    assert(checksum(s.u, s.count) == velocity && checksum(s.p, s.cells) == pressure);
    assert(s.inlet_pressure == accepted_pressure && s.physical_dissipation == accepted_energy);
    s.requested_flow = .008;
    cfd_obstacle_mixed3d_checkpoint(s.mixed, cancel, NULL);
    assert(!cfd_obstacle3d_solve(&s));
    assert(!strcmp(s.error, "cancelled_at_krylov_checkpoint"));
    assert(checksum(s.u, s.count) == velocity && checksum(s.p, s.cells) == pressure);
    cfd_obstacle3d_destroy(&s);
    assert(!budget.live_bytes);
    budget = (CfdMemoryBudget){0};
    budget.limit_bytes = peak;
    assert(cfd_obstacle3d_init(&s, n, length, 1, .1, .008, 2));
    assert(cfd_obstacle3d_solve(&s));
    cfd_obstacle3d_destroy(&s);
    assert(!budget.live_bytes);
    budget = (CfdMemoryBudget){0};
    budget.limit_bytes = peak - 1;
    assert(!cfd_obstacle3d_init(&s, n, length, 1, .1, .008, 2));
    assert(budget.last_failure == CFD_MEMORY_LIMIT);
    cfd_obstacle3d_destroy(&s);
    assert(!budget.live_bytes);
    /* Mean-interval quadratic/cubic pressure and wall-shear formulas independently
     * recover exact polynomial traces; pressure shift on a closed body is zero. */
    for (int m = 0; m < 4; m++) {
        double h = .17, a = .3, b = -.2, c = .07;
        double u[3];
        for (int q = 0; q < 3; q++) {
            double l = q * h, r = (q + 1) * h;
            u[q] = (a * (r * r - l * l) / 2 + b * (r * r * r - l * l * l) / 3) / h;
        }
        assert(fabs((7 * u[0] - u[1]) / (2 * h) - a) < 1e-12);
        for (int q = 0; q < 3; q++) {
            double l = q * h, r = (q + 1) * h;
            u[q] = c + a * (r * r - l * l) / (2 * h) + b * (r * r * r - l * l * l) / (3 * h);
        }
        assert(fabs((11 * u[0] - 7 * u[1] + 2 * u[2]) / 6 - c) < 1e-12);
    }
    cfd_memory_scope(old);
    printf("C3D-8 compact adjoint/SPD, closed pressure gauge, polynomial reconstruction, cached "
           "solve, cancellation and exact-cap cleanup passed; peak=%zu\n",
           peak);
    return 0;
}
