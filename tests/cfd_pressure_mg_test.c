#include "app/cfd_pressure_mg.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static void run(int nx, int ny) {
    int n = nx * ny;
    unsigned char *solid = calloc(n, 1);
    double *a = calloc((size_t)n * 5, sizeof(double));
    assert(a && solid);
    double *b = a + n, *ma = b + n, *mb = ma + n, *sum = mb + n;
    for (int j = ny / 3; j < 2 * ny / 3; j++)
        for (int i = nx / 3; i < 2 * nx / 3; i++)
            solid[j * nx + i] = 1;
    CfdPressureMG *m = cfd_pressure_mg_create(nx, ny, 4. / nx, 2. / ny, solid);
    assert(m);
    for (int k = 0; k < n; k++) {
        a[k] = sin(k * .73);
        b[k] = cos(k * .29);
        sum[k] = a[k] + b[k];
    }
    assert(cfd_pressure_mg_apply(m, a, ma));
    assert(cfd_pressure_mg_apply(m, b, mb));
    double ab = 0, ba = 0, positive = 0;
    for (int k = 0; k < n; k++) {
        ab += a[k] * mb[k];
        ba += b[k] * ma[k];
        positive += a[k] * ma[k];
    }
    assert(positive > 0);
    assert(fabs(ab - ba) < 1e-10 * (1 + fabs(ab)));
    assert(cfd_pressure_mg_apply(m, sum, b));
    for (int k = 0; k < n; k++)
        assert(fabs(b[k] - ma[k] - mb[k]) < 1e-10);
    printf("nx=%d ny=%d symmetry_error=%.12g positive_quadratic_form=%.12g\n", nx, ny,
           fabs(ab - ba), positive);
    cfd_pressure_mg_destroy(m);
    free(a);
    free(solid);
}
int main(void) {
    run(16, 16);
    run(17, 13);
    run(64, 32);
}
