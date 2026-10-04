#include "app/cfd_obstacle3d_pressure_trace.h"
#include <math.h>

static bool patch(const CfdObstacle3d *s, const double *pressure, size_t cells, int axis,
                  int direction, const int fluid_cell[3], double *force, size_t counts[3]) {
    double values[4];
    int depth = 0;
    for (int d = 0; d < 4; d++) {
        int c[3] = {fluid_cell[0], fluid_cell[1], fluid_cell[2]};
        c[axis] += d * direction;
        int q = cfd_obstacle_mixed3d_cell(s->mixed, c[0], c[1], c[2]);
        if (q < 0)
            break;
        if ((size_t)q >= cells || !isfinite(pressure[q]))
            return false;
        values[depth++] = pressure[q];
    }
    if (depth < 2)
        return false;
    double trace;
    if (depth == 4)
        trace = (25 * values[0] - 23 * values[1] + 13 * values[2] - 3 * values[3]) / 12;
    else if (depth == 3)
        trace = (11 * values[0] - 7 * values[1] + 2 * values[2]) / 6;
    else
        trace = 1.5 * values[0] - .5 * values[1];
    *force = -trace * direction * s->grid.area[axis];
    if (!isfinite(*force))
        return false;
    counts[depth - 2]++;
    return true;
}

bool cfd_obstacle3d_pressure_trace_diagnostic(const CfdObstacle3d *s, const double *pressure,
                                             size_t cells, CfdObstacle3dPressureTrace *out) {
    if (!s || !s->mixed || !pressure || !out ||
        cells != (size_t)cfd_obstacle_mixed3d_cells(s->mixed))
        return false;
    for (int a = 0; a < 3; a++)
        if (s->lo[a] < 0 || s->hi[a] <= s->lo[a] || s->hi[a] >= s->grid.n[a] ||
            !isfinite(s->grid.area[a]) || s->grid.area[a] <= 0)
            return false;
    CfdObstacle3dPressureTrace result = {0};
    for (int a = 0; a < 3; a++)
        for (int side = 0; side < 2; side++)
            for (int k = s->lo[2]; k < s->hi[2]; k++)
                for (int j = s->lo[1]; j < s->hi[1]; j++)
                    for (int i = s->lo[0]; i < s->hi[0]; i++) {
                        int c[3] = {i, j, k};
                        if (c[a] != s->lo[a])
                            continue;
                        c[a] = side ? s->hi[a] : s->lo[a] - 1;
                        double force;
                        if (!patch(s, pressure, cells, a, side ? 1 : -1, c, &force,
                                   result.interval_depth_patches))
                            return false;
                        result.force_n[a] += force;
                        result.side_force_n[2 * a + side][a] += force;
                    }
    for (int a = 0; a < 3; a++)
        if (!isfinite(result.force_n[a]) || !isfinite(result.side_force_n[2 * a][a]) ||
            !isfinite(result.side_force_n[2 * a + 1][a]))
            return false;
    *out = result;
    return true;
}
