#include "app/cfd_duct3d.h"
#include "app/cfd_startup3d.h"
#include <math.h>
static const double pi = 3.14159265358979323846;
void cfd_startup3d_reference(CfdStartup3d *s, double time, int terms, double *profile) {
    const CfdCartesian3d *g = &s->grid;
    double H = g->length[1], W = g->length[2], L = g->length[0], nu = s->mu / s->rho, wall[4];
    s->reference_flow = s->gradient / s->mu * H * W * cfd_duct3d_reference_mean(H, W, 1024);
    cfd_duct3d_reference_walls(H, W, wall, 4096);
    for (int a = 0; a < 4; a++)
        s->reference_wall[a] = s->gradient * L * wall[a];
    s->reference_kinetic = s->reference_dissipation = s->reference_energy_rate = 0;
    if (profile)
        for (int q = 0; q < g->n[1] * g->n[2]; q++)
            profile[q] = s->steady_profile[q];
    for (int a = 0; a < terms; a++)
        for (int b = 0; b < terms; b++) {
            double n = 2 * a + 1, m = 2 * b + 1, ky = n * pi / H, kz = m * pi / W,
                   lambda = ky * ky + kz * kz;
            double c = 16 * s->gradient / (s->mu * pi * pi * n * m * lambda),
                   decay = exp(-nu * lambda * time), growth = -expm1(-nu * lambda * time);
            s->reference_flow -= c * decay * 2 / ky * 2 / kz;
            s->reference_wall[0] -= s->mu * L * c * decay * ky * 2 / kz;
            s->reference_wall[1] -= s->mu * L * c * decay * ky * 2 / kz;
            s->reference_wall[2] -= s->mu * L * c * decay * kz * 2 / ky;
            s->reference_wall[3] -= s->mu * L * c * decay * kz * 2 / ky;
            s->reference_kinetic += s->rho * L * H * W / 8 * c * c * growth * growth;
            s->reference_dissipation += s->mu * L * H * W / 4 * lambda * c * c * growth * growth;
            s->reference_energy_rate +=
                s->rho * L * H * W / 4 * c * c * growth * nu * lambda * decay;
            if (profile && decay > 1e-18)
                for (int k = 0; k < g->n[2]; k++)
                    for (int j = 0; j < g->n[1]; j++) {
                        double y = (j + .5) * g->h[1], z = (k + .5) * g->h[2],
                               sy = sin(ky * y) * sin(ky * g->h[1] / 2) / (ky * g->h[1] / 2),
                               sz = sin(kz * z) * sin(kz * g->h[2] / 2) / (kz * g->h[2] / 2);
                        profile[k * g->n[1] + j] -= c * decay * sy * sz;
                    }
        }
    s->reference_power = s->gradient * L * s->reference_flow;
}
