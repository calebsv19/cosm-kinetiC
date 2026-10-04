#include "app/cfd_cartesian3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double dot(const double *a, const double *b, int n) {
    double s = 0;
    for (int i = 0; i < n; i++)
        s += a[i] * b[i];
    return s;
}
static void check(int axis) {
    int n[3] = {7, 9, 11};
    double length[3] = {1.7, 2.3, 4.1};
    CfdCartesian3d g;
    assert(cfd_cartesian3d_init(&g, n, length));
    int count = g.count;
    double *all = calloc((size_t)9 * count, sizeof(double));
    assert(all);
    double *p = all, *v = p + count, *grad = v + 3 * count, *div = grad + 3 * count,
           *out = div + count;
    for (int q = 0; q < count; q++) {
        double xyz[3];
        cfd_cartesian3d_position(&g, q, -1, xyz);
        p[q] = sin(2 * 3.141592653589793 * xyz[axis] / length[axis]);
        for (int a = 0; a < 3; a++)
            v[a * count + q] = sin(.17 * (q + 1) * (a + 1)) + cos(.21 * q);
    }
    cfd_cartesian3d_gradient(&g, p, grad);
    cfd_cartesian3d_divergence(&g, v, div);
    double lhs = dot(p, div, count), rhs = dot(grad, v, 3 * count), sum = 0;
    for (int q = 0; q < count; q++)
        sum += div[q] * g.volume;
    assert(fabs(lhs + rhs) / fmax(1, fabs(lhs)) < 1e-12);
    assert(fabs(sum) < 1e-12);
    double error = 0;
    for (int q = 0; q < count; q++) {
        double xyz[3];
        cfd_cartesian3d_position(&g, q, axis, xyz);
        double expected = 2 * sin(3.141592653589793 / n[axis]) / g.h[axis] *
                          cos(2 * 3.141592653589793 * xyz[axis] / length[axis]);
        for (int a = 0; a < 3; a++)
            error = fmax(error, fabs(grad[a * count + q] - (a == axis ? expected : 0)));
    }
    assert(error < 1e-12);
    for (int q = 0; q < count; q++)
        p[q] = 7;
    cfd_cartesian3d_gradient(&g, p, grad);
    assert(dot(grad, grad, 3 * count) == 0);
    CfdMemoryBudget budget = {.limit_bytes = 32 * 1024 * 1024};
    CfdMemoryBudget *prev = cfd_memory_scope(&budget);
    bool periodic[3] = {true, true, true};
    CfdCartesian3dLinear *s = cfd_cartesian3d_linear_create(&g, periodic, 2, .13, false);
    assert(s);
    for (int q = 0; q < count; q++)
        p[q] = sin(.31 * q) + .3 * cos(.14 * q);
    cfd_cartesian3d_linear_apply(s, p, out);
    int iterations;
    double residual;
    size_t allocations = budget.successful_allocations;
    assert(cfd_cartesian3d_linear_solve(s, out, div, &iterations, &residual));
    assert(budget.successful_allocations == allocations);
    for (int q = 0; q < count; q++)
        assert(fabs(p[q] - div[q]) < 1e-9);
    printf("axis=%d adjoint=%.3g conservation=%.3g gradient=%.3g pcg_iterations=%d "
           "true_residual=%.3g peak=%zu\n",
           axis, fabs(lhs + rhs), fabs(sum), error, iterations, residual, budget.peak_bytes);
    cfd_cartesian3d_linear_destroy(s);
    assert(budget.live_bytes == 0);
    cfd_memory_scope(prev);
    free(all);
}
int main(void) {
    for (int a = 0; a < 3; a++)
        check(a);
    return 0;
}
