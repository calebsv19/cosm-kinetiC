#include "app/cfd_wall3d.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static bool cancel(void *c) {
    int *n = c;
    return ++(*n) < 4;
}
static void failure_cleanup(void) {
    for (int cap = 1; cap <= 512; cap *= 2) {
        CfdMemoryBudget b = {.limit_bytes = (size_t)cap * 1024};
        CfdMemoryBudget *prev = cfd_memory_scope(&b);
        int n[3] = {8, 8, 8};
        double l[3] = {2, 2.5, 3};
        CfdWall3d s;
        assert(!cfd_wall3d_init(&s, n, l, 1, .1, .01, false));
        cfd_wall3d_destroy(&s);
        assert(b.live_bytes == 0 && b.last_failure == CFD_MEMORY_LIMIT);
        cfd_memory_scope(prev);
    }
}
static void reference(void) {
    double l[3] = {2, 2.5, 3}, x[3] = {.43, 1.13, 2.11}, w[3] = {0}, v[3], d[3][3];
    int zero[3] = {0};
    for (int a = 0; a < 3; a++) {
        int du[3] = {0};
        du[a] = 1;
        cfd_wall3d_reference(l, x, w, du, v);
        for (int b = 0; b < 3; b++)
            d[b][a] = v[b];
    }
    assert(fabs(d[0][0] + d[1][1] + d[2][2]) < 1e-14);
    for (int a = 1; a < 3; a++)
        for (int side = 0; side < 2; side++) {
            double xx[3] = {x[0], x[1], x[2]};
            xx[a] = side ? l[a] : 0;
            cfd_wall3d_reference(l, xx, w, zero, v);
            for (int b = 0; b < 3; b++)
                assert(fabs(v[b]) < 1e-14);
        }
    double conv[3], u[3];
    cfd_wall3d_reference_transport(l, x, w, conv);
    cfd_wall3d_reference(l, x, w, zero, u);
    for (int a = 0; a < 3; a++) {
        double direct = 0;
        for (int b = 0; b < 3; b++)
            direct += u[b] * d[a][b];
        assert(fabs(conv[a] - direct) < 1e-14);
    }
    double h = 1e-5;
    for (int a = 0; a < 3; a++) {
        double xp[3] = {x[0], x[1], x[2]}, xm[3] = {x[0], x[1], x[2]}, vp[3], vm[3];
        xp[a] += h;
        xm[a] -= h;
        cfd_wall3d_reference(l, xp, w, zero, vp);
        cfd_wall3d_reference(l, xm, w, zero, vm);
        for (int b = 0; b < 3; b++)
            assert(fabs((vp[b] - vm[b]) / (2 * h) - d[b][a]) < 1e-10);
    }
    x[1] = 0;
    assert(fabs(cfd_wall3d_reference_pressure(l, x, w, 1)) > 1e-3);
}
static uint64_t field_hash(const double *u, int m, const double *p, int n) {
    uint64_t h = UINT64_C(1469598103934665603);
    for (int a = 0; a < 2; a++) {
        const unsigned char *v = (const unsigned char *)(a ? p : u);
        size_t size = (size_t)(a ? n : m) * sizeof(double);
        for (size_t q = 0; q < size; q++)
            h = (h ^ v[q]) * UINT64_C(1099511628211);
    }
    return h;
}
static void quadrature(const CfdWall3d *s) {
    const double nodes[8] = {.095012509837637440, .281603550779258913, .458016777657227386,
                             .617876244402643748, .755404408355003034, .865631202387831744,
                             .944575023073232576, .989400934991649933};
    const double weights[8] = {.189450610455068496, .182603415044923589, .169156519395002538,
                               .149595988816576732, .124628971255533872, .095158511682492785,
                               .062253523938647893, .027152459411754095};
    double kinetic = 0, dissipation = 0, width[3] = {0};
    int pieces = (int)lround(2 * s->grid.length[0] / s->reference_length[0]);
    for (int part = 0; part < pieces; part++)
        for (int i = 0; i < 16; i++)
            for (int j = 0; j < 16; j++)
                for (int k = 0; k < 16; k++) {
                    int q[3] = {i, j, k}, zero[3] = {0};
                    double x[3], volume = 1, u[3], d[3][3];
                    for (int a = 0; a < 3; a++) {
                        double L = a == 0 ? s->grid.length[0] / pieces : s->grid.length[a];
                        x[a] = .5 * L * (1 + (q[a] < 8 ? -1 : 1) * nodes[q[a] % 8]);
                        if (a == 0)
                            x[a] += part * L;
                        volume *= .5 * L * weights[q[a] % 8];
                    }
                    cfd_wall3d_reference(s->reference_length, x, width, zero, u);
                    for (int a = 0; a < 3; a++) {
                        int derivative[3] = {0};
                        double v[3];
                        derivative[a] = 1;
                        cfd_wall3d_reference(s->reference_length, x, width, derivative, v);
                        for (int b = 0; b < 3; b++)
                            d[b][a] = v[b];
                    }
                    for (int a = 0; a < 3; a++) {
                        kinetic += .5 * s->rho * u[a] * u[a] * volume;
                        for (int b = 0; b < 3; b++) {
                            double strain = .5 * (d[a][b] + d[b][a]);
                            dissipation += 2 * s->mu * strain * strain * volume;
                        }
                    }
                }
    assert(fabs(kinetic / s->kinetic_base - 1) < 1e-8);
    assert(fabs(dissipation / s->dissipation_base - 1) < 1e-8);
}
static void open_contract(void) {
    for (int size = 4; size <= 8; size *= 2)
        for (int L = 4; L <= 8; L += 2) {
            CfdMemoryBudget b = {.limit_bytes = 64 * 1024 * 1024};
            CfdMemoryBudget *prev = cfd_memory_scope(&b);
            int n[3] = {L * size / 2, size, size};
            double l[3] = {L, 2, 2};
            CfdWall3d s;
            assert(cfd_wall3d_init_open(&s, n, l, 1, .1, .01));
            assert(cfd_mixed3d_verify(s.mixed));
            quadrature(&s);
            /* An exact quadratic's face-area averages have derivative 2y
             * at cell centres, including both first and last wall cells. */
            for (int q = 0; q < s.count; q++) {
                int a, c[3];
                double x[3];
                cfd_mixed3d_position(s.mixed, q, &a, x, c);
                double h = s.grid.h[1], j = c[1];
                s.velocity[q] = a == 0 ? h * h * (j * j + j + 1. / 3) : 0;
            }
            for (int j = 0; j < n[1]; j++) {
                double d[3][3];
                cfd_wall3d_derivatives(&s, 1, j, 1, d);
                assert(fabs(d[0][1] - (2 * j + 1) * s.grid.h[1]) < 1e-12);
            }
            memcpy(s.velocity, s.face_reference, (size_t)s.count * sizeof(double));
            assert(cfd_wall3d_step(&s) && cfd_wall3d_step(&s));
            size_t allocations = b.successful_allocations;
            assert(cfd_wall3d_step(&s));
            assert(allocations == b.successful_allocations);
            double time = s.time;
            uint64_t hash = field_hash(s.velocity, s.count, s.pressure, s.grid.count);
            int calls = 0;
            cfd_mixed3d_checkpoint(s.mixed, cancel, &calls);
            assert(!cfd_wall3d_step(&s));
            assert(s.time == time &&
                   hash == field_hash(s.velocity, s.count, s.pressure, s.grid.count));
            assert(!strcmp(s.error, "cancelled_at_krylov_checkpoint"));
            cfd_wall3d_destroy(&s);
            assert(b.live_bytes == 0);
            cfd_memory_scope(prev);
        }
}
int main(void) {
    reference();
    failure_cleanup();
    open_contract();
    CfdMemoryBudget b = {.limit_bytes = 64 * 1024 * 1024};
    CfdMemoryBudget *prev = cfd_memory_scope(&b);
    int n[3] = {8, 8, 8};
    double l[3] = {2, 2.5, 3};
    CfdWall3d s;
    assert(cfd_wall3d_init(&s, n, l, 1, .1, .01, true));
#ifdef CFD_MIXED3D_VERIFY
    assert(cfd_mixed3d_verify(s.mixed));
#endif
    quadrature(&s);
    assert(cfd_wall3d_step(&s));
    assert(cfd_wall3d_step(&s));
    size_t count = b.successful_allocations;
    assert(cfd_wall3d_step(&s));
    assert(count == b.successful_allocations);
    double time = s.time, old = s.velocity[0], p = s.pressure[0];
    uint64_t hash = field_hash(s.velocity, s.count, s.pressure, s.grid.count);
    int calls = 0;
    cfd_mixed3d_checkpoint(s.mixed, cancel, &calls);
    assert(!cfd_wall3d_step(&s));
    assert(s.time == time && s.velocity[0] == old && s.pressure[0] == p);
    assert(hash == field_hash(s.velocity, s.count, s.pressure, s.grid.count));
    assert(!strcmp(s.error, "cancelled_at_krylov_checkpoint"));
    cfd_wall3d_destroy(&s);
    assert(b.live_bytes == 0);
    assert(cfd_wall3d_init(&s, n, l, 1, .1, .1, true));
    for (int q = 0; q < s.count; q++)
        s.velocity[q] *= 100;
    time = s.time;
    old = s.velocity[0];
    assert(!cfd_wall3d_step(&s));
    assert(s.time == time && s.velocity[0] == old);
    assert(!strcmp(s.error, "wall_transport_cfl_exceeded"));
    cfd_wall3d_destroy(&s);
    assert(b.live_bytes == 0);
    cfd_memory_scope(prev);
    puts("wall reference derivatives, nonzero wall pressure gradient, conservative transport, "
         "cached steps, safe cancellation, CFL and allocation cleanup passed");
    return 0;
}
