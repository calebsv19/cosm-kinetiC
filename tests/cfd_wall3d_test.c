#define _DARWIN_C_SOURCE 1
#include "app/cfd_wall3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
int main(int argc, char **argv) {
    int size = argc > 1 ? atoi(argv[1]) : 8;
    double dt = argc > 2 ? atof(argv[2]) : .01, T = argc > 3 ? atof(argv[3]) : .4;
    int n[3] = {size, size, size};
    double length[3] = {2, 2.5, 3};
    CfdMemoryBudget b = {.limit_bytes = 512 * 1024 * 1024};
    CfdMemoryBudget *prev = cfd_memory_scope(&b);
    CfdWall3d s;
    assert(cfd_wall3d_init(&s, n, length, 1, .1, dt, argc > 5 && atoi(argv[5]) != 0));
    int steps = (int)lround(T / dt);
    FILE *history = argc > 6 ? fopen(argv[6], "w") : NULL;
    if (argc > 6)
        assert(history);
    if (history)
        fprintf(history, "time,velocity_amplitude,pressure_amplitude,velocity_error,pressure_error,"
                         "wall_error,dissipation_w,energy_residual_w\n");
    for (int i = 0; i < steps; i++) {
        if (!cfd_wall3d_step(&s)) {
            fprintf(stderr, "step %d: %s\n", i, s.error);
            return 1;
        }
        if (history) {
            double pn = 0, pd = 0, wall = 0;
            for (int q = 0; q < s.grid.count; q++) {
                pn += s.pressure[q] * s.pressure_reference[q];
                pd += s.pressure_reference[q] * s.pressure_reference[q];
            }
            for (int a = 0; a < 8; a++)
                wall = fmax(wall, s.wall_error[a]);
            fprintf(history, "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n", s.time,
                    s.amplitude, pn / pd, s.velocity_error, s.pressure_error, wall, s.dissipation_w,
                    s.energy_residual_w);
        }
    }
    if (history)
        assert(fclose(history) == 0);
    double wall = 0;
    for (int a = 0; a < 8; a++)
        wall = fmax(wall, s.wall_error[a]);
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("{\"n\":%d,\"dt\":%.17g,\"time\":%.17g,\"velocity_error\":%.17g,\"pressure_error\":%."
           "17g,\"wall_error\":%.17g,\"dissipation_error\":%.17g,\"energy_imbalance\":%.17g,"
           "\"amplitude\":%.17g,\"orthogonal_error\":%.17g,\"residual\":%.17g,\"max_divergence\":%."
           "17g,\"iterations\":%d,\"inner_iterations\":%d,\"setup_cpu_ms\":%.17g,\"last_step_cpu_"
           "ms\":%.17g,\"peak_bytes\":%zu,\"rss_bytes\":%ld}\n",
           size, dt, s.time, s.velocity_error, s.pressure_error, wall,
           fabs(s.dissipation_w / s.reference_dissipation_w - 1),
           fabs(s.energy_residual_w) / fmax(fabs(s.reference_power_w), s.reference_dissipation_w),
           s.amplitude, s.orthogonal_error, s.true_residual, s.max_divergence, s.iterations,
           s.inner_iterations, s.setup_cpu_ms, s.solve_cpu_ms, b.peak_bytes, usage.ru_maxrss);
    if (argc > 4) {
        FILE *f = fopen(argv[4], "wb");
        assert(f);
        assert(fwrite(s.velocity, sizeof(double), s.count, f) == (size_t)s.count);
        assert(fwrite(s.pressure, sizeof(double), s.grid.count, f) == (size_t)s.grid.count);
        assert(fclose(f) == 0);
    }
    assert(fabs(s.transport_self_power_w) < 1e-12);
    cfd_wall3d_destroy(&s);
    assert(b.live_bytes == 0);
    cfd_memory_scope(prev);
    return 0;
}
