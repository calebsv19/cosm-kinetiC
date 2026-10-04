#define _DARWIN_C_SOURCE 1
#include "app/cfd_obstacle3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 8;
    double L = argc > 2 ? atof(argv[2]) : 4, cx = argc > 3 ? atof(argv[3]) : 2;
    int grid[3] = {(int)round(L * n / 2), n, n};
    double length[3] = {L, 2, 2};
    CfdMemoryBudget memory = {0};
    memory.limit_bytes = (size_t)1024 * 1024 * 1024;
    CfdMemoryBudget *prev = cfd_memory_scope(&memory);
    CfdObstacle3d s;
    assert(cfd_obstacle3d_init(&s, grid, length, 1, .1, .008, cx));
#ifdef CFD_MIXED3D_VERIFY
    assert(cfd_obstacle_mixed3d_verify(s.mixed));
#endif
    bool ok = cfd_obstacle3d_solve(&s);
    if (!ok) {
        fprintf(stderr, "solve failed: %s\n", s.error);
        return 1;
    }
    struct rusage rss;
    getrusage(RUSAGE_SELF, &rss);
    printf("{\"n\":%d,\"length\":%.9g,\"center_x\":%.9g,\"fluid_cells\":%d,\"faces\":%d,\"pressure_"
           "drop_pa\":%.17g,\"pressure_force_n\":[%.17g,%.17g,%.17g],\"viscous_force_n\":[%.17g,%."
           "17g,%.17g],\"physical_dissipation_w\":%.17g,\"physical_boundary_power_w\":%.17g,"
           "\"physical_energy_imbalance\":%.17g,\"discrete_energy_imbalance\":%.17g,\"momentum_"
           "relative_residual\":%.17g,\"relative_residual\":%.17g,\"divergence\":%.17g,\"flux_"
           "error\":%.17g,\"iterations\":%d,\"inner_iterations\":%d,\"setup_cpu_ms\":%.9g,\"solve_"
           "cpu_ms\":%.9g,\"numerical_peak_bytes\":%zu,\"process_peak_rss_bytes\":%ld}\n",
           n, L, cx, s.cells, s.count, s.inlet_pressure, s.pressure_force[0], s.pressure_force[1],
           s.pressure_force[2], s.viscous_force[0], s.viscous_force[1], s.viscous_force[2],
           s.physical_dissipation, s.physical_power, s.physical_energy_imbalance,
           s.discrete_energy_imbalance, fabs(s.momentum_residual[0]) / (4 * s.inlet_pressure),
           s.relative_residual, s.max_divergence, s.flux_error, s.iterations, s.inner_iterations,
           s.setup_cpu_ms, s.solve_cpu_ms, memory.peak_bytes, rss.ru_maxrss);
    if (argc > 4) {
        FILE *f = fopen(argv[4], "wb");
        assert(f);
        assert(fwrite(grid, sizeof(int), 3, f) == 3);
        for (int k = 0; k < grid[2]; k++)
            for (int j = 0; j < grid[1]; j++)
                for (int i = 0; i < grid[0]; i++) {
                    double row[4];
                    for (int a = 0; a < 3; a++)
                        row[a] = cfd_obstacle3d_face(&s, a, i, j, k);
                    row[3] = cfd_obstacle_mixed3d_cell(s.mixed, i, j, k) < 0
                                 ? NAN
                                 : cfd_obstacle3d_pressure(&s, i, j, k);
                    assert(fwrite(row, sizeof(double), 4, f) == 4);
                }
        for (int k = 0; k < grid[2]; k++)
            for (int j = 0; j < grid[1]; j++) {
                double v = cfd_obstacle3d_face(&s, 0, grid[0], j, k);
                assert(fwrite(&v, sizeof(double), 1, f) == 1);
            }
        assert(fclose(f) == 0);
    }
    cfd_obstacle3d_destroy(&s);
    assert(memory.live_bytes == 0);
    cfd_memory_scope(prev);
    return 0;
}
