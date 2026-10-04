#define _DARWIN_C_SOURCE 1
#include "app/cfd_open3d.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
int main(int argc, char **argv) {
    int ny = argc > 1 ? atoi(argv[1]) : 8;
    double L = argc > 2 ? atof(argv[2]) : 4, H = argc > 3 ? atof(argv[3]) : 2,
           W = argc > 4 ? atof(argv[4]) : 2;
    double mu = argc > 5 ? atof(argv[5]) : .1, datum = argc > 6 ? atof(argv[6]) : 0;
    int n[3] = {(int)(L / 4 * 2 * ny), ny, argc > 7 ? atoi(argv[7]) : ny};
    double length[3] = {L, H, W};
    CfdMemoryBudget budget = {.limit_bytes = 512 * 1024 * 1024};
    CfdMemoryBudget *prev = cfd_memory_scope(&budget);
    CfdOpen3d s;
    assert(cfd_open3d_init(&s, n, length, 1, mu, .002 * H * W, datum));
#ifdef CFD_OPEN3D_VERIFY
    assert(cfd_open3d_verify_operators(&s));
#endif
    size_t alloc = budget.successful_allocations;
    bool ok = cfd_open3d_solve(&s);
    assert(alloc == budget.successful_allocations);
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("{\"grid\":[%d,%d,%d],\"length_m\":%.17g,\"mu\":%.17g,\"datum_pa\":%.17g,\"solved\":%s,"
           "\"passed\":%s,\"velocity_error\":%.17g,\"pressure_error\":%.17g,\"energy_error\":%.17g,"
           "\"energy_imbalance\":%.17g,\"pressure_in_pa\":%.17g,\"pressure_out_pa\":%.17g,"
           "\"pressure_drop_pa\":%.17g,\"upstream_gradient_pa_m\":%.17g,\"inlet_flow\":%.17g,"
           "\"outlet_flow\":%.17g,\"wall_force_n\":[%.17g,%.17g,%.17g,%.17g],\"wall_error\":[%.17g,"
           "%.17g,%.17g,%.17g],\"dissipation_w\":%.17g,\"boundary_power_w\":%.17g,\"discrete_"
           "diffusion_w\":%.17g,\"residual\":%.17g,\"max_divergence\":%.17g,\"flux_error\":%.17g,"
           "\"pressure_iterations\":%d,\"velocity_iterations\":%d,\"setup_cpu_ms\":%.17g,\"solve_"
           "cpu_ms\":%.17g,\"numerical_peak_bytes\":%zu,\"process_peak_rss_bytes\":%ld}\n",
           n[0], n[1], n[2], L, mu, datum, ok ? "true" : "false",
           cfd_open3d_gate(&s) ? "true" : "false", s.velocity_relative_l2,
           s.pressure_relative_error, s.energy_relative_error, s.energy_imbalance, s.pressure_in_pa,
           s.pressure_out_pa, s.pressure_drop_pa, s.upstream_gradient_pa_m, s.inlet_flow,
           s.outlet_flow, s.wall_force_n[0], s.wall_force_n[1], s.wall_force_n[2],
           s.wall_force_n[3], s.wall_relative_error[0], s.wall_relative_error[1],
           s.wall_relative_error[2], s.wall_relative_error[3], s.dissipation_w, s.boundary_power_w,
           s.discrete_diffusion_w, s.relative_residual, s.max_divergence, s.flux_relative_error,
           s.iterations, s.velocity_iterations, s.setup_cpu_ms, s.solve_cpu_ms, budget.peak_bytes,
           usage.ru_maxrss);
    cfd_open3d_destroy(&s);
    assert(budget.live_bytes == 0);
    cfd_memory_scope(prev);
    assert(ok);
    return 0;
}
