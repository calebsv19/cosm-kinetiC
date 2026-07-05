#include "app/water_object_coupling.h"

#include <math.h>
#include <string.h>

#include "app/sim_runtime_3d_domain.h"
#include "app/sim_runtime_backend.h"

static int clamp_i(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static void resolve_axis_bounds(int cells,
                                float min_fraction,
                                float max_fraction,
                                int *out_min,
                                int *out_max) {
    int lo = 0;
    int hi = 0;
    int interior_min = cells > 2 ? 1 : 0;
    int interior_max = cells > 2 ? cells - 2 : cells - 1;
    if (!out_min || !out_max || cells <= 0) return;
    lo = (int)floorf((float)cells * min_fraction);
    hi = (int)ceilf((float)cells * max_fraction) - 1;
    lo = clamp_i(lo, interior_min, interior_max);
    hi = clamp_i(hi, interior_min, interior_max);
    if (hi < lo) hi = lo;
    if (hi == lo && hi < interior_max) hi += 1;
    *out_min = lo;
    *out_max = hi;
}

bool water_object_coupling_enabled(const AppConfig *cfg) {
    return cfg && cfg->water_object_fixture;
}

bool water_object_coupling_grid_bounds(const AppConfig *cfg,
                                       const SceneFluidVolumeExportView3D *volume,
                                       WaterObjectCouplingGridBounds *out_bounds) {
    WaterObjectCouplingGridBounds bounds = {0};
    if (!out_bounds) return false;
    memset(out_bounds, 0, sizeof(*out_bounds));
    if (!water_object_coupling_enabled(cfg) || !volume ||
        volume->width <= 0 || volume->height <= 0 || volume->depth <= 0) {
        return false;
    }
    resolve_axis_bounds(volume->width, 0.42f, 0.58f, &bounds.min_x, &bounds.max_x);
    resolve_axis_bounds(volume->height, 0.20f, 0.70f, &bounds.min_y, &bounds.max_y);
    resolve_axis_bounds(volume->depth, 0.42f, 0.58f, &bounds.min_z, &bounds.max_z);
    bounds.active = true;
    *out_bounds = bounds;
    return true;
}

bool water_object_coupling_cell_in_fixture(const WaterObjectCouplingGridBounds *bounds,
                                           int x,
                                           int y,
                                           int z) {
    return bounds && bounds->active &&
           x >= bounds->min_x && x <= bounds->max_x &&
           y >= bounds->min_y && y <= bounds->max_y &&
           z >= bounds->min_z && z <= bounds->max_z;
}

size_t water_object_coupling_apply_fixture(SceneState *scene) {
    SceneFluidVolumeExportView3D volume = {0};
    WaterObjectCouplingGridBounds bounds = {0};
    size_t stamped = 0u;
    if (!scene || !scene->backend || !water_object_coupling_enabled(scene->config)) return 0u;
    if (!scene_backend_volume_export_view_3d(scene, &volume) ||
        !water_object_coupling_grid_bounds(scene->config, &volume, &bounds)) {
        return 0u;
    }
    for (int z = bounds.min_z; z <= bounds.max_z; ++z) {
        for (int y = bounds.min_y; y <= bounds.max_y; ++y) {
            for (int x = bounds.min_x; x <= bounds.max_x; ++x) {
                if (sim_runtime_backend_debug_write_volume_cell_3d(scene->backend,
                                                                   x,
                                                                   y,
                                                                   z,
                                                                   0.0f,
                                                                   0.0f,
                                                                   0.0f,
                                                                   0.0f,
                                                                   0.0f,
                                                                   1u)) {
                    stamped++;
                }
            }
        }
    }
    return stamped;
}

float water_object_coupling_surface_delta(const SceneFluidVolumeExportView3D *volume,
                                          const VolumeFrameHeaderVf3dV1 *volume_header,
                                          const WaterObjectCouplingGridBounds *bounds,
                                          int x,
                                          int z,
                                          float displaced_volume_m3,
                                          float *out_extent_weight,
                                          bool *out_was_capped) {
    float cx = 0.0f;
    float cz = 0.0f;
    float hx = 1.0f;
    float hz = 1.0f;
    float nx = 0.0f;
    float nz = 0.0f;
    float r2 = 0.0f;
    float radial = 0.0f;
    float footprint_area = 1.0f;
    float volume_height = 0.0f;
    float t = volume_header ? (float)volume_header->time_seconds : 0.0f;
    float weight = 0.0f;
    float base_delta = 0.0f;
    float ring_wake = 0.0f;
    float shear_wake = 0.0f;
    float wake_delta = 0.0f;
    float delta = 0.0f;
    if (out_extent_weight) *out_extent_weight = 0.0f;
    if (out_was_capped) *out_was_capped = false;
    if (!volume || !bounds || !bounds->active || displaced_volume_m3 <= 0.0f ||
        volume->voxel_size <= 0.0f) {
        return 0.0f;
    }
    cx = 0.5f * (float)(bounds->min_x + bounds->max_x);
    cz = 0.5f * (float)(bounds->min_z + bounds->max_z);
    hx = 0.5f * (float)(bounds->max_x - bounds->min_x + 1) + 4.0f;
    hz = 0.5f * (float)(bounds->max_z - bounds->min_z + 1) + 4.0f;
    nx = ((float)x - cx) / hx;
    nz = ((float)z - cz) / hz;
    r2 = nx * nx + nz * nz;
    if (r2 > 5.2f) return 0.0f;
    radial = sqrtf(r2);
    weight = expf(-0.50f * r2);
    footprint_area = (float)((bounds->max_x - bounds->min_x + 1) *
                             (bounds->max_z - bounds->min_z + 1)) *
                     volume->voxel_size * volume->voxel_size;
    volume_height = footprint_area > 0.0f ? displaced_volume_m3 / footprint_area : 0.0f;
    base_delta = volume_height * 0.035f * weight;
    ring_wake = sinf((radial * 5.2f) - (t * 2.0f)) * expf(-1.10f * r2);
    shear_wake = sinf((((float)x - cx) * 0.23f) +
                      (((float)z - cz) * 0.17f) +
                      (t * 1.4f)) *
                 expf(-0.85f * r2);
    wake_delta = volume->voxel_size * ((0.018f * ring_wake) + (0.008f * shear_wake));
    delta = base_delta + wake_delta;
    if (delta > 0.20f * volume->voxel_size) {
        delta = 0.20f * volume->voxel_size;
        if (out_was_capped) *out_was_capped = true;
    }
    if (delta < -0.08f * volume->voxel_size) {
        delta = -0.08f * volume->voxel_size;
        if (out_was_capped) *out_was_capped = true;
    }
    if (out_extent_weight) *out_extent_weight = weight;
    return delta;
}

void water_object_coupling_accumulate_diagnostics(const AppConfig *cfg,
                                                  const SceneFluidVolumeExportView3D *volume,
                                                  const VolumeFrameHeaderVf3dV1 *volume_header,
                                                  WaterObjectCouplingDiagnostics *out_diag) {
    WaterObjectCouplingGridBounds bounds = {0};
    SimRuntime3DDomainDesc desc = {0};
    if (!out_diag) return;
    memset(out_diag, 0, sizeof(*out_diag));
    out_diag->enabled = water_object_coupling_enabled(cfg);
    if (!out_diag->enabled || !volume ||
        !water_object_coupling_grid_bounds(cfg, volume, &bounds) ||
        !volume->solid_mask) {
        return;
    }
    out_diag->fixture_active = true;
    out_diag->affected_min_x = bounds.min_x;
    out_diag->affected_max_x = bounds.max_x;
    out_diag->affected_min_z = bounds.min_z;
    out_diag->affected_max_z = bounds.max_z;
    desc.grid_w = volume->width;
    desc.grid_h = volume->height;
    desc.grid_d = volume->depth;
    desc.slice_cell_count = (size_t)volume->width * (size_t)volume->height;
    (void)volume_header;
    for (int z = bounds.min_z; z <= bounds.max_z; ++z) {
        for (int x = bounds.min_x; x <= bounds.max_x; ++x) {
            bool column_has_object = false;
            for (int y = bounds.min_y; y <= bounds.max_y; ++y) {
                size_t idx = sim_runtime_3d_volume_index(&desc, x, y, z);
                float cell_top_y = volume->origin_y + ((float)y + 1.0f) * volume->voxel_size;
                if (volume->solid_mask[idx] == 0u) continue;
                out_diag->object_solid_cells++;
                column_has_object = true;
                if (cell_top_y <= volume->origin_y + cfg->water_level *
                                          ((float)volume->height * volume->voxel_size)) {
                    out_diag->object_wet_overlap_cells++;
                }
            }
            if (column_has_object) out_diag->object_footprint_columns++;
        }
    }
    out_diag->displaced_volume_m3 =
        (float)out_diag->object_wet_overlap_cells * volume->voxel_size *
        volume->voxel_size * volume->voxel_size;
}
