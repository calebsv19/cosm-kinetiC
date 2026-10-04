/* Test-only interval-average wall derivative; production observer is unchanged. */
static void candidate_trace(const CfdObstacle3d *s, const double *u, int a, int direction,
                            const int cell[3], double force[3]) {
    int far_cell[3] = {cell[0], cell[1], cell[2]};
    far_cell[a] += direction;
    if (cfd_obstacle_mixed3d_cell(s->mixed, far_cell[0], far_cell[1], far_cell[2]) < 0) {
        cfd_obstacle3d_trace_viscous(s, u, a, direction, cell, force);
        return;
    }
    int b = (a + 1) % 3, c = (a + 2) % 3;
    double delta[3] = {s->grid.h[0] / 2, s->grid.h[1] / 2, s->grid.h[2] / 2};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            int near[3] = {2 * cell[0], 2 * cell[1], 2 * cell[2]}, far[3];
            near[a] = 2 * (cell[a] + (direction < 0)) + direction;
            near[b] += i;
            near[c] += j;
            memcpy(far, near, sizeof(far));
            far[a] += 2 * direction;
            double v[3], w[3];
            node(s, u, near, v);
            node(s, u, far, w);
            double area = delta[b] * delta[c] * (i == 1 ? 1 : .5) * (j == 1 ? 1 : .5);
            for (int component = 0; component < 3; component++)
                if (component != a)
                    force[component] += s->mu * (7 * v[component] - w[component]) /
                                        (2 * s->grid.h[a]) * area;
        }
}
