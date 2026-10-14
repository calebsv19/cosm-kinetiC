#include "app/cfd_hotgas3d.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int main(void) {
    CfdHotGas3d s;
    int n[3] = {4, 4, 4};
    double length[3] = {1, 1, 1};
    assert(cfd_hotgas3d_init(&s, n, length, 1.4, 287));
    double source[320] = {0}, accepted[320];
    for (int q = 0; q < 64; ++q) {
        s.state[5 * q] = 1;
        s.state[5 * q + 4] = 287 * 300 / .4;
    }
    memcpy(accepted, s.state, sizeof(accepted));
    assert(!cfd_hotgas3d_step(&s, source, 1));
    assert(memcmp(accepted, s.state, sizeof(accepted)) == 0);
    source[0] = -2;
    assert(!cfd_hotgas3d_step(&s, source, 1e-5));
    assert(memcmp(accepted, s.state, sizeof(accepted)) == 0);
    source[0] = NAN;
    assert(!cfd_hotgas3d_step(&s, source, 1e-5));
    assert(memcmp(accepted, s.state, sizeof(accepted)) == 0);
    source[0] = 0;
    source[4] = 1e12; /* CFL must include newly heated candidate sound speed. */
    assert(!cfd_hotgas3d_step(&s, source, 1e-5));
    assert(memcmp(accepted, s.state, sizeof(accepted)) == 0);
    source[4] = -s.state[4];
    assert(!cfd_hotgas3d_step(&s, source, 1e-5));
    assert(memcmp(accepted, s.state, sizeof(accepted)) == 0);
    source[4] = 1;
    assert(cfd_hotgas3d_step(&s, source, 1e-5));
    cfd_hotgas3d_destroy(&s);
    return 0;
}
