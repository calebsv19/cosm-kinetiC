#include "app/cfd_hotgas_domain3d.h"
#include "app/cfd_hotgas_flux3d.h"
#include "app/cfd_hotgas_reconstruction3d.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int main(void) {
    CfdHotGas3d s;
    int n[3] = {4, 4, 4};
    double length[3] = {.004, .004, .004};
    assert(cfd_hotgas3d_init(&s, n, length, 1.4, 287));
    CfdHotGasDomain d = {0};
    for (int f = 0; f < 6; ++f)
        d.face[f].kind = CFD_HOTGAS_SLIP_WALL;
    double source[320] = {0}, accepted[320];
    for (int q = 0; q < 64; ++q) {
        s.state[5 * q] = .001;
        s.state[5 * q + 4] = .001 * 287 * 300 / .4;
    }
    memcpy(accepted, s.state, sizeof(accepted));
    CfdHotGasDomainBudget budget, before;
    memset(&budget, 0xa5, sizeof(budget));
    memcpy(&before, &budget, sizeof(before));
    d.conductivity_w_m_k = 100;
    assert(!cfd_hotgas_domain3d_step(&s, &d, source, 1e-7, &budget));
    assert(memcmp(s.state, accepted, sizeof(accepted)) == 0 &&
           memcmp(&budget, &before, sizeof(before)) == 0);
    d.conductivity_w_m_k = 0;
    assert(!cfd_hotgas_domain3d_step(&s, &d, source, 1, &budget));
    assert(memcmp(s.state, accepted, sizeof(accepted)) == 0 &&
           memcmp(&budget, &before, sizeof(before)) == 0);
    source[0] = -.002;
    assert(!cfd_hotgas_domain3d_step(&s, &d, source, 1e-7, &budget));
    assert(memcmp(s.state, accepted, sizeof(accepted)) == 0 &&
           memcmp(&budget, &before, sizeof(before)) == 0);
    source[0] = NAN;
    assert(!cfd_hotgas_domain3d_step(&s, &d, source, 1e-7, &budget));
    assert(memcmp(s.state, accepted, sizeof(accepted)) == 0 &&
           memcmp(&budget, &before, sizeof(before)) == 0);
    source[0] = 0;
    source[4] = 1e12;
    assert(!cfd_hotgas_domain3d_step(&s, &d, source, 1e-7, &budget));
    assert(memcmp(s.state, accepted, sizeof(accepted)) == 0 &&
           memcmp(&budget, &before, sizeof(before)) == 0);
    source[4] = 0;
    d.gravity_m_s2[2] = -9.81;
    d.face[4].kind = CFD_HOTGAS_PERIODIC;
    assert(!cfd_hotgas_domain3d_step(&s, &d, source, 1e-7, &budget));
    assert(memcmp(s.state, accepted, sizeof(accepted)) == 0 &&
           memcmp(&budget, &before, sizeof(before)) == 0);
    d.face[4].kind = CFD_HOTGAS_SLIP_WALL;
    d.contact_flux = 1;
    d.conductivity_w_m_k = 100;
    assert(!cfd_hotgas_domain3d_step(&s, &d, source, 1e-7, &budget));
    assert(memcmp(s.state, accepted, sizeof(accepted)) == 0 &&
           memcmp(&budget, &before, sizeof(before)) == 0);
    d.conductivity_w_m_k = 0;
    d.contact_flux = 4;
    assert(!cfd_hotgas_domain3d_step(&s, &d, source, 1e-7, &budget));
    assert(memcmp(s.state, accepted, sizeof(accepted)) == 0 &&
           memcmp(&budget, &before, sizeof(before)) == 0);
    d.contact_flux = 1;
    assert(cfd_hotgas_domain3d_step(&s, &d, source, 1e-7, &budget));
    /* Stage one remains positive, but stage two would remove more mass than
     * exists. RK2 must refuse without publishing its intermediate state. */
    d.contact_flux = 2;
    d.gravity_m_s2[2] = 0;
    for (int q = 0; q < 64; ++q) {
        memset(s.state + 5 * q, 0, 5 * sizeof(double));
        s.state[5 * q] = .001;
        s.state[5 * q + 4] = .001 * 287 * 300 / .4;
        source[5 * q] = -.0006;
    }
    memcpy(accepted, s.state, sizeof(accepted));
    memcpy(&before, &budget, sizeof(before));
    assert(!cfd_hotgas_domain3d_step(&s, &d, source, 1e-8, &budget));
    assert(memcmp(s.state, accepted, sizeof(accepted)) == 0 &&
           memcmp(&budget, &before, sizeof(before)) == 0);
    /* Constant volumetric heating enters exactly once across the two stages. */
    for (int q = 0; q < 64; ++q) {
        source[5 * q] = 0;
        source[5 * q + 4] = 1;
    }
    assert(cfd_hotgas_domain3d_step(&s, &d, source, 1e-8, &budget));
    for (int q = 0; q < 64; ++q) {
        assert(s.state[5 * q] == accepted[5 * q]);
        assert(fabs(s.state[5 * q + 4] - accepted[5 * q + 4] - 1) < 1e-10);
    }
    for (int f = 0; f < 6; ++f)
        assert(budget.outward[f][0] == 0 && budget.outward[f][4] == 0);
    d.contact_flux = 3;
    for (int q = 0; q < 64; ++q) {
        source[5 * q] = -.0006;
        source[5 * q + 4] = 0;
    }
    memcpy(accepted, s.state, sizeof(accepted));
    memcpy(&before, &budget, sizeof(before));
    assert(!cfd_hotgas_domain3d_step(&s, &d, source, 1e-8, &budget));
    assert(memcmp(s.state, accepted, sizeof(accepted)) == 0 &&
           memcmp(&budget, &before, sizeof(before)) == 0);
    for (int q = 0; q < 64; ++q) {
        source[5 * q] = 0;
        source[5 * q + 4] = 1;
    }
    assert(cfd_hotgas_domain3d_step(&s, &d, source, 1e-8, &budget));
    for (int q = 0; q < 64; ++q)
        assert(fabs(s.state[5 * q + 4] - accepted[5 * q + 4] - 1) < 1e-10);
    double left[5] = {1, -10000, 0, 0, 50000000 + 287 * 300 / .4};
    double right[5] = {1, 10000, 0, 0, 50000000 + 287 * 300 / .4};
    double output[5] = {11, 22, 33, 44, 55}, saved[5], speed = 7;
    memcpy(saved, output, sizeof(saved));
    assert(!cfd_hotgas_hllc3d(&s, left, right, 0, output, &speed));
    assert(memcmp(saved, output, sizeof(saved)) == 0 && speed == 7);
    assert(!cfd_hotgas_lhllc3d(&s, left, right, 0, 1, output, &speed));
    assert(memcmp(saved, output, sizeof(saved)) == 0 && speed == 7);
    /* Isolated advecting contact and shear in each normal direction: exact
     * upwind flux when both states share pressure and normal speed. */
    for (int axis = 0; axis < 3; ++axis) {
        for (int sign = -1; sign <= 1; sign += 2) {
            for (int side = 0; side < 2; ++side) {
                double *u = side ? right : left, rho = side ? .5 : 1;
                u[0] = rho;
                u[4] = 101325 / .4;
                for (int a = 0; a < 3; ++a) {
                    double v = a == axis ? sign * 2 : side * 3;
                    u[a + 1] = rho * v;
                    u[4] += .5 * rho * v * v;
                }
            }
            assert(cfd_hotgas_hllc3d(&s, left, right, axis, output, &speed));
            double *u = sign > 0 ? left : right, velocity = sign * 2;
            double exact[5] = {u[axis + 1], u[1] * velocity, u[2] * velocity,
                               u[3] * velocity, (u[4] + 101325) * velocity};
            exact[axis + 1] += 101325;
            for (int i = 0; i < 5; ++i)
                assert(fabs(output[i] - exact[i]) < 1e-7);
            assert(cfd_hotgas_lhllc3d(&s, left, right, axis, 1, output, &speed));
            for (int i = 0; i < 5; ++i)
                assert(fabs(output[i] - exact[i]) < 1e-7);
        }
    }
    /* Independent symmetric compression: normal momentum is the corrected
     * pressure; physical sound speed is unchanged and damping scales as v^2. */
    double sound = sqrt(1.4 * 101325), previous = 0;
    for (int k = 0; k < 3; ++k) {
        double velocity = pow(.1, k), chi = velocity / sound;
        double expected = 101325 + chi * (2 - chi) * (sound + 2 * velocity) * velocity;
        for (int side = 0; side < 2; ++side) {
            double *u = side ? right : left;
            u[0] = 1; u[1] = side ? -velocity : velocity; u[2] = u[3] = 0;
            u[4] = 101325 / .4 + .5 * velocity * velocity;
        }
        assert(cfd_hotgas_lhllc3d(&s, left, right, 0, 1, output, &speed));
        assert(fabs(output[1] - expected) < 1e-8);
        assert(output[0] == 0 && output[4] == 0);
        assert(fabs(speed - sound - velocity) < 1e-10);
        double correction = output[1] - 101325;
        if (k)
            assert(previous / correction > 99 && previous / correction < 101);
        previous = correction;
    }
    /* Independent linear-gradient sensor oracles at an interior cell. */
    for (int kind = 0; kind < 4; ++kind) {
        for (int q = 0; q < s.grid.count; ++q) {
            double x[3]; cfd_cartesian3d_position(&s.grid, q, -1, x);
            double v[3] = {0, 0, 0};
            if (kind == 1 || kind == 3) { v[0] = -x[1]; v[1] = x[0]; }
            if (kind == 2 || kind == 3)
                for (int a = 0; a < 3; ++a) v[a] -= x[a];
            s.state[5 * q] = 1; s.state[5 * q + 4] = 101325 / .4;
            for (int a = 0; a < 3; ++a) {
                s.state[5 * q + a + 1] = v[a];
                s.state[5 * q + 4] += .5 * v[a] * v[a];
            }
        }
        double expected = kind < 2 ? 0 : kind == 2 ? 1 : 9. / 13;
        assert(fabs(cfd_hotgas_compression3d(&s, &d, 21) - expected) < 1e-12);
    }
    cfd_hotgas3d_destroy(&s);
    return 0;
}
