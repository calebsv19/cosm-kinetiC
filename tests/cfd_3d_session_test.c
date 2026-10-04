#include "app/cfd_3d_session.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static struct json_object *get(struct json_object *o, const char *k) {
    struct json_object *v = NULL;
    json_object_object_get_ex(o, k, &v);
    return v;
}
static struct json_object *request(bool steady) {
    return json_tokener_parse(
        steady ? "{\"grid\":[16,8,8],\"steps\":1,\"dt\":0.01,\"numerical_memory_limit_mib\":16,"
                 "\"solver_cell_budget\":262144,\"fluid\":{\"density_kg_m3\":1,\"dynamic_viscosity_"
                 "pa_s\":0.1},\"channel\":{\"solve_mode\":\"steady_duct\",\"dimensions_m\":[4,2,2],"
                 "\"volume_flow_m3_s\":0.008}}"
               : "{\"grid\":[8,8,8],\"steps\":5,\"dt\":0.01,\"numerical_memory_limit_mib\":16,"
                 "\"solver_cell_budget\":262144,\"fluid\":{\"density_kg_m3\":1,\"dynamic_viscosity_"
                 "pa_s\":0.1},\"channel\":{\"solve_mode\":\"manufactured_transient\",\"dimensions_"
                 "m\":[2,2.5,3]}}");
}
static void check(bool steady) {
    struct json_object *r = request(steady);
    Cfd3dSession s;
    assert(cfd_3d_session_init(&s, r));
    assert(s.ready);
    struct json_object *sample =
        json_tokener_parse("{\"plane\":\"YZ\",\"position\":0.5,\"resolution\":8,\"points\":[[1,1,1]"
                           ",[1,0,1],[99,1,1]]}");
    struct json_object *o = cfd_3d_session_sample(&s, sample);
    assert(json_object_array_length(get(o, "samples")) == 64);
    assert(get(get(o, "statistics"), "pressure_proxy") == NULL);
    json_object_put(o);
    assert(cfd_3d_session_step(&s));
    assert(s.observed);
    double time = s.time;
    size_t allocations = s.memory.successful_allocations;
    for (int i = 0; i < 3; i++) {
        json_object_object_add(sample, "plane",
                               json_object_new_string(i == 0   ? "XY"
                                                      : i == 1 ? "XZ"
                                                               : "YZ"));
        o = cfd_3d_session_sample(&s, sample);
        assert(json_object_array_length(get(o, "samples")) == 64);
        struct json_object *probe = json_object_array_get_idx(get(o, "probes"), 0);
        assert(json_object_array_get_idx(get(probe, "values"), 9));
        assert(s.time == time);
        json_object_put(o);
    }
    assert(allocations == s.memory.successful_allocations);
    if (!steady) {
        assert(cfd_3d_session_step(&s));
        allocations = s.memory.successful_allocations;
        assert(cfd_3d_session_step(&s));
        assert(allocations == s.memory.successful_allocations);
        double mean = 0;
        for (int q = 0; q < s.grid.count; q++)
            mean += s.transient.pressure[q] / s.grid.count;
        assert(fabs(mean) < 1e-12);
    }
    o = json_object_new_object();
    cfd_3d_session_snapshot(&s, o, true);
    assert(get(o, "cartesian_fields"));
    json_object_put(o);
    cfd_3d_session_destroy(&s);
    assert(s.memory.live_bytes == 0);
    json_object_put(sample);
    json_object_put(r);
}
static void check_open(void) {
    struct json_object *r = request(true), *ch = get(r, "channel");
    json_object_object_add(ch, "solve_mode", json_object_new_string("steady_open_duct"));
    Cfd3dSession s;
    assert(cfd_3d_session_init(&s, r));
    size_t alloc = s.memory.successful_allocations;
    size_t exact_cap = s.memory.peak_bytes;
    assert(cfd_3d_session_step(&s));
    assert(s.open && s.steady && s.time == 0);
    assert(alloc == s.memory.successful_allocations);
    for (int axis = 0; axis < 3; axis++) {
        struct json_object *req =
            json_tokener_parse("{\"resolution\":8,\"points\":[[4,1,1],[0,1,1]]}");
        json_object_object_add(req, "plane",
                               json_object_new_string(axis == 0   ? "XY"
                                                      : axis == 1 ? "XZ"
                                                                  : "YZ"));
        struct json_object *o = cfd_3d_session_sample(&s, req);
        assert(json_object_array_length(get(o, "samples")) == 64);
        struct json_object *probe = json_object_array_get_idx(get(o, "probes"), 0);
        assert(json_object_array_get_idx(get(probe, "values"), 9));
        assert(s.time == 0 && alloc == s.memory.successful_allocations);
        json_object_put(o);
        json_object_put(req);
    }
    struct json_object *o = json_object_new_object();
    cfd_3d_session_snapshot(&s, o, true);
    assert(fabs(json_object_get_double(get(get(o, "physics"), "pressure_drop_pa")) -
                s.open_duct.pressure_drop_pa) < 1e-15);
    assert(json_object_array_length(get(get(o, "cartesian_fields"), "outlet_x_velocity_m_s")) ==
           64);
    assert(!json_object_get_boolean(
        get(get(get(o, "qualification"), "duct_reference_gate"), "passed")));
    assert(cfd_3d_session_step(&s));
    assert(alloc == s.memory.successful_allocations);
    json_object_put(o);
    cfd_3d_session_destroy(&s);
    assert(s.memory.live_bytes == 0);
    json_object_put(r);
    for (int delta = 0; delta <= 1; delta++) {
        CfdMemoryBudget b = {.limit_bytes = exact_cap - delta};
        CfdMemoryBudget *prev = cfd_memory_scope(&b);
        int n[3] = {16, 8, 8};
        double l[3] = {4, 2, 2};
        CfdOpen3d d;
        assert(cfd_open3d_init(&d, n, l, 1, .1, .008, 0) == (delta == 0));
        if (!delta)
            assert(cfd_open3d_solve(&d));
        cfd_open3d_destroy(&d);
        assert(b.live_bytes == 0);
        cfd_memory_scope(prev);
    }
    for (int cap = 1; cap <= 256; cap *= 2) {
        CfdMemoryBudget b = {.limit_bytes = (size_t)cap * 1024};
        CfdMemoryBudget *prev = cfd_memory_scope(&b);
        int n[3] = {16, 8, 8};
        double l[3] = {4, 2, 2};
        CfdOpen3d d;
        assert(!cfd_open3d_init(&d, n, l, 1, .1, .008, 0));
        cfd_open3d_destroy(&d);
        assert(b.live_bytes == 0 && b.last_failure == CFD_MEMORY_LIMIT);
        cfd_memory_scope(prev);
    }
}
int main(void) {
    check_open();
    check(true);
    check(false);
    for (int cap = 1; cap <= 64; cap *= 2) {
        CfdMemoryBudget b = {.limit_bytes = (size_t)cap * 1024};
        CfdMemoryBudget *prev = cfd_memory_scope(&b);
        CfdPeriodic3d s;
        int n[3] = {8, 8, 8};
        double l[3] = {2, 2.5, 3};
        assert(!cfd_periodic3d_init(&s, n, l, 1, .1, .01));
        cfd_periodic3d_destroy(&s);
        assert(b.live_bytes == 0);
        assert(b.last_failure == CFD_MEMORY_LIMIT);
        cfd_memory_scope(prev);
    }
    CfdMemoryBudget b = {.limit_bytes = 16 * 1024 * 1024};
    CfdMemoryBudget *prev = cfd_memory_scope(&b);
    CfdPeriodic3d s;
    int n[3] = {8, 8, 8};
    double l[3] = {.1, .1, .1};
    assert(cfd_periodic3d_init(&s, n, l, 1, .1, .1));
    double old = s.pressure[0];
    assert(!cfd_periodic3d_step(&s));
    assert(s.steps == 0 && s.time == 0 && s.pressure[0] == old);
    cfd_periodic3d_destroy(&s);
    assert(b.live_bytes == 0);
    cfd_memory_scope(prev);
    puts("3D session ownership, unavailable fields, pressure gauge, no-advance sampling, "
         "repeated-step allocations and failure cleanup passed");
    return 0;
}
