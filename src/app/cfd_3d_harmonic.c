#include "app/cfd_3d_session.h"
#include <math.h>
static const double pi = 3.14159265358979323846;
void cfd_3d_harmonic_observe(Cfd3dSession *s) {
    Cfd3dHarmonic *h = &s->harmonic;
    if (h->complete)
        return;
    double sn = sin(2 * pi * s->time), cs = cos(2 * pi * s->time), p = 0, norm = 0;
    for (int q = 0; q < s->grid.count; q++) {
        p += s->wall.pressure[q] * s->wall.pressure_reference[q];
        norm += s->wall.pressure_reference[q] * s->wall.pressure_reference[q];
    }
    p /= norm;
    h->sum_sin += sn;
    h->sum_cos += cs;
    h->sum_sin2 += sn * sn;
    h->sum_cos2 += cs * cs;
    h->sum_sincos += sn * cs;
    double basis[3] = {1, sn, cs};
    for (int a = 0; a < 3; a++) {
        h->velocity[a] += s->wall.amplitude * basis[a];
        h->pressure[a] += p * basis[a];
    }
    h->count++;
    h->last_time = s->time;
    h->complete = s->time >= 1 - 1e-10;
}
static void number(struct json_object *o, const char *k, double v) {
    json_object_object_add(o, k, isfinite(v) ? json_object_new_double(v) : NULL);
}
struct json_object *cfd_3d_harmonic_snapshot(const Cfd3dSession *s) {
    const Cfd3dHarmonic *h = &s->harmonic;
    struct json_object *out = json_object_new_object();
    double count = h->count, ss = h->sum_sin2, cc = h->sum_cos2, sc = h->sum_sincos;
    if (count > 0) {
        ss -= h->sum_sin * h->sum_sin / count;
        cc -= h->sum_cos * h->sum_cos / count;
        sc -= h->sum_sin * h->sum_cos / count;
    }
    double determinant = ss * cc - sc * sc;
    bool available = s->transient_kind && s->transient_kind != CFD_3D_PRESSURE_STARTUP &&
                     h->complete && h->count >= 10 && determinant > 1e-10;
    json_object_object_add(out, "available", json_object_new_boolean(available));
    json_object_object_add(
        out, "method",
        json_object_new_string("offset/sine/cosine least squares of every accepted step through "
                               "first 1 s period; no advance"));
    number(out, "accepted_samples", count);
    number(out, "last_sample_time_s", h->last_time);
    if (!available)
        return out;
    for (int mode = 0; mode < 2; mode++) {
        const double *rhs = mode ? h->pressure : h->velocity;
        double rs = rhs[1] - rhs[0] * h->sum_sin / count;
        double rc = rhs[2] - rhs[0] * h->sum_cos / count;
        double sn = (cc * rs - sc * rc) / determinant;
        double cs = (ss * rc - sc * rs) / determinant;
        double amp = hypot(sn, cs), phase = mode ? atan2(-sn, cs) - .3 : atan2(cs, sn);
        struct json_object *value = json_object_new_object();
        number(value, "offset", (rhs[0] - sn * h->sum_sin - cs * h->sum_cos) / count);
        number(value, "harmonic_amplitude", amp);
        number(value, "amplitude_relative_error", fabs(amp / (mode ? .01 : .2) - 1));
        number(value, "phase_error_degrees", fabs(remainder(phase * 180 / pi, 360)));
        json_object_object_add(out, mode ? "pressure" : "velocity", value);
    }
    return out;
}
