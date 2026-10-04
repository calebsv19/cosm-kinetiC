#include "app/cfd_open2d_force_check.h"
#include <assert.h>
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
static struct json_object *member(struct json_object *o, const char *key) {
    struct json_object *v = NULL;
    assert(json_object_object_get_ex(o, key, &v));
    return v;
}
static void load(struct json_object *o, const char *key, double *v, int n) {
    struct json_object *a = member(o, key);
    assert(json_object_array_length(a) == (size_t)n);
    for (int k = 0; k < n; k++)
        v[k] = json_object_get_double(json_object_array_get_idx(a, k));
}
static void num(struct json_object *o, const char *key, double v) {
    json_object_object_add(o, key, json_object_new_double(v));
}
static double uc(CfdOpen2D *c, int i, int j) {
    return .5 * (c->u[j * (c->nx + 1) + i] + c->u[j * (c->nx + 1) + i + 1]);
}
static struct json_object *array(double v[8]) {
    struct json_object *a = json_object_new_array();
    for (int k = 0; k < 8; k++)
        json_object_array_add(a, json_object_new_double(v[k]));
    return a;
}
int main(int argc, char **argv) {
    assert(argc == 3);
    struct json_object *in = json_object_from_file(argv[1]);
    assert(in);
    int n = json_object_get_int(member(in, "n"));
    CfdOpen2D c;
    assert(cfd_open2d_init(&c, n, n, 4, 2, .5, 1, .1, .002));
    int lo = 3 * n / 8, hi = 5 * n / 8;
    assert(cfd_open2d_set_obstacle(&c, lo, lo, hi, hi));
    load(in, "u", c.u, n * (n + 1));
    load(in, "v", c.v, n * (n + 1));
    load(in, "p", c.p, n * n);
    CfdMac2DForceCheck f;
    assert(cfd_open2d_force_check(&c, n / 8, n / 8, 7 * n / 8, 7 * n / 8, &f));
    double pb[8] = {0}, vb[8] = {0}, dx = 4. / n, dy = 2. / n;
    for (int side = -1; side <= 1; side += 2)
        for (int k = lo; k < hi; k++) {
            int i = side < 0 ? lo - 1 : hi, away = side < 0 ? -1 : 1,
                bin = 8 * (k - lo) / (hi - lo);
            pb[bin] += -side *
                       (1.875 * c.p[k * n + i] - 1.25 * c.p[k * n + i + away] +
                        .375 * c.p[k * n + i + 2 * away]) *
                       dy * .5;
            int j = side < 0 ? lo - 1 : hi;
            vb[bin] +=
                .1 * (225 * uc(&c, k, j) - 50 * uc(&c, k, j + away) + 9 * uc(&c, k, j + 2 * away)) /
                (60 * dy) * dx * .5;
        }
    struct json_object *out = json_object_new_object(), *reference = member(in, "reference");
    num(out, "n", n);
    num(out, "pressure", f.surface_pressure_x_n);
    num(out, "viscous", f.surface_viscous_x_n);
    num(out, "pressure_reference_error",
        fabs(f.surface_pressure_x_n / json_object_get_double(member(reference, "pressure_drag_n")) -
             1));
    num(out, "viscous_reference_error",
        fabs(f.surface_viscous_x_n / json_object_get_double(member(reference, "viscous_drag_n")) -
             1));
    double ps = 0, vs = 0;
    for (int k = 0; k < 8; k++) {
        ps += pb[k];
        vs += vb[k];
    }
    assert(fabs(ps - f.surface_pressure_x_n) < 1e-12 && fabs(vs - f.surface_viscous_x_n) < 1e-12);
    json_object_object_add(out, "pressure_bands", array(pb));
    json_object_object_add(out, "viscous_bands", array(vb));
    assert(json_object_to_file_ext(argv[2], out, JSON_C_TO_STRING_PRETTY) == 0);
    puts(json_object_to_json_string(out));
    json_object_put(out);
    json_object_put(in);
    cfd_open2d_destroy(&c);
}
