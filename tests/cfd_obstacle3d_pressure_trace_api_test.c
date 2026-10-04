#include "app/cfd_obstacle3d_pressure_trace.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static double interval_mean(double left, double right, int degree) {
    return (pow(right, degree + 1) - pow(left, degree + 1)) /
           ((degree + 1) * (right - left));
}

int main(void) {
    int controls = 0, failed_controls = 0;
    size_t depths[3] = {0};
    double maximum = 0, maximum_gauge = 0;
    for (int scene = 0; scene < 4; scene++) {
        int n[3] = {32, 16, 16};
        double lengths[3] = {4, 2, 2};
        int lo[3] = {12, 4, 4}, hi[3] = {20, 12, 12};
        if (scene) {
            lo[1] = 4 - scene;
            hi[1] = 16 - lo[1];
        }
        CfdMemoryBudget budget = {.limit_bytes = 64 * 1024 * 1024};
        CfdMemoryBudget *previous = cfd_memory_scope(&budget);
        CfdObstacle3d s = {0};
        assert(cfd_cartesian3d_init(&s.grid, n, lengths));
        memcpy(s.lo, lo, sizeof(lo));
        memcpy(s.hi, hi, sizeof(hi));
        s.mixed = cfd_obstacle_mixed3d_create(&s.grid, .1, lo, hi);
        assert(s.mixed);
        size_t cells = (size_t)cfd_obstacle_mixed3d_cells(s.mixed);
        double *p = cfd_memory_calloc(cells, sizeof(double));
        assert(p);
        size_t allocations = budget.successful_allocations;
        CfdObstacle3dPressureTrace out, sentinel;
        memset(&sentinel, 0x5a, sizeof(sentinel));
        out = sentinel;
        if (scene == 3) {
            assert(!cfd_obstacle3d_pressure_trace_diagnostic(&s, p, cells, &out));
            assert(!memcmp(&out, &sentinel, sizeof(out)));
            failed_controls++;
        } else {
            for (int px = 0; px <= 3; px++)
                for (int py = 0; py <= 3 - px; py++)
                    for (int pz = 0; pz <= 3 - px - py; pz++) {
                        int powers[3] = {px, py, pz};
                        if (powers[1] > 3 - scene)
                            continue;
                        CfdObstacle3dPressureTrace base = {0};
                        for (int gauge = 0; gauge < 2; gauge++) {
                            double offset = gauge ? 3.2 : 0;
                            for (int k = 0; k < n[2]; k++)
                                for (int j = 0; j < n[1]; j++)
                                    for (int i = 0; i < n[0]; i++) {
                                        int c[3] = {i, j, k};
                                        int q = cfd_obstacle_mixed3d_cell(s.mixed, i, j, k);
                                        if (q < 0)
                                            continue;
                                        double value = 1;
                                        for (int a = 0; a < 3; a++)
                                            value *= interval_mean(c[a] * s.grid.h[a],
                                                                  (c[a] + 1) * s.grid.h[a],
                                                                  powers[a]);
                                        p[q] = offset + value;
                                    }
                            assert(cfd_obstacle3d_pressure_trace_diagnostic(&s, p, cells, &out));
                            double net[3] = {0};
                            for (int a = 0; a < 3; a++)
                                for (int side = 0; side < 2; side++) {
                                    double plane = (side ? hi[a] : lo[a]) * s.grid.h[a];
                                    double exact = pow(plane, powers[a]), area = 1;
                                    for (int b = 0; b < 3; b++)
                                        if (b != a) {
                                            double left = lo[b] * s.grid.h[b];
                                            double right = hi[b] * s.grid.h[b];
                                            exact *= interval_mean(left, right, powers[b]);
                                            area *= right - left;
                                        }
                                    exact = -(exact + offset) * (side ? 1 : -1) * area;
                                    net[a] += exact;
                                    maximum = fmax(maximum,
                                        fabs(out.side_force_n[2 * a + side][a] - exact));
                                    if (gauge)
                                        maximum_gauge = fmax(maximum_gauge,
                                            fabs(out.side_force_n[2 * a + side][a] -
                                                 base.side_force_n[2 * a + side][a] +
                                                 offset * (side ? 1 : -1) * area));
                                    controls++;
                                }
                            for (int a = 0; a < 3; a++) {
                                maximum = fmax(maximum, fabs(out.force_n[a] - net[a]));
                                if (gauge)
                                    maximum_gauge = fmax(maximum_gauge,
                                        fabs(out.force_n[a] - base.force_n[a]));
                                depths[a] += out.interval_depth_patches[a];
                            }
                            if (!gauge)
                                base = out;
                        }
                    }
            int q = cfd_obstacle_mixed3d_cell(s.mixed, lo[0] - 1, lo[1], lo[2]);
            assert(q >= 0);
            const double invalid[3] = {NAN, INFINITY, -INFINITY};
            for (int i = 0; i < 3; i++) {
                p[q] = invalid[i];
                out = sentinel;
                assert(!cfd_obstacle3d_pressure_trace_diagnostic(&s, p, cells, &out));
                assert(!memcmp(&out, &sentinel, sizeof(out)));
                failed_controls++;
            }
        }
        out = sentinel;
        assert(!cfd_obstacle3d_pressure_trace_diagnostic(NULL, p, cells, &out));
        assert(!cfd_obstacle3d_pressure_trace_diagnostic(&s, NULL, cells, &out));
        assert(!cfd_obstacle3d_pressure_trace_diagnostic(&s, p, cells - 1, &out));
        assert(!cfd_obstacle3d_pressure_trace_diagnostic(&s, p, cells, NULL));
        assert(!memcmp(&out, &sentinel, sizeof(out)));
        failed_controls += 4;
        assert(budget.successful_allocations == allocations);
        cfd_memory_free(p);
        cfd_obstacle_mixed3d_destroy(s.mixed);
        assert(budget.live_bytes == 0);
        cfd_memory_scope(previous);
    }
    assert(maximum < 1e-11 && maximum_gauge < 1e-11);
    assert(depths[0] && depths[1] && depths[2]);
    printf("{\"polynomial_face_controls\":%d,\"failure_controls\":%d,"
           "\"maximum_force_error_n\":%.17g,\"maximum_gauge_error_n\":%.17g,"
           "\"two_three_four_interval_patches\":[%zu,%zu,%zu],\"allocations\":0}\n",
           controls, failed_controls, maximum, maximum_gauge, depths[0], depths[1], depths[2]);
    return 0;
}
