#include "app/cfd_wall3d.h"
#include <math.h>
static const double pi = 3.14159265358979323846;
static double sinc(double x) { return fabs(x) < 1e-12 ? 1 : sin(x) / x; }
static double basis(double length, double x, double width, int axis, int derivative) {
    double k = 2 * pi / length;
    if (axis == 0)
        return pow(k, derivative) * sin(k * x + derivative * pi / 2) * sinc(k * width / 2);
    if (derivative == 0)
        return .5 - .5 * cos(k * x) * sinc(k * width / 2);
    return -.5 * pow(k, derivative) * cos(k * x + derivative * pi / 2) * sinc(k * width / 2);
}
void cfd_wall3d_reference(const double length[3], const double xyz[3], const double width[3],
                          const int derivative[3], double out[3]) {
    const double a[3] = {.011, .007, .013};
    const int plus[3] = {1, 2, 0}, minus[3] = {2, 0, 1}, ap[3] = {2, 0, 1}, am[3] = {1, 2, 0};
    for (int c = 0; c < 3; c++) {
        double p = a[ap[c]], m = a[am[c]];
        for (int b = 0; b < 3; b++) {
            p *= basis(length[b], xyz[b], width[b], b, derivative[b] + (b == plus[c]));
            m *= basis(length[b], xyz[b], width[b], b, derivative[b] + (b == minus[c]));
        }
        out[c] = p - m;
    }
}
double cfd_wall3d_reference_pressure(const double length[3], const double xyz[3],
                                     const double width[3], int derivative) {
    double product = 1;
    for (int a = 0; a < 3; a++) {
        int d = derivative == a;
        double k = pi / length[a], x = xyz[a], w = width[a];
        double v = a == 0   ? pow(2 * k, d) * cos(2 * k * x + .2 + d * pi / 2) * sinc(k * w)
                   : a == 1 ? pow(k, d) * sin(k * x + .3 + d * pi / 2) * sinc(k * w / 2) +
                                  .5 * pow(2 * k, d) * cos(2 * k * x + d * pi / 2) * sinc(k * w)
                            : pow(k, d) * cos(k * x + .4 + d * pi / 2) * sinc(k * w / 2) +
                                  .4 * pow(2 * k, d) * sin(2 * k * x + d * pi / 2) * sinc(k * w);
        product *= v;
    }
    return product;
}

void cfd_wall3d_reference_transport(const double length[3], const double x[3],
                                    const double width[3], double out[3]) {
    const double nodes[4] = {.183434642495649805, .525532409916328986, .79666647741362674,
                             .960289856497536232};
    const double weights[4] = {.362683783378361983, .313706645877887287, .222381034453374471,
                               .101228536290376259};
    double integral[3][3][3] = {0};
    for (int a = 0; a < 3; a++)
        for (int q = 0; q < 8; q++) {
            int j = q % 4;
            double xx = x[a] + (q < 4 ? -1 : 1) * .5 * width[a] * nodes[j], v[3];
            for (int d = 0; d < 3; d++)
                v[d] = basis(length[a], xx, 0, a, d);
            for (int d = 0; d < 3; d++)
                for (int e = 0; e < 3; e++)
                    integral[a][d][e] += .5 * weights[j] * v[d] * v[e];
        }
    const double coef[3][2] = {{.013, -.007}, {.011, -.013}, {.007, -.011}};
    const int derivative[3][2] = {{1, 2}, {2, 0}, {0, 1}};
    for (int a = 0; a < 3; a++) {
        out[a] = 0;
        for (int b = 0; b < 3; b++)
            for (int left = 0; left < 2; left++)
                for (int right = 0; right < 2; right++) {
                    double product = coef[b][left] * coef[a][right];
                    for (int c = 0; c < 3; c++)
                        product *= integral[c][c == derivative[b][left]]
                                           [(c == derivative[a][right]) + (c == b)];
                    out[a] += product;
                }
    }
}
