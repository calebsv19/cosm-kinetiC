#include "app/cfd_periodic3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
static void check(int n, double dt, const char *output) {
    int grid[3] = {n, n, n};
    double length[3] = {2, 2.5, 3};
    CfdMemoryBudget budget = {.limit_bytes = 256 * 1024 * 1024};
    CfdMemoryBudget *prev = cfd_memory_scope(&budget);
    CfdPeriodic3d s;
    assert(cfd_periodic3d_init(&s, grid, length, 1, .1, dt));
    int steps = (int)lround(.4 / dt);
    double solve = 0, transport = 0;
    for (int i = 0; i < steps; i++) {
        size_t allocations = budget.successful_allocations;
        assert(cfd_periodic3d_step(&s));
        if (i >= 2)
            assert(allocations == budget.successful_allocations);
        solve += s.solve_cpu_ms;
        transport += s.transport_cpu_ms;
    }
    double sum[3] = {0}, work = 0;
    cfd_periodic3d_transport(&s.grid, s.velocity, s.transport);
    for (int a = 0; a < 3; a++)
        for (int q = 0; q < s.grid.count; q++) {
            sum[a] += s.transport[a * s.grid.count + q] * s.grid.volume;
            work += s.velocity[a * s.grid.count + q] * s.transport[a * s.grid.count + q] *
                    s.grid.volume;
        }
    for (int a = 0; a < 3; a++)
        assert(fabs(sum[a]) < 1e-12);
    assert(fabs(work) < 1e-11);
    printf("{\"grid\":[%d,%d,%d],\"dt_s\":%.12g,\"time_s\":%.12g,\"velocity_l2_error_m_s\":%.12g,"
           "\"pressure_l2_error_pa\":%.12g,\"true_residual\":%.12g,\"max_divergence_s_inv\":%.12g,"
           "\"kinetic_j\":%.12g,\"physical_dissipation_w\":%.12g,\"forcing_power_w\":%.12g,"
           "\"energy_rate_w\":%.12g,\"energy_residual_w\":%.12g,\"setup_cpu_ms\":%.12g,\"transport_"
           "cpu_ms\":%.12g,\"solve_cpu_ms\":%.12g,\"numerical_peak_bytes\":%zu}\n",
           n, n, n, dt, s.time, s.velocity_l2_error, s.pressure_l2_error, s.true_residual,
           s.max_divergence, s.kinetic_j, s.physical_dissipation_w, s.forcing_power_w,
           s.energy_rate_w, s.energy_residual_w, s.setup_cpu_ms, transport, solve,
           budget.peak_bytes);
    if (output) {
        FILE *f = fopen(output, "wb");
        assert(f);
        assert(fwrite(s.velocity, sizeof(double), (size_t)3 * s.grid.count, f) ==
               (size_t)3 * s.grid.count);
        assert(fwrite(s.pressure, sizeof(double), (size_t)s.grid.count, f) == (size_t)s.grid.count);
        assert(fclose(f) == 0);
    }
    struct rusage usage;
    assert(getrusage(RUSAGE_SELF, &usage) == 0);
#ifdef __APPLE__
    long rss = usage.ru_maxrss;
#else
    long rss = usage.ru_maxrss * 1024;
#endif
    fprintf(stderr, "process_peak_rss_bytes=%ld\n", rss);
    cfd_periodic3d_destroy(&s);
    assert(budget.live_bytes == 0);
    cfd_memory_scope(prev);
}
int main(int argc, char **argv) {
    if (argc == 3 || argc == 4) {
        check(atoi(argv[1]), atof(argv[2]), argc == 4 ? argv[3] : NULL);
        return 0;
    }
    check(8, .001, NULL);
    check(16, .001, NULL);
    check(32, .001, NULL);
    return 0;
}
