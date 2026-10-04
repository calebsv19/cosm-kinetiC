#define _DARWIN_C_SOURCE 1
#include "app/cfd_startup3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void report(const CfdStartup3d *s, const CfdMemoryBudget *b, const char *path) {
    double wall = 0;
    for (int a = 0; a < 4; a++)
        wall = fmax(wall, s->wall_error[a]);
    printf("{\"grid\":[%d,%d,%d],\"dt\":%.17g,\"time\":%.17g,\"length\":%.17g,"
           "\"velocity_error\":%.17g,\"pressure_error\":%.17g,\"flow_error\":%.17g,"
           "\"wall_error\":%.17g,\"dissipation_error\":%.17g,\"energy_imbalance\":%.17g,"
           "\"flow\":%.17g,\"wall_force\":[%.17g,%.17g,%.17g,%.17g],\"dissipation\":%.17g,"
           "\"kinetic\":%.17g,\"boundary_power\":%.17g,\"energy_rate\":%.17g,"
           "\"reference_flow\":%.17g,\"reference_kinetic\":%.17g,\"reference_power\":%.17g,"
           "\"reference_dissipation\":%.17g,\"reference_energy_rate\":%.17g,"
           "\"residual\":%.17g,\"divergence\":%.17g,\"last_step_ms\":%.17g,\"peak_bytes\":%zu}\n",
           s->grid.n[0], s->grid.n[1], s->grid.n[2], s->dt, s->time, s->grid.length[0],
           s->velocity_error, s->pressure_error, s->flow_error, wall, s->dissipation_error,
           fabs(s->energy_residual) / s->reference_power, s->flow, s->wall_force[0],
           s->wall_force[1], s->wall_force[2], s->wall_force[3], s->dissipation, s->kinetic,
           s->boundary_power, s->energy_rate, s->reference_flow, s->reference_kinetic,
           s->reference_power, s->reference_dissipation, s->reference_energy_rate, s->true_residual,
           s->max_divergence, s->solve_cpu_ms, b->peak_bytes);
    fflush(stdout);
    if (path) {
        FILE *f = fopen(path, "wb");
        assert(f);
        assert(fwrite(s->velocity, sizeof(double), s->count, f) == (size_t)s->count);
        assert(fwrite(s->pressure, sizeof(double), s->grid.count, f) == (size_t)s->grid.count);
        assert(fclose(f) == 0);
    }
}
int main(int argc, char **argv) {
    int ny = argc > 1 ? atoi(argv[1]) : 8;
    double dt = argc > 2 ? atof(argv[2]) : .01, T = argc > 3 ? atof(argv[3]) : .5,
           L = argc > 4 ? atof(argv[4]) : 4;
    int n[3] = {(int)lround(L * ny / 2), ny, ny};
    double lengths[3] = {L, 2, 2}, times[64];
    int observations = 1, next = 0;
    times[0] = T;
    if (argc > 6) {
        char *p = argv[6];
        observations = 0;
        while (*p) {
            assert(observations < 64);
            char *end;
            times[observations++] = strtod(p, &end);
            assert(end != p && (*end == ',' || !*end));
            p = *end ? end + 1 : end;
        }
    }
    for (int i = 0; i < observations; i++) {
        assert(times[i] > 0 && times[i] <= T + 1e-12);
        assert(fabs(times[i] / dt - lround(times[i] / dt)) < 1e-9);
        assert(i == 0 || times[i] > times[i - 1]);
    }
    CfdMemoryBudget b = {.limit_bytes = 512 * 1024 * 1024};
    CfdMemoryBudget *prev = cfd_memory_scope(&b);
    CfdStartup3d s;
    assert(cfd_startup3d_init(&s, n, lengths, 1, .1, dt, .008));
    for (int i = 0; i < (int)lround(T / dt); i++) {
        if (!cfd_startup3d_step(&s)) {
            fprintf(stderr, "step %d %s\n", i, s.error);
            cfd_startup3d_destroy(&s);
            return 1;
        }
        if (next < observations && i + 1 == (int)lround(times[next] / dt)) {
            char path[4096];
            const char *output = NULL;
            if (argc > 5) {
                if (observations == 1)
                    output = argv[5];
                else {
                    int len = snprintf(path, sizeof(path), "%s-t%.6g.bin", argv[5], times[next]);
                    assert(len > 0 && len < (int)sizeof(path));
                    output = path;
                }
            }
            report(&s, &b, output);
            next++;
        }
    }
    assert(next == observations);
    cfd_startup3d_destroy(&s);
    assert(b.live_bytes == 0);
    cfd_memory_scope(prev);
    return 0;
}
