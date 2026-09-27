#include "app/session_observation.h"
#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "app/wind_tunnel_3d_inspector.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static struct json_object *get(struct json_object *o, const char *k) {
    struct json_object *v = NULL;
    json_object_object_get_ex(o, k, &v);
    return v;
}
int main(void) {
    AppConfig cfg = app_config_default();
    cfg.grid_w = 16;
    cfg.grid_h = 8;
    cfg.grid_d = 8;
    SimModeRoute route = {.simulation_mode = SIM_MODE_WIND_TUNNEL,
                          .requested_space_mode = SPACE_MODE_3D,
                          .projection_space_mode = SPACE_MODE_2D,
                          .backend_lane = SIM_BACKEND_CONTROLLED_3D,
                          .wind_tunnel_3d_active = true};
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min = (CoreObjectVec3){0, 0, 0};
    visual.scene_domain.max = (CoreObjectVec3){4, 2, 2};
    visual.wind_tunnel_authored = true;
    visual.wind_tunnel = wind_tunnel_3d_config_default(&cfg);
    SimRuntimeBackend *backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    assert(backend);
    SceneState scene = {0};
    scene.mode_route = route;
    scene.config = &cfg;
    scene.backend = backend;
    scene.runtime_visual = visual;
    SimRuntimeBackendReport r = {0};
    assert(scene_backend_report(&scene, &r));
    SimRuntimeBackend3DScaffold *state = backend->impl;
    // Manufactured affine velocity v=(2x-y, x+3y,4z): div=9, curl=(0,0,2).
    for (int z = 0; z < r.domain_d; z++)
        for (int y = 0; y < r.domain_h; y++)
            for (int x = 0; x < r.domain_w; x++) {
                float xx = (x + .5f) * r.voxel_size, yy = (y + .5f) * r.voxel_size,
                      zz = (z + .5f) * r.voxel_size;
                assert(sim_runtime_3d_brick_store_set_cell(&state->brick_store, x, y, z, .25f,
                                                           2 * xx - yy, xx + 3 * yy, 4 * zz, xx));
            }
    const char *planes[] = {"XY", "XZ", "YZ"};
    for (int plane = 0; plane < 3; plane++)
        for (int edge = 0; edge < 2; edge++) {
            struct json_object *req = json_object_new_object();
            json_object_object_add(req, "plane", json_object_new_string(planes[plane]));
            json_object_object_add(req, "position", json_object_new_double(edge));
            json_object_object_add(req, "resolution", json_object_new_int(64));
            struct json_object *out = physics_sim_session_sample(&scene, req);
            assert(out);
            struct json_object *cells = get(out, "samples");
            for (size_t i = 0; i < json_object_array_length(cells); i++) {
                struct json_object *cell = json_object_array_get_idx(cells, i);
                assert(fabs(json_object_get_double(json_object_array_get_idx(cell, 7)) - 9) < 1e-4);
                assert(fabs(json_object_get_double(json_object_array_get_idx(cell, 8)) - 2) < 1e-4);
                assert(fabs(json_object_get_double(json_object_array_get_idx(cell, 1)) - .25) <
                       1e-6);
            }
            json_object_put(out);
            json_object_put(req);
        }
    assert(scene_backend_report(&scene, &r));
    assert(r.runtime_export_cache_materialization_count == 0);
    sim_runtime_backend_destroy(backend);
    puts("Sparse sample affine divergence/curl, all planes and edge stencils passed");
    return 0;
}
