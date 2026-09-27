#include "app/session_observation.h"
#include "sim_runtime_backend_3d_scaffold_internal.h"
#include <math.h>
#include <string.h>

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

static struct json_object *ob_get(struct json_object *o, const char *key) {
    struct json_object *v = NULL;
    json_object_object_get_ex(o, key, &v);
    return v;
}
static void ob_number(struct json_object *o, const char *key, double value) {
    json_object_object_add(o, key, isfinite(value) ? json_object_new_double(value) : NULL);
}
static void cell_values(const SimRuntimeBackend3DScaffold *state, const int q[3], float v[5]) {
    sim_runtime_3d_brick_store_get_cell(&state->brick_store, q[0], q[1], q[2], &v[0], &v[1], &v[2],
                                        &v[3], &v[4]);
}
static struct json_object *sample_cell(const SimRuntimeBackend3DScaffold *state, const int dims[3],
                                       double dx, const int q[3]) {
    float v[5] = {0};
    cell_values(state, q, v);
    double grad[3][3] = {{0}};
    for (int axis = 0; axis < 3; axis++) {
        int lo[3] = {q[0], q[1], q[2]}, hi[3] = {q[0], q[1], q[2]};
        if (lo[axis] > 0)
            lo[axis]--;
        if (hi[axis] < dims[axis] - 1)
            hi[axis]++;
        float a[5] = {0}, b[5] = {0};
        cell_values(state, lo, a);
        cell_values(state, hi, b);
        double span = (hi[axis] - lo[axis]) * dx;
        for (int component = 0; component < 3; component++)
            grad[component][axis] = span > 0 ? (b[component + 1] - a[component + 1]) / span : 0;
    }
    double curl[3] = {grad[2][1] - grad[1][2], grad[0][2] - grad[2][0], grad[1][0] - grad[0][1]};
    double values[] = {sqrt((double)v[1] * v[1] + (double)v[2] * v[2] + (double)v[3] * v[3]),
                       v[0],
                       backend_3d_scaffold_obstacle_cell_solid(state, q[0], q[1], q[2]),
                       v[1],
                       v[2],
                       v[3],
                       v[4],
                       grad[0][0] + grad[1][1] + grad[2][2],
                       sqrt(curl[0] * curl[0] + curl[1] * curl[1] + curl[2] * curl[2])};
    struct json_object *out = json_object_new_array();
    for (int i = 0; i < 9; i++)
        json_object_array_add(out, isfinite(values[i]) ? json_object_new_double(values[i]) : NULL);
    return out;
}
struct json_object *physics_sim_session_sample(const SceneState *scene,
                                               struct json_object *request) {
    SimRuntimeBackendReport r = {0};
    if (!scene_backend_report(scene, &r) || !r.full_3d_solver_live)
        return NULL;
    const SimRuntimeBackend3DScaffold *state = scene->backend->impl;
    int dims[3] = {r.domain_w, r.domain_h, r.domain_d};
    const char *plane = json_object_get_string(ob_get(request, "plane"));
    int u = 0, v = 1, n = 2;
    if (plane && !strcmp(plane, "XZ")) {
        v = 2;
        n = 1;
    } else if (plane && !strcmp(plane, "YZ")) {
        u = 1;
        v = 2;
        n = 0;
    }
    double position = json_object_get_double(ob_get(request, "position"));
    int index = (int)floor(position * dims[n]);
    if (index >= dims[n])
        index = dims[n] - 1;
    if (index < 0)
        index = 0;
    int limit = json_object_get_int(ob_get(request, "resolution"));
    if (limit < 4)
        limit = 4;
    if (limit > 64)
        limit = 64;
    int width = dims[u] < limit ? dims[u] : limit, height = dims[v] < limit ? dims[v] : limit;
    struct json_object *out = json_object_new_object(), *samples = json_object_new_array(),
                       *stats = json_object_new_object();
    double minimum[9], maximum[9], sum[9] = {0};
    int count[9] = {0};
    for (int f = 0; f < 9; f++) {
        minimum[f] = INFINITY;
        maximum[f] = -INFINITY;
    }
    int invalid = 0;
    for (int j = 0; j < height; j++)
        for (int i = 0; i < width; i++) {
            int q[3] = {0};
            q[u] = i * dims[u] / width;
            q[v] = j * dims[v] / height;
            q[n] = index;
            struct json_object *cell = sample_cell(state, dims, r.voxel_size, q);
            bool bad = false;
            for (int f = 0; f < 9; f++) {
                struct json_object *value = json_object_array_get_idx(cell, f);
                if (!value) {
                    bad = true;
                    continue;
                }
                double x = json_object_get_double(value);
                if (x < minimum[f])
                    minimum[f] = x;
                if (x > maximum[f])
                    maximum[f] = x;
                sum[f] += x;
                count[f]++;
            }
            invalid += bad;
            json_object_array_add(samples, cell);
        }
    const char *fields[] = {"speed",          "dye",        "solid",    "vx", "vy", "vz",
                            "pressure_proxy", "divergence", "vorticity"};
    const char *units[] = {"m/s", "simulation dye scalar", "mask", "m/s", "m/s",
                           "m/s", "solver proxy",          "1/s",  "1/s"};
    struct json_object *names = json_object_new_array(), *unit_list = json_object_new_array();
    for (int f = 0; f < 9; f++) {
        struct json_object *stat = json_object_new_object();
        ob_number(stat, "min", minimum[f]);
        ob_number(stat, "max", maximum[f]);
        ob_number(stat, "mean", count[f] ? sum[f] / count[f] : NAN);
        json_object_object_add(stat, "finite_samples", json_object_new_int(count[f]));
        json_object_object_add(stats, fields[f], stat);
        json_object_array_add(names, json_object_new_string(fields[f]));
        json_object_array_add(unit_list, json_object_new_string(units[f]));
    }
    json_object_object_add(out, "fields", names);
    json_object_object_add(out, "units", unit_list);
    json_object_object_add(out, "statistics", stats);
    json_object_object_add(out, "samples", samples);
    json_object_object_add(out, "width", json_object_new_int(width));
    json_object_object_add(out, "height", json_object_new_int(height));
    json_object_object_add(out, "plane", json_object_new_string(plane ? plane : "XY"));
    json_object_object_add(out, "slice_index", json_object_new_int(index));
    json_object_object_add(out, "normal_axis", json_object_new_int(n));
    json_object_object_add(out, "u_axis", json_object_new_int(u));
    json_object_object_add(out, "v_axis", json_object_new_int(v));
    ob_number(out, "speed_max", isfinite(maximum[0]) ? maximum[0] : 0);
    ob_number(out, "extent_u_m", dims[u] * r.voxel_size);
    ob_number(out, "extent_v_m", dims[v] * r.voxel_size);
    double origin[3] = {r.world_min_x, r.world_min_y, r.world_min_z};
    ob_number(out, "slice_world_m", origin[n] + (index + .5) * r.voxel_size);
    struct json_object *bounds = json_object_new_array(), *grid = json_object_new_array();
    for (int a = 0; a < 3; a++) {
        json_object_array_add(bounds, json_object_new_double(origin[a]));
        json_object_array_add(grid, json_object_new_int(dims[a]));
    }
    for (int a = 0; a < 3; a++)
        json_object_array_add(bounds, json_object_new_double(origin[a] + dims[a] * r.voxel_size));
    json_object_object_add(out, "world_bounds_m", bounds);
    json_object_object_add(out, "grid", grid);
    json_object_object_add(out, "sampled_nonfinite_cells", json_object_new_int(invalid));
    json_object_object_add(
        out, "sampling",
        json_object_new_string(
            "nearest sparse cells; statistics include solids; derivatives use adjacent cells, "
            "one-sided at domain edges; obstacle interfaces are not wall-corrected"));
    struct json_object *points = ob_get(request, "points"), *probes = json_object_new_array();
    size_t total = points ? json_object_array_length(points) : 0;
    if (total > 16)
        total = 16;
    for (size_t i = 0; i < total; i++) {
        struct json_object *point = json_object_array_get_idx(points, i),
                           *probe = json_object_new_object(), *coord = json_object_new_array(),
                           *center = json_object_new_array();
        int q[3];
        bool inside = true;
        for (int a = 0; a < 3; a++) {
            double x = json_object_get_double(json_object_array_get_idx(point, a));
            double cell_index = floor((x - origin[a]) / r.voxel_size);
            q[a] = cell_index < 0 ? -1 : cell_index >= dims[a] ? dims[a] : (int)cell_index;
            if (q[a] < 0 || q[a] >= dims[a])
                inside = false;
            json_object_array_add(coord, json_object_new_int(q[a]));
            json_object_array_add(center,
                                  json_object_new_double(origin[a] + (q[a] + .5) * r.voxel_size));
        }
        json_object_object_add(probe, "requested_world_m", json_object_get(point));
        json_object_object_add(probe, "inside", json_object_new_boolean(inside));
        json_object_object_add(probe, "cell", coord);
        json_object_object_add(probe, "cell_center_world_m", center);
        if (inside)
            json_object_object_add(probe, "values", sample_cell(state, dims, r.voxel_size, q));
        json_object_array_add(probes, probe);
    }
    json_object_object_add(out, "probes", probes);
    return out;
}
