/* Test-only cubic extrapolation of actual pressure interval means. */
static void candidate_pressure_load(const CfdObstacle3d *s, const double *p, int a,
                                    int direction, const int cell[3], double force[3]) {
    int c[4][3];
    for (int j = 0; j < 4; j++) {
        memcpy(c[j], cell, sizeof(c[j]));
        c[j][a] += j * direction;
    }
    double trace;
    if (cfd_obstacle_mixed3d_cell(s->mixed, c[3][0], c[3][1], c[3][2]) >= 0)
        trace = (25 * pressure(s, p, c[0]) - 23 * pressure(s, p, c[1]) +
                 13 * pressure(s, p, c[2]) - 3 * pressure(s, p, c[3])) / 12;
    else if (cfd_obstacle_mixed3d_cell(s->mixed, c[2][0], c[2][1], c[2][2]) >= 0)
        trace = (11 * pressure(s, p, c[0]) - 7 * pressure(s, p, c[1]) +
                 2 * pressure(s, p, c[2])) / 6;
    else
        trace = 1.5 * pressure(s, p, c[0]) - .5 * pressure(s, p, c[1]);
    force[a] -= trace * direction * s->grid.area[a];
}
