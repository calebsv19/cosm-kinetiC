#include "app/cfd_duct3d.h"
#include <math.h>
#include <string.h>
#include <time.h>
static const double pi = 3.14159265358979323846;
double cfd_duct3d_reference_response(double h, double w, double y, double z, int terms) {
    double sum = 0;
    for (int t = 0; t < terms; t++) {
        double n = 2 * t + 1, k = n * pi / h;
        double ratio = (exp(-k * z) + exp(-k * (w - z))) / (1 + exp(-k * w));
        sum += sin(k * y) * (1 - ratio) / (n * n * n);
    }
    return 4 * h * h / (pi * pi * pi) * sum;
}
double cfd_duct3d_reference_mean(double h, double w, int terms) {
    if (h > w) {
        double t = h;
        h = w;
        w = t;
    }
    double sum = 0;
    for (int t = 0; t < terms; t++) {
        double n = 2 * t + 1;
        sum += tanh(n * pi * w / (2 * h)) / pow(n, 5);
    }
    return h * h / 12 * (1 - 192 * h / (pow(pi, 5) * w) * sum);
}
void cfd_duct3d_reference_walls(double h, double w, double out[4], int terms) {
    double sum = 0;
    for (int t = 0; t < terms; t++) {
        double n = 2 * t + 1;
        sum += tanh(n * pi * w / (2 * h)) / (n * n * n);
    }
    double z = 8 * h * h / pow(pi, 3) * sum;
    out[0] = out[1] = h * w / 2 - z;
    out[2] = out[3] = z;
}
void cfd_duct3d_destroy(CfdDuct3d *s) {
    if (!s)
        return;
    cfd_cartesian3d_linear_destroy(s->linear);
    cfd_memory_free(s->velocity);
    cfd_memory_free(s->pressure);
    cfd_memory_free(s->rhs);
    s->linear = NULL;
    s->velocity = s->pressure = s->rhs = NULL;
    s->solved = false;
}
bool cfd_duct3d_init(CfdDuct3d *s, const int n[3], const double length[3], double rho, double mu,
                     double flow) {
    if (!s)
        return false;
    memset(s, 0, sizeof(*s));
    clock_t start = clock();
    if (!cfd_cartesian3d_init(&s->grid, n, length) || !isfinite(rho) || rho <= 0 || !isfinite(mu) ||
        mu <= 0 || !isfinite(flow) || flow <= 0)
        return false;
    double dh = 2 * length[1] * length[2] / (length[1] + length[2]);
    if (rho * flow / (length[1] * length[2]) * dh / mu > 100)
        return false;
    s->density = rho;
    s->viscosity = mu;
    s->requested_flow = flow;
    int count = s->grid.count;
    s->velocity = cfd_memory_calloc((size_t)3 * count, sizeof(double));
    s->pressure = cfd_memory_calloc(count, sizeof(double));
    s->rhs = cfd_memory_malloc((size_t)count * sizeof(double));
    bool periodic[3] = {true, false, false};
    s->linear = cfd_cartesian3d_linear_create(&s->grid, periodic, 0, mu, false);
    if (!s->velocity || !s->pressure || !s->rhs || !s->linear) {
        cfd_duct3d_destroy(s);
        return false;
    }
    for (int q = 0; q < count; q++)
        s->rhs[q] = 1;
    s->setup_cpu_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    return true;
}
bool cfd_duct3d_solve(CfdDuct3d *s) {
    if (!s || !s->linear)
        return false;
    if (s->solved)
        return true;
    clock_t start = clock();
    CfdCartesian3d *g = &s->grid;
    int n = g->count;
    if (!cfd_cartesian3d_linear_solve(s->linear, s->rhs, s->velocity, &s->iterations,
                                      &s->relative_residual))
        return false;
    double response_flow = 0;
    for (int q = 0; q < n; q++)
        response_flow += s->velocity[q] * g->area[0] / g->n[0];
    if (!isfinite(response_flow) || response_flow <= 0)
        return false;
    s->gradient_pa_m = s->requested_flow / response_flow;
    for (int q = 0; q < n; q++) {
        s->velocity[q] *= s->gradient_pa_m;
        double xyz[3];
        cfd_cartesian3d_position(g, q, -1, xyz);
        s->pressure[q] = s->gradient_pa_m * (g->length[0] - xyz[0]);
    }
    cfd_cartesian3d_divergence(g, s->velocity, s->rhs);
    for (int q = 0; q < n; q++)
        s->max_divergence = fmax(s->max_divergence, fabs(s->rhs[q]));
    cfd_cartesian3d_linear_apply(s->linear, s->velocity, s->rhs);
    double exact_mean = cfd_duct3d_reference_mean(g->length[1], g->length[2], 512);
    double exact_g = s->requested_flow * s->viscosity / (g->length[1] * g->length[2] * exact_mean);
    double error = 0, norm = 0, wall[4];
    cfd_duct3d_reference_walls(g->length[1], g->length[2], wall, 4096);
    double exact = 0;
    for (int q = 0; q < n; q++) {
        double xyz[3];
        cfd_cartesian3d_position(g, q, 0, xyz);
        double u = s->velocity[q];
        if (q % g->n[0] == 0)
            exact = exact_g / s->viscosity *
                    cfd_duct3d_reference_response(g->length[1], g->length[2], xyz[1], xyz[2], 512);
        error += (u - exact) * (u - exact);
        norm += exact * exact;
        s->flow_m3_s += u * g->area[0] / g->n[0];
        s->discrete_dissipation_w += u * s->rhs[q] * g->volume;
        for (int a = 1; a < 3; a++) {
            int stride = a == 1 ? g->n[0] : g->n[0] * g->n[1], c = (q / stride) % g->n[a];
            double left = c == 0 ? -u : s->velocity[cfd_cartesian3d_neighbor(g, q, a, -1)];
            double right =
                c == g->n[a] - 1 ? -u : s->velocity[cfd_cartesian3d_neighbor(g, q, a, 1)];
            double derivative = (right - left) / (2 * g->h[a]);
            s->dissipation_w += s->viscosity * derivative * derivative * g->volume;
            if (c == 0)
                s->wall_force_n[2 * (a - 1)] += s->viscosity * 2 * u / g->h[a] * g->area[a];
            if (c == g->n[a] - 1)
                s->wall_force_n[2 * (a - 1) + 1] += s->viscosity * 2 * u / g->h[a] * g->area[a];
        }
    }
    s->velocity_relative_l2 = sqrt(error / norm);
    s->pressure_relative_error = fabs(s->gradient_pa_m - exact_g) / exact_g;
    for (int a = 0; a < 4; a++)
        s->wall_relative_error[a] = fabs(s->wall_force_n[a] - exact_g * g->length[0] * wall[a]) /
                                    (exact_g * g->length[0] * wall[a]);
    double exact_power = exact_g * g->length[0] * s->requested_flow;
    s->energy_relative_error = fabs(s->dissipation_w - exact_power) / exact_power;
    s->solve_cpu_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    s->solved = true;
    return true;
}
