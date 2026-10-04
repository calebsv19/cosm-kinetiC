#include "app/cfd_refined_session.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static struct json_object *get(struct json_object *o, const char *k) {
    struct json_object *v = NULL;
    json_object_object_get_ex(o, k, &v);
    return v;
}
static struct json_object *request(void) {
    return json_tokener_parse(
        "{\"grid\":[16,8,1],\"dt\":0.005,\"steps\":3,\"numerical_memory_limit_mib\":32,\"solver_"
        "cell_budget\":10000,\"fluid\":{\"density_kg_m3\":1,\"dynamic_viscosity_pa_s\":0.1},"
        "\"channel\":{\"dimensions_m\":[4,2,0.5],\"inlet_mean_m_s\":0.002,\"solve_mode\":"
        "\"transient_navier_stokes\"}}");
}
int main(void) {
    CfdRefinedSession s;
    struct json_object *r = request();
    assert(r && cfd_refined_session_init(&s, r));
    struct json_object *query = json_tokener_parse(
        "{\"plane\":\"XY\",\"position\":0.5,\"resolution\":8,\"points\":[[2,1,0.25]]}");
    struct json_object *sample = cfd_refined_session_sample(&s, query);
    assert(json_object_array_length(get(sample, "samples")) == 64);
    assert(json_object_array_length(get(sample,"probes"))==1);
    assert(json_object_get_int(
               get(get(get(sample, "statistics"), "pressure_pa"), "finite_samples")) == 0);
    json_object_put(sample);
    assert(cfd_refined_session_step(&s) && cfd_refined_session_step(&s));
    assert(s.observed && s.channel.time == .01 && s.linear_residual < 1e-11);
    sample = cfd_refined_session_sample(&s, query);
    assert(json_object_get_int(
               get(get(get(sample, "statistics"), "pressure_pa"), "finite_samples")) == 64);
    json_object_put(sample);
    struct json_object *snapshot = json_object_new_object();
    cfd_refined_session_snapshot(&s, snapshot, true);
    assert(json_object_array_length(get(get(snapshot, "refined_fields"), "leaves")) ==
           (size_t)s.mesh.cell_count);
    assert(json_object_get_boolean(get(get(snapshot, "energy_budget"), "available")));
    assert(!json_object_get_boolean(get(get(snapshot, "boundary_force_budget"), "available")));
    assert(json_object_get_double(get(get(snapshot, "health"), "observed_transport_dt_limit_s")) > s.dt);
    assert(json_object_get_double(get(get(snapshot, "health"), "transport_cfl_limit")) == .25);
    assert(!json_object_get_boolean(
        get(get(snapshot, "qualification"), "physical_accuracy_certified")));
    json_object_put(snapshot);
    cfd_refined_session_destroy(&s);
    assert(!s.memory.live_bytes);
    json_object_put(r);
    r = request();
    json_object_object_add(r, "steps", json_object_new_int(1));
    json_object_object_add(get(r, "channel"), "solve_mode",
                           json_object_new_string("steady_stokes"));
    json_object_object_add(get(r, "channel"), "obstacle_bounds_m",
                           json_tokener_parse("[1.5,0.75,2.5,1.25]"));
    assert(cfd_refined_session_init(&s, r) && cfd_refined_session_step(&s));
    assert(s.channel.time == 0 && s.channel.tick == 1 && s.observed);
    snapshot = json_object_new_object();
    cfd_refined_session_snapshot(&s, snapshot, true);
    struct json_object *gate = get(get(snapshot, "qualification"), "fixed_case_reference_gate");
    assert(json_object_get_boolean(get(gate, "applicable")) &&
           !json_object_get_boolean(get(gate, "passed")));
    assert(!get(get(snapshot, "health"), "observed_transport_dt_limit_s"));
    assert(s.body_force.pressure[0] > 0 && s.body_force.viscous[0] > 0);
    assert(!cfd_refined_session_step(&s));
    json_object_put(snapshot);
    cfd_refined_session_destroy(&s);
    assert(!s.memory.live_bytes);
    json_object_object_add(r, "numerical_memory_limit_mib", json_object_new_int(1));
    bool initialized = cfd_refined_session_init(&s, r);
    if (initialized)
        assert(!cfd_refined_session_step(&s));
    assert(s.error && s.memory.rejected_allocations);
    cfd_refined_session_destroy(&s);
    assert(!s.memory.live_bytes);
    sample = cfd_refined_session_sample(&s, query);
    assert(get(sample, "error"));
    json_object_put(sample);
    json_object_put(r);
    r = request();
    json_object_object_add(r, "dt", json_object_new_boolean(true));
    assert(!cfd_refined_session_init(&s, r));
    cfd_refined_session_destroy(&s);
    json_object_put(r);
    json_object_put(query);
    puts("refined session modes, bounded sampling/export, unavailable observations, physical-gate "
         "scope and memory rejection passed");
    return 0;
}
