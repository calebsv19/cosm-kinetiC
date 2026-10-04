#include "app/cfd_obstacle3d.h"
#include <math.h>
#include <string.h>
/* One continuous, piecewise trilinear interpolant on the half-cell lattice.
 * Actual no-slip planes/edges are zero nodes. No matrix work, reference force,
 * or balance residual enters this physical quadrature. */
static void node(const CfdObstacle3d *s, const double *u, const int half[3], double v[3]) {
    const CfdCartesian3d *g = &s->grid;
    bool body = true;
    for (int a = 0; a < 3; a++)
        if (half[a] < 2 * s->lo[a] || half[a] > 2 * s->hi[a])
            body = false;
    if (body || half[1] == 0 || half[1] == 2 * g->n[1] || half[2] == 0 || half[2] == 2 * g->n[2]) {
        memset(v, 0, 3 * sizeof(double));
        return;
    }
    for (int a = 0; a < 3; a++) {
        int lo[3], hi[3];
        double f[3];
        v[a] = 0;
        for (int b = 0; b < 3; b++) {
            double x = .5 * half[b] - (a == b ? 0 : .5), upper = g->n[b] - (a == b ? 0 : 1);
            x = fmax(0, fmin(upper, x));
            lo[b] = (int)floor(x);
            hi[b] = (int)fmin(lo[b] + 1, upper);
            f[b] = x - lo[b];
        }
        for (int i = 0; i <= 1; i++)
            for (int j = 0; j <= 1; j++)
                for (int k = 0; k <= 1; k++) {
                    double w =
                        (i ? f[0] : 1 - f[0]) * (j ? f[1] : 1 - f[1]) * (k ? f[2] : 1 - f[2]);
                    if (w == 0)
                        continue;
                    int q = cfd_obstacle_mixed3d_index(s->mixed, a, i ? hi[0] : lo[0],
                                                       j ? hi[1] : lo[1], k ? hi[2] : lo[2]);
                    if (q >= 0)
                        v[a] += w * u[q];
                }
    }
}
void cfd_obstacle3d_trace_viscous(const CfdObstacle3d *s, const double *u, int a, int direction,
                                  const int cell[3], double force[3]) {
    int b = (a + 1) % 3, c = (a + 2) % 3;
    double delta[3] = {s->grid.h[0] / 2, s->grid.h[1] / 2, s->grid.h[2] / 2};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            int at[3] = {2 * cell[0], 2 * cell[1], 2 * cell[2]};
            at[a] = 2 * (cell[a] + (direction < 0)) + direction;
            at[b] += i;
            at[c] += j;
            double v[3];
            node(s, u, at, v);
            double area = delta[b] * delta[c] * (i == 1 ? 1 : .5) * (j == 1 ? 1 : .5);
            for (int component = 0; component < 3; component++) {
                /* Exact flat no-slip trace and div u=0 fix normal derivative to
                 * zero; tangential derivatives of the wall trace also vanish. */
                if (component != a)
                    force[component] += s->mu * v[component] / delta[a] * area;
            }
        }
}
static double cell_energy(const double v[8][3], const double delta[3], double mu, double *div2) {
    double linear[3][3] = {{0}}, quad[3][3][3] = {{{0}}}, cubic[3] = {0};
    for (int corner = 0; corner < 8; corner++) {
        int sign[3] = {corner & 1 ? 1 : -1, corner & 2 ? 1 : -1, corner & 4 ? 1 : -1};
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++)
                linear[a][b] += .25 * sign[b] * v[corner][a];
            for (int b = 0; b < 3; b++)
                for (int c = b + 1; c < 3; c++) {
                    double x = .5 * sign[b] * sign[c] * v[corner][a];
                    quad[a][b][c] += x;
                    quad[a][c][b] += x;
                }
            cubic[a] += sign[0] * sign[1] * sign[2] * v[corner][a];
        }
    }
    double density = 0, div = 0, divergence = 0;
    for (int a = 0; a < 3; a++) {
        double norm = linear[a][a] * linear[a][a];
        div += linear[a][a] / delta[a];
        for (int c = 0; c < 3; c++)
            if (c != a)
                norm += quad[a][a][c] * quad[a][a][c] / 12;
        norm += cubic[a] * cubic[a] / 144;
        density += 2 * mu * norm / (delta[a] * delta[a]);
    }
    divergence = div * div;
    for (int c = 0; c < 3; c++) {
        double sum = 0;
        for (int a = 0; a < 3; a++)
            if (a != c)
                sum += quad[a][a][c] / delta[a];
        divergence += sum * sum / 12;
    }
    for (int a = 0; a < 3; a++)
        divergence += cubic[a] * cubic[a] / (144 * delta[a] * delta[a]);
    for (int a = 0; a < 3; a++)
        for (int b = a + 1; b < 3; b++) {
            double mean = linear[a][b] / delta[b] + linear[b][a] / delta[a], norm = mean * mean;
            for (int c = 0; c < 3; c++) {
                double q = quad[a][b][c] / delta[b] + quad[b][a][c] / delta[a];
                norm += q * q / 12;
            }
            norm += (cubic[a] * cubic[a] / (delta[b] * delta[b]) +
                     cubic[b] * cubic[b] / (delta[a] * delta[a])) /
                    144;
            density += mu * norm;
        }
    double volume = delta[0] * delta[1] * delta[2];
    *div2 += divergence * volume;
    return density * volume;
}
void cfd_obstacle3d_reconstruct_strain(CfdObstacle3d *s, const double *u) {
    int sx = 2 * s->grid.n[0] + 1, sy = 2 * s->grid.n[1] + 1;
    size_t plane = (size_t)sx * sy * 3;
    double delta[3] = {s->grid.h[0] / 2, s->grid.h[1] / 2, s->grid.h[2] / 2};
    double D = 0, div2 = 0;
    for (int z = 0; z <= 2 * s->grid.n[2]; z++) {
        double *current = s->reconstruction_planes + (z % 3) * plane;
        for (int y = 0; y < sy; y++)
            for (int x = 0; x < sx; x++) {
                int at[3] = {x, y, z};
                node(s, u, at, current + 3 * ((size_t)y * sx + x));
            }
        if (z == 0)
            continue;
        double *below = s->reconstruction_planes + ((z - 1) % 3) * plane;
        for (int y = 0; y < sy - 1; y++)
            for (int x = 0; x < sx - 1; x++) {
                if (x >= 2 * s->lo[0] && x < 2 * s->hi[0] && y >= 2 * s->lo[1] &&
                    y < 2 * s->hi[1] && z - 1 >= 2 * s->lo[2] && z - 1 < 2 * s->hi[2])
                    continue;
                double values[8][3];
                for (int k = 0; k < 2; k++)
                    for (int j = 0; j < 2; j++)
                        for (int i = 0; i < 2; i++)
                            memcpy(values[(k << 2) | (j << 1) | i],
                                   (k ? current : below) + 3 * ((size_t)(y + j) * sx + x + i),
                                   3 * sizeof(double));
                D += cell_energy(values, delta, s->mu, &div2);
            }
    }
    s->physical_dissipation = D;
    s->reconstruction_divergence_l2 = sqrt(div2);
}
