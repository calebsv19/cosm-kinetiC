#include "app/cfd_open2d.h"
#include "app/cfd_open2d_budget.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static void seed(CfdOpen2D *c) {
    double pi = acos(-1.), dx = c->length / c->nx, dy = c->height / c->ny;
    for (int j = 0; j < c->ny; j++)
        for (int i = 1; i <= c->nx; i++) {
            double x = i * dx, y = (j + .5) * dy;
            c->u[j * (c->nx + 1) + i] += .004 * exp(-pow((x - 1) / .3, 2)) * pi * sin(2 * pi * y);
        }
    for (int j = 1; j < c->ny; j++)
        for (int i = 0; i < c->nx; i++) {
            double x = (i + .5) * dx, y = j * dy;
            c->v[j * c->nx + i] =
                .004 * 2 * (x - 1) / .09 * exp(-pow((x - 1) / .3, 2)) * pow(sin(pi * y), 2);
        }
}
static double energy(CfdOpen2D *a, CfdOpen2D *base) {
    double sum = 0;
    for (int j = 0; j < a->ny; j++)
        for (int i = 0; i <= a->nx; i++) {
            double q = a->u[j * (a->nx + 1) + i] - base->u[j * (a->nx + 1) + i];
            sum += q * q * (i == 0 || i == a->nx ? .5 : 1);
        }
    for (int k = 0; k < a->nx * (a->ny + 1); k++) {
        double q = a->v[k] - base->v[k];
        sum += q * q;
    }
    return sum * a->length / a->nx * a->height / a->ny;
}
typedef struct Audit {
    CfdOpen2DBudget first, last;
    double net_work, pressure_work, viscous_work, kinetic_flux, dissipation;
    double net_impulse;
    int count;
} Audit;
static double power(CfdOpen2DBudget b) {
    return b.pressure_work_w + b.viscous_work_w - b.outward_kinetic_flux_w - b.dissipation_w;
}
static double force(CfdOpen2DBudget b) {
    return b.pressure_force_x_n + b.normal_viscous_force_x_n + b.wall_force_x_n -
           b.outward_momentum_flux_n;
}
static void observe(Audit *a, CfdOpen2D *c, double dt) {
    CfdOpen2DBudget b;
    assert(cfd_open2d_budget(c, &b));
    if (a->count) {
        a->net_work += .5 * dt * (power(a->last) + power(b));
        a->net_impulse += .5 * dt * (force(a->last) + force(b));
        a->pressure_work += .5 * dt * (a->last.pressure_work_w + b.pressure_work_w);
        a->viscous_work += .5 * dt * (a->last.viscous_work_w + b.viscous_work_w);
        a->kinetic_flux += .5 * dt * (a->last.outward_kinetic_flux_w + b.outward_kinetic_flux_w);
        a->dissipation += .5 * dt * (a->last.dissipation_w + b.dissipation_w);
    } else
        a->first = b;
    a->last = b;
    a->count++;
}
static void audit_result(Audit a, Audit b, int n, int length) {
    double initial = a.first.kinetic_energy_j - b.first.kinetic_energy_j;
    double final = a.last.kinetic_energy_j - b.last.kinetic_energy_j;
    double residual = a.net_work - b.net_work - (final - initial);
    double momentum = a.net_impulse - b.net_impulse -
                      ((a.last.momentum_x_kg_m_s - a.first.momentum_x_kg_m_s) -
                       (b.last.momentum_x_kg_m_s - b.first.momentum_x_kg_m_s));
    assert(initial > 0 && isfinite(residual));
    printf("budget n=%d length=%d initial_excess_energy=%.12g final_excess_energy=%.12g "
           "pressure_work=%.12g viscous_work=%.12g kinetic_flux=%.12g dissipation=%.12g "
           "energy_residual=%.12g relative_energy_residual=%.12g momentum_residual=%.12g\n",
           n, length, initial, final, a.pressure_work - b.pressure_work,
           a.viscous_work - b.viscous_work, a.kinetic_flux - b.kinetic_flux,
           a.dissipation - b.dissipation, residual, residual / initial, momentum);
    fflush(stdout);
#if !defined(CFD_OPEN2D_VERIFY_CENTERED) && !defined(CFD_OPEN2D_VERIFY_DONOR) &&                   \
    !defined(CFD_OPEN2D_VERIFY_COPY_OUTLET)
    if (n == 32)
        assert(fabs(residual) / initial < .02);
#endif
}
int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 24, ny = n == 24 ? 16 : 24;
    assert(n == 24 || n == 32);
    double dt = argc > 2 ? atof(argv[2]) : .002;
    assert(dt == .002 || dt == .001);
    CfdOpen2D short_p, short_b, long_p, long_b;
    assert(cfd_open2d_init(&short_p, n, ny, 2, 1, .5, 1, .01, .2));
    assert(cfd_open2d_init(&short_b, n, ny, 2, 1, .5, 1, .01, .2));
    assert(cfd_open2d_init(&long_p, 2 * n, ny, 4, 1, .5, 1, .01, .2));
    assert(cfd_open2d_init(&long_b, 2 * n, ny, 4, 1, .5, 1, .01, .2));
    seed(&short_p);
    seed(&long_p);
    double initial = energy(&short_p, &short_b), peak_difference = 0, late_difference = 0;
    double initial_peak = 0;
    for (int k = 0; k < (n + 1) * ny; k++)
        initial_peak = fmax(initial_peak, fabs(short_p.u[k] - short_b.u[k]));
    Audit audits[4] = {0};
    for (int t = 0; t < (int)lround(12 / dt); t++) {
        assert(cfd_open2d_step(&short_p, dt));
        assert(cfd_open2d_step(&short_b, dt));
        assert(cfd_open2d_step(&long_p, dt));
        assert(cfd_open2d_step(&long_b, dt));
        /* Start after the first projection; uninitialized t=0 pressure is not
         * used as a boundary-work observation. Baseline subtraction retains
         * the pulse-specific budget instead of hiding it under channel power. */
        observe(&audits[0], &short_p, dt);
        observe(&audits[1], &short_b, dt);
        observe(&audits[2], &long_p, dt);
        observe(&audits[3], &long_b, dt);
        double diff = 0;
        for (int j = 0; j < ny; j++)
            for (int i = 1; i <= n / 2; i++) {
                double a = short_p.u[j * (n + 1) + i] - short_b.u[j * (n + 1) + i],
                       b = long_p.u[j * (2 * n + 1) + i] - long_b.u[j * (2 * n + 1) + i];
                diff = fmax(diff, fabs(a - b));
            }
        peak_difference = fmax(peak_difference, diff);
        if ((t + 1) * dt >= 4)
            late_difference = fmax(late_difference, diff);
        if ((t + 1) % (int)lround(2 / dt) == 0) {
            printf("time=%g short_energy_ratio=%.12g long_energy_ratio=%.12g "
                   "upstream_difference=%.12g\n",
                   (t + 1) * dt, energy(&short_p, &short_b) / initial,
                   energy(&long_p, &long_b) / initial, diff);
            fflush(stdout);
        }
    }
    printf("initial_peak=%.12g peak_domain_difference=%.12g late_domain_difference=%.12g "
           "final_short_energy_ratio=%.12g\n",
           initial_peak, peak_difference, late_difference, energy(&short_p, &short_b) / initial);
    audit_result(audits[0], audits[1], n, 2);
    audit_result(audits[2], audits[3], n, 4);
    assert(isfinite(late_difference));
    assert(peak_difference / initial_peak < .01);
    assert(late_difference / initial_peak < .01);
    assert(energy(&short_p, &short_b) / initial < 1e-5);
    assert(energy(&short_p, &short_b) < .1 * energy(&long_p, &long_b));
#if defined(CFD_OPEN2D_VERIFY_CENTERED) || defined(CFD_OPEN2D_VERIFY_DONOR) ||                     \
    defined(CFD_OPEN2D_VERIFY_COPY_OUTLET)
    puts("Energy-budget rows are diagnostic; no energy acceptance inferred.");
#else
    if (n == 32)
        puts("Bounded pulse energy consistency passes 2 percent; no general outlet acceptance "
             "inferred.");
#endif
    puts("Bounded disturbance-exit/domain-extension gate passed; no general outlet acceptance "
         "inferred.");
    cfd_open2d_destroy(&short_p);
    cfd_open2d_destroy(&short_b);
    cfd_open2d_destroy(&long_p);
    cfd_open2d_destroy(&long_b);
}
