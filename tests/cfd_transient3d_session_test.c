#include "app/cfd_3d_session.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct json_object *get(struct json_object *o, const char *k) {
    struct json_object *v = NULL;
    json_object_object_get_ex(o, k, &v);
    return v;
}
static double value(struct json_object *o, const char *k) {
    return json_object_get_double(get(o, k));
}
static struct json_object *snapshot(Cfd3dSession *s) {
    struct json_object *out = json_object_new_object();
    cfd_3d_session_snapshot(s, out, true);
    return out;
}
typedef struct {
    int calls;
    Cfd3dSession *session;
    struct json_object *accepted;
} CancelContext;
static bool cancel(void *context) {
    CancelContext *c = context;
    c->calls++;
    struct json_object *during = snapshot(c->session);
    assert(json_object_equal(get(during, "energy_budget"), get(c->accepted, "energy_budget")));
    assert(json_object_equal(get(get(during, "health"), "transport_cfl"),
                             get(get(c->accepted, "health"), "transport_cfl")));
    json_object_put(during);
    return false;
}
static void check(const char *mode) {
    bool startup = !strcmp(mode, "pressure_startup");
    bool open = startup || !strcmp(mode, "open_wall_stokes_transient");
    struct json_object *r = json_tokener_parse(
        "{\"grid\":[8,8,8],\"steps\":100,\"dt\":0.025,\"numerical_memory_limit_mib\":32,"
        "\"fluid\":{\"density_kg_m3\":1,\"dynamic_viscosity_pa_s\":0.1},"
        "\"channel\":{\"dimensions_m\":[4,2,2],\"volume_flow_m3_s\":0.008}}");
    json_object_object_add(get(r, "channel"), "solve_mode", json_object_new_string(mode));
    Cfd3dSession s;
    assert(cfd_3d_session_init(&s, r));
    struct json_object *o = snapshot(&s), *fields = get(o, "cartesian_fields");
    assert(get(get(o, "health"), "linear_relative_residual") == NULL);
    assert(json_object_array_get_idx(get(fields, "pressure_pa"), 0) == NULL);
    json_object_put(o);
    assert(cfd_3d_session_step(&s));
    assert(cfd_3d_session_step(&s));
    size_t allocations = s.memory.successful_allocations;
    assert(cfd_3d_session_step(&s));
    assert(allocations == s.memory.successful_allocations);
    double time = s.time;
    o = snapshot(&s);
    fields = get(o, "cartesian_fields");
    assert(!strcmp(json_object_get_string(get(o, "solve_mode")), mode));
    assert(json_object_get_boolean(json_object_array_get_idx(get(fields, "periodic_axes"), 0)) ==
           !open);
    CfdMixed3d *mixed = startup ? s.startup.mixed : s.wall.mixed;
    const double *u = startup ? s.startup.velocity : s.wall.velocity;
    const double *p = startup ? s.startup.pressure : s.wall.pressure;
    struct json_object *vel = get(fields, "velocity_faces_m_s"), *pres = get(fields, "pressure_pa"),
                       *outlet = get(fields, "outlet_x_velocity_m_s");
    int nx = s.grid.n[0], ny = s.grid.n[1], nz = s.grid.n[2];
    assert(json_object_array_length(vel) == (size_t)s.grid.count);
    assert(open ? json_object_array_length(outlet) == (size_t)ny * nz : outlet == NULL);
    double maxdiv = 0;
    for (int k = 0; k < nz; k++)
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < nx; i++) {
                int q = (k * ny + j) * nx + i;
                assert(json_object_get_double(json_object_array_get_idx(pres, q)) == p[q]);
                double div = 0;
                for (int a = 0; a < 3; a++) {
                    double west = json_object_get_double(
                        json_object_array_get_idx(json_object_array_get_idx(vel, q), a));
                    int id = cfd_mixed3d_index(mixed, a, i, j, k);
                    assert(west == (id >= 0 ? u[id] : 0));
                    int hi[3] = {i, j, k};
                    hi[a]++;
                    double east;
                    if (hi[a] == s.grid.n[a]) {
                        if (a == 0 && open)
                            east = json_object_get_double(
                                json_object_array_get_idx(outlet, k * ny + j));
                        else if (a == 0)
                            east = json_object_get_double(json_object_array_get_idx(
                                json_object_array_get_idx(vel, (k * ny + j) * nx), a));
                        else
                            east = 0;
                    } else
                        east = json_object_get_double(json_object_array_get_idx(
                            json_object_array_get_idx(vel, (hi[2] * ny + hi[1]) * nx + hi[0]), a));
                    div += (east - west) / s.grid.h[a];
                }
                maxdiv = fmax(maxdiv, fabs(div));
            }
    assert(maxdiv < 1e-8);
    assert(fabs(value(get(o, "energy_budget"), "physical_strain_dissipation_w") -
                (startup ? s.startup.dissipation : s.wall.dissipation_w)) < 1e-16);
    assert(!json_object_get_boolean(get(get(o, "boundary_force_budget"), "body_force_available")));
    if (startup)
        for (int a = 0; a < 4; a++)
            assert(json_object_get_double(json_object_array_get_idx(
                       get(get(o, "boundary_force_budget"), "wall_drag_n"), a)) ==
                   s.startup.wall_force[a]);
    for (int axis = 0; axis < 3; axis++) {
        struct json_object *req =
            json_tokener_parse("{\"resolution\":8,\"points\":[[4,1,1],[0,1,1],[1,0,1],[99,1,1]]}");
        json_object_object_add(req, "plane",
                               json_object_new_string(axis == 0   ? "XY"
                                                      : axis == 1 ? "XZ"
                                                                  : "YZ"));
        struct json_object *sample = cfd_3d_session_sample(&s, req);
        assert(json_object_array_length(get(sample, "samples")) == 64);
        struct json_object *wall = json_object_array_get_idx(get(sample, "probes"), 2);
        assert(json_object_get_double(json_object_array_get_idx(get(wall, "values"), 0)) == 0);
        assert(get(get(sample, "statistics"), "pressure_proxy") == NULL);
        assert(!json_object_get_boolean(
            get(json_object_array_get_idx(get(sample, "probes"), 3), "inside")));
        assert(s.time == time && allocations == s.memory.successful_allocations);
        json_object_put(sample);
        json_object_put(req);
    }
    // Published fields survive a rejected candidate bit-for-bit, not merely at a probe.
    CancelContext context = {.session = &s, .accepted = o};
    cfd_3d_session_checkpoint(&s, cancel, &context);
    assert(!cfd_3d_session_step(&s));
    assert(context.calls > 0 && s.cancelled && !s.failed && s.time == time);
    struct json_object *after = snapshot(&s);
    assert(json_object_equal(fields, get(after, "cartesian_fields")));
    assert(json_object_equal(get(o, "energy_budget"), get(after, "energy_budget")));
    assert(!strcmp(json_object_get_string(get(get(after, "health"), "last_step_outcome")),
                   "cancelled_preserved_last_accepted"));
    assert(!cfd_3d_session_step(&s));
    json_object_put(after);
    json_object_put(o);
    cfd_3d_session_destroy(&s);
    assert(s.memory.live_bytes == 0);
    json_object_put(r);
}
// On an exact one-period uniform time grid the Fourier moments are orthogonal.
// This independent readback fit uses actual native field projections, not the
// accumulator's centered normal-equation implementation.
static void check_harmonic(void) {
    struct json_object *r = json_tokener_parse(
        "{\"grid\":[8,8,8],\"steps\":40,\"dt\":0.025,\"numerical_memory_limit_mib\":32,"
        "\"fluid\":{\"density_kg_m3\":1,\"dynamic_viscosity_pa_s\":0.1},"
        "\"channel\":{\"dimensions_m\":[2,2.5,3],\"solve_mode\":\"wall_transport_transient\"}}");
    Cfd3dSession s;
    assert(cfd_3d_session_init(&s, r));
    double vs = 0, vc = 0, ps = 0, pc = 0;
    const double pi = acos(-1);
    for (int step = 0; step < 40; step++) {
        assert(cfd_3d_session_step(&s));
        double v = 0, vn = 0, p = 0, pn = 0;
        for (int q = 0; q < s.wall.count; q++) {
            v += s.wall.velocity[q] * s.wall.face_reference[q];
            vn += s.wall.face_reference[q] * s.wall.face_reference[q];
        }
        for (int q = 0; q < s.grid.count; q++) {
            p += s.wall.pressure[q] * s.wall.pressure_reference[q];
            pn += s.wall.pressure_reference[q] * s.wall.pressure_reference[q];
        }
        double sn = sin(2 * pi * s.time), cs = cos(2 * pi * s.time);
        vs += v / vn * sn / 20;
        vc += v / vn * cs / 20;
        ps += p / pn * sn / 20;
        pc += p / pn * cs / 20;
        struct json_object *h = cfd_3d_harmonic_snapshot(&s);
        assert(json_object_get_boolean(get(h, "available")) == (step == 39));
        json_object_put(h);
    }
    struct json_object *h = cfd_3d_harmonic_snapshot(&s), *v = get(h, "velocity"),
                       *p = get(h, "pressure");
    assert(fabs(value(v, "harmonic_amplitude") - hypot(vs, vc)) < 1e-12);
    assert(fabs(value(p, "harmonic_amplitude") - hypot(ps, pc)) < 1e-12);
    assert(fabs(value(v, "phase_error_degrees") - fabs(remainder(atan2(vc, vs) * 180 / pi, 360))) <
           1e-10);
    assert(fabs(value(p, "phase_error_degrees") -
                fabs(remainder((atan2(-ps, pc) - .3) * 180 / pi, 360))) < 1e-10);
    int count = s.harmonic.count;
    assert(cfd_3d_session_step(&s));
    assert(s.harmonic.count == count);
    json_object_put(h);
    json_object_put(r);
    cfd_3d_session_destroy(&s);
    assert(s.memory.live_bytes == 0);
}
int main(void) {
    check_harmonic();
    check("wall_stokes_transient");
    check("wall_transport_transient");
    check("pressure_startup");
    check("open_wall_stokes_transient");
    puts("Four transient session modes: physical field/divergence readback, energy/wall units, XYZ "
         "no-advance inspection, and complete accepted-state cancellation preservation passed");
    return 0;
}
