#include "app/cfd_duct3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static void run(int nx, int ny, int nz, double h, double w, bool gate) {
    CfdMemoryBudget budget = {.limit_bytes = 256 * 1024 * 1024};
    CfdMemoryBudget *prev = cfd_memory_scope(&budget);
    int grid[3] = {nx, ny, nz};
    double length[3] = {4, h, w};
    CfdDuct3d s;
    assert(cfd_duct3d_init(&s, grid, length, 1, .1, .002 * h * w));
    size_t count = budget.successful_allocations;
    assert(cfd_duct3d_solve(&s));
    assert(count == budget.successful_allocations);
    double wall = 0, sum = 0;
    for (int i = 0; i < 4; i++) {
        wall = fmax(wall, s.wall_relative_error[i]);
        sum += s.wall_force_n[i];
    }
    double momentum =
        fabs(sum - s.gradient_pa_m * length[0] * h * w) / (s.gradient_pa_m * length[0] * h * w);
    double discrete = fabs(s.discrete_dissipation_w - s.gradient_pa_m * length[0] * s.flow_m3_s) /
                      (s.gradient_pa_m * length[0] * s.flow_m3_s);
    double flow = fabs(s.flow_m3_s - s.requested_flow) / s.requested_flow;
    assert(s.relative_residual <= 1e-11 && s.max_divergence < 1e-8 && momentum < 1e-9 &&
           discrete < 1e-9 && flow < 1e-11);
    if (gate)
        assert(s.velocity_relative_l2 <= .01 && s.pressure_relative_error <= .01 && wall <= .02 &&
               s.energy_relative_error <= .02);
    printf("{\"grid\":[%d,%d,%d],\"dimensions_m\":[4,%.12g,%.12g],\"velocity_relative_l2\":%.12g,"
           "\"pressure_relative_error\":%.12g,\"max_wall_relative_error\":%.12g,\"energy_relative_"
           "error\":%.12g,\"volume_flow_m3_s\":%.12g,\"flow_relative_error\":%.12g,\"pressure_drop_"
           "pa\":%.12g,\"wall_forces_n\":[%.12g,%.12g,%.12g,%.12g],\"physical_dissipation_w\":%."
           "12g,\"discrete_dissipation_w\":%.12g,\"momentum_relative_residual\":%.12g,\"discrete_"
           "energy_relative_residual\":%.12g,\"linear_relative_residual\":%.12g,\"max_divergence_s_"
           "inv\":%.12g,\"iterations\":%d,\"setup_cpu_ms\":%.12g,\"solve_cpu_ms\":%.12g,"
           "\"numerical_peak_bytes\":%zu,\"accepted_finest\":%s}\n",
           nx, ny, nz, h, w, s.velocity_relative_l2, s.pressure_relative_error, wall,
           s.energy_relative_error, s.flow_m3_s, flow, s.gradient_pa_m * 4, s.wall_force_n[0],
           s.wall_force_n[1], s.wall_force_n[2], s.wall_force_n[3], s.dissipation_w,
           s.discrete_dissipation_w, momentum, discrete, s.relative_residual, s.max_divergence,
           s.iterations, s.setup_cpu_ms, s.solve_cpu_ms, budget.peak_bytes,
           gate ? "true" : "false");
    cfd_duct3d_destroy(&s);
    assert(budget.live_bytes == 0);
    cfd_memory_scope(prev);
}
static void units(void) {
    int n[3] = {8, 8, 8};
    double l[3] = {4, 2, 2};
    CfdDuct3d a, b, c;
    assert(cfd_duct3d_init(&a, n, l, 1, .1, .008));
    assert(cfd_duct3d_solve(&a));
    assert(cfd_duct3d_init(&b, n, l, 2, .1, .008));
    assert(cfd_duct3d_solve(&b));
    assert(cfd_duct3d_init(&c, n, l, 1, .2, .008));
    assert(cfd_duct3d_solve(&c));
    assert(fabs(a.gradient_pa_m - b.gradient_pa_m) / a.gradient_pa_m < 1e-10);
    assert(fabs(c.gradient_pa_m - 2 * a.gradient_pa_m) / a.gradient_pa_m < 1e-10);
    for (int i = 0; i < 4; i++)
        assert(fabs(c.wall_force_n[i] - 2 * a.wall_force_n[i]) / a.wall_force_n[i] < 1e-10);
    cfd_duct3d_destroy(&a);
    cfd_duct3d_destroy(&b);
    cfd_duct3d_destroy(&c);
}
int main(int argc, char **argv) {
    if (argc == 2) {
        int n = atoi(argv[1]);
        run(2 * n, n, n, 2, 2, n >= 32);
        return 0;
    }
    units();
    double a = cfd_duct3d_reference_mean(2, 2, 256), b = cfd_duct3d_reference_mean(2, 2, 512);
    assert(fabs(a - b) / b < 1e-10);
    double v = cfd_duct3d_reference_response(2, 3, .7, 1.2, 512),
           v2 = cfd_duct3d_reference_response(2, 3, .7, 1.2, 1024);
    assert(fabs(v - v2) / v2 < 1e-7);
    assert(fabs(cfd_duct3d_reference_response(2, 3, 0, 1.2, 64)) < 1e-14);
    assert(fabs(cfd_duct3d_reference_response(2, 3, .7, 0, 64)) < 1e-14);
    run(16, 8, 8, 2, 2, false);
    run(32, 16, 16, 2, 2, false);
    run(64, 32, 32, 2, 2, true);
    run(32, 24, 32, 1.5, 3, true);
    CfdMemoryBudget budget = {.limit_bytes = 4096};
    CfdMemoryBudget *prev = cfd_memory_scope(&budget);
    int n[3] = {8, 8, 8};
    double l[3] = {4, 2, 2};
    CfdDuct3d s;
    assert(!cfd_duct3d_init(&s, n, l, 1, .1, .008));
    assert(budget.last_failure == CFD_MEMORY_LIMIT);
    assert(budget.live_bytes == 0);
    cfd_memory_scope(prev);
    return 0;
}
