#include "app/session_observation.h"
#include "sim_runtime_backend_3d_scaffold_internal.h"
#include <math.h>

struct json_object *physics_sim_session_observation(const SceneState *scene) {
    SimRuntimeBackendReport report = {0};
    if (!scene_backend_report(scene, &report) || !report.full_3d_solver_live)
        return NULL;
    const SimRuntimeBackend3DScaffold *state = scene->backend->impl;
    int w = report.domain_w < 64 ? report.domain_w : 64;
    int h = report.domain_h < 64 ? report.domain_h : 64;
    int z = report.domain_d / 2;
    struct json_object *out = json_object_new_object();
    struct json_object *samples = json_object_new_array();
    float peak = 0.0f;
    int nonfinite = 0;
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            int x = i * report.domain_w / w;
            int y = j * report.domain_h / h;
            float d = 0, vx = 0, vy = 0, vz = 0, p = 0;
            sim_runtime_3d_brick_store_get_cell(&state->brick_store, x, y, z, &d, &vx, &vy, &vz,
                                                &p);
            bool solid = backend_3d_scaffold_obstacle_cell_solid(state, x, y, z);
            float speed = sqrtf(vx * vx + vy * vy + vz * vz);
            if (!isfinite(d) || !isfinite(speed) || !isfinite(p)) {
                nonfinite++;
                speed = d = 0;
            }
            if (speed > peak)
                peak = speed;
            struct json_object *cell = json_object_new_array();
            json_object_array_add(cell, json_object_new_double(speed));
            json_object_array_add(cell, json_object_new_double(d));
            json_object_array_add(cell, json_object_new_int(solid));
            json_object_array_add(samples, cell);
        }
    }
    json_object_object_add(out, "width", json_object_new_int(w));
    json_object_object_add(out, "height", json_object_new_int(h));
    json_object_object_add(out, "slice_z", json_object_new_int(z));
    json_object_object_add(out, "plane", json_object_new_string("XY"));
    json_object_object_add(
        out, "sampling",
        json_object_new_string("nearest sparse cell; row-major [speed,dye_density,solid]"));
    json_object_object_add(out, "speed_units", json_object_new_string("m/s"));
    json_object_object_add(out, "density_units", json_object_new_string("simulation dye scalar"));
    json_object_object_add(out, "speed_max", json_object_new_double(peak));
    json_object_object_add(out, "sampled_nonfinite_cells", json_object_new_int(nonfinite));
    json_object_object_add(out, "samples", samples);
    struct json_object *bounds = json_object_new_array();
    double b[] = {report.world_min_x, report.world_min_y, report.world_min_z,
                  report.world_max_x, report.world_max_y, report.world_max_z};
    for (int i = 0; i < 6; i++)
        json_object_array_add(bounds, json_object_new_double(b[i]));
    json_object_object_add(out, "world_bounds_m", bounds);
    return out;
}
