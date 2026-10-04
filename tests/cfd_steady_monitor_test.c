#include "app/cfd_steady_monitor.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
    CfdOpen2D c = {.nx = 4, .ny = 4, .inlet_mean = 1};
    double u[20] = {0}, v[20] = {0};
    c.u = u;
    c.v = v;
    CfdSteadyMonitor m;
    assert(cfd_steady_init(&m, &c));
    for (int k = 1; k <= 70; k++) {
        c.time = k * .1;
        cfd_steady_observe(&m, &c);
    }
    assert(cfd_steady_passed(&m));
    c.time = 7.1;
    u[9] = .01;
    cfd_steady_observe(&m, &c);
    assert(!cfd_steady_passed(&m));
    c.time = 7.2;
    u[9] = 0;
    cfd_steady_observe(&m, &c);
    assert(!cfd_steady_passed(&m));
    for (int k = 73; k <= 100; k++) {
        c.time = k * .1;
        cfd_steady_observe(&m, &c);
    }
    assert(cfd_steady_passed(&m));
    c.time = 10.1;
    v[2] = NAN;
    cfd_steady_observe(&m, &c);
    assert(!cfd_steady_passed(&m));
    cfd_steady_destroy(&m);
    puts("Every-step stationarity: startup, recovery, interior oscillation and nonfinite rejection "
         "passed");
}
