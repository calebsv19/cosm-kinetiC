#include "app/cfd_momentum_flux.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static double run(int n, int square, double speed) {
    double *q = calloc(n, sizeof(double)), *next = calloc(n, sizeof(double)),
           *f = calloc(n, sizeof(double));
    assert(q && next && f);
    double pi = acos(-1.), mass = 0;
    for (int i = 0; i < n; i++) {
        double x = (i + .5) / n;
        q[i] = square ? (x > .25 && x < .5) : .5 + .25 * sin(2 * pi * x);
        mass += q[i];
    }
    int ticks = square ? 5 * n : n * n;
    double dt = square ? .4 / n : .1 / (n * n);
    for (int t = 0; t < ticks; t++) {
        for (int i = 0; i < n; i++)
            f[i] = cfd_limited_momentum_flux(q[(i + n - 1) % n], q[i], q[(i + 1) % n],
                                             q[(i + 2) % n], speed);
        for (int i = 0; i < n; i++)
            next[i] = q[i] - dt * n * (f[i] - f[(i + n - 1) % n]);
        for (int i = 0; i < n; i++) {
            q[i] = next[i];
            assert(q[i] >= -1e-12 && q[i] <= 1 + 1e-12);
        }
    }
    double sum = 0, error = 0;
    for (int i = 0; i < n; i++) {
        sum += q[i];
        error += fabs(q[i] - (.5 + .25 * sin(2 * pi * ((i + .5) / n - speed * .1))));
    }
    assert(fabs(sum - mass) < 1e-9);
    free(q);
    free(next);
    free(f);
    return error / n;
}
int main(void) {
    run(128, 1, 1);
    run(128, 1, -1);
    double prior = 1;
    for (int n = 32; n <= 128; n *= 2) {
        double e = run(n, 0, 1);
        printf("n=%d smooth_l1=%.12g refinement_ratio=%.12g\n", n, e, prior / e);
        if (n > 32)
            assert(prior / e > 3);
        prior = e;
    }
    for (int a = -3; a <= 3; a++)
        for (int b = -3; b <= 3; b++)
            for (int c = -3; c <= 3; c++)
                for (int d = -3; d <= 3; d++)
                    for (int sign = -1; sign <= 1; sign += 2) {
                        double face = cfd_limited_momentum_flux(a, b, c, d, sign) / sign;
                        assert(face >= fmin(b, c) - 1e-12 && face <= fmax(b, c) + 1e-12);
                    }
    puts("Limited reconstruction: bounded faces, conservative square advection, smooth refinement "
         "passed");
}
