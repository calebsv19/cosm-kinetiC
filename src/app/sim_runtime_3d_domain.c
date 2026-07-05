#include "app/sim_runtime_3d_domain.h"

#include "import/runtime_scene_bridge.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

enum {
    SIM_RUNTIME_3D_GRID_MIN = 4,
    SIM_RUNTIME_3D_MAJOR_AXIS_DEFAULT = 64,
    SIM_RUNTIME_3D_MAJOR_AXIS_MAX = 256
};

enum {
    SIM_RUNTIME_3D_VOLUME_FIELD_COUNT = 5,
    SIM_RUNTIME_3D_SCRATCH_FIELD_COUNT = 6,
    SIM_RUNTIME_3D_SLICE_FLOAT_FIELD_COUNT = 7
};

static const size_t SIM_RUNTIME_3D_RESIDENT_BYTES_BUDGET =
    (size_t)320u * 1024u * 1024u;

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float sanitize_extent(float value) {
    return value > 0.0f ? value : 1.0f;
}

static bool compute_cell_counts(int grid_w,
                                int grid_h,
                                int grid_d,
                                size_t *out_slice_count,
                                size_t *out_cell_count) {
    size_t slice_count = 0;
    if (!out_slice_count || !out_cell_count) return false;
    if (grid_w < SIM_RUNTIME_3D_GRID_MIN ||
        grid_h < SIM_RUNTIME_3D_GRID_MIN ||
        grid_d < SIM_RUNTIME_3D_GRID_MIN) {
        return false;
    }
    if ((size_t)grid_w > SIZE_MAX / (size_t)grid_h) return false;
    slice_count = (size_t)grid_w * (size_t)grid_h;
    if (slice_count > SIZE_MAX / (size_t)grid_d) return false;
    *out_slice_count = slice_count;
    *out_cell_count = slice_count * (size_t)grid_d;
    return true;
}

static bool estimate_resident_bytes_for_counts(size_t slice_count,
                                               size_t cell_count,
                                               size_t *out_bytes) {
    size_t full_volume_float_count = 0;
    size_t full_volume_float_bytes = 0;
    size_t full_volume_mask_bytes = 0;
    size_t slice_float_count = 0;
    size_t slice_float_bytes = 0;
    size_t slice_mask_bytes = 0;
    size_t total = 0;

    if (!out_bytes) return false;

    if (cell_count > SIZE_MAX / (size_t)(SIM_RUNTIME_3D_VOLUME_FIELD_COUNT +
                                         SIM_RUNTIME_3D_SCRATCH_FIELD_COUNT)) {
        return false;
    }
    full_volume_float_count =
        cell_count * (size_t)(SIM_RUNTIME_3D_VOLUME_FIELD_COUNT + SIM_RUNTIME_3D_SCRATCH_FIELD_COUNT);
    if (full_volume_float_count > SIZE_MAX / sizeof(float)) return false;
    full_volume_float_bytes = full_volume_float_count * sizeof(float);

    if (cell_count > SIZE_MAX / sizeof(uint8_t)) return false;
    full_volume_mask_bytes = cell_count * sizeof(uint8_t);

    if (slice_count > SIZE_MAX / (size_t)SIM_RUNTIME_3D_SLICE_FLOAT_FIELD_COUNT) {
        return false;
    }
    slice_float_count = slice_count * (size_t)SIM_RUNTIME_3D_SLICE_FLOAT_FIELD_COUNT;
    if (slice_float_count > SIZE_MAX / sizeof(float)) return false;
    slice_float_bytes = slice_float_count * sizeof(float);

    if (slice_count > SIZE_MAX / sizeof(uint8_t)) return false;
    slice_mask_bytes = slice_count * sizeof(uint8_t);

    total = full_volume_float_bytes;
    if (SIZE_MAX - total < full_volume_mask_bytes) return false;
    total += full_volume_mask_bytes;
    if (SIZE_MAX - total < slice_float_bytes) return false;
    total += slice_float_bytes;
    if (SIZE_MAX - total < slice_mask_bytes) return false;
    total += slice_mask_bytes;

    *out_bytes = total;
    return true;
}

static bool fill_counts_from_voxel_size(float extent_x,
                                        float extent_y,
                                        float extent_z,
                                        SimRuntime3DDomainDesc *desc) {
    if (!desc || desc->voxel_size <= 0.0f) return false;

    desc->grid_w = clamp_int((int)ceilf(extent_x / desc->voxel_size),
                             SIM_RUNTIME_3D_GRID_MIN,
                             INT_MAX);
    desc->grid_h = clamp_int((int)ceilf(extent_y / desc->voxel_size),
                             SIM_RUNTIME_3D_GRID_MIN,
                             INT_MAX);
    desc->grid_d = clamp_int((int)ceilf(extent_z / desc->voxel_size),
                             SIM_RUNTIME_3D_GRID_MIN,
                             INT_MAX);
    desc->applied_major_axis_cells =
        desc->grid_w > desc->grid_h ? desc->grid_w : desc->grid_h;
    desc->applied_depth_cells = desc->grid_d;
    return compute_cell_counts(desc->grid_w,
                               desc->grid_h,
                               desc->grid_d,
                               &desc->slice_cell_count,
                               &desc->cell_count);
}

static bool domain_desc_estimated_resident_bytes(const SimRuntime3DDomainDesc *desc,
                                                 size_t *out_bytes) {
    if (!desc) return false;
    return estimate_resident_bytes_for_counts(desc->slice_cell_count,
                                              desc->cell_count,
                                              out_bytes);
}

static bool apply_resident_budget_limit(float extent_x,
                                        float extent_y,
                                        float extent_z,
                                        SimRuntime3DDomainDesc *desc) {
    size_t estimated_bytes = 0;

    if (!desc) return false;
    if (!domain_desc_estimated_resident_bytes(desc, &estimated_bytes)) return false;
    if (estimated_bytes <= SIM_RUNTIME_3D_RESIDENT_BYTES_BUDGET) return true;

    for (int pass = 0; pass < 16; ++pass) {
        float next_voxel_size = 0.0f;
        double scale = cbrt((double)estimated_bytes /
                            (double)SIM_RUNTIME_3D_RESIDENT_BYTES_BUDGET);
        int prev_w = desc->grid_w;
        int prev_h = desc->grid_h;
        int prev_d = desc->grid_d;

        if (!(scale > 1.0)) break;
        next_voxel_size = desc->voxel_size * (float)scale;
        if (next_voxel_size <= desc->voxel_size) {
            next_voxel_size = desc->voxel_size * 1.02f;
        }

        desc->voxel_size = next_voxel_size;
        if (!fill_counts_from_voxel_size(extent_x, extent_y, extent_z, desc)) {
            return false;
        }
        if (desc->grid_w == prev_w &&
            desc->grid_h == prev_h &&
            desc->grid_d == prev_d) {
            desc->voxel_size *= 1.02f;
            if (!fill_counts_from_voxel_size(extent_x, extent_y, extent_z, desc)) {
                return false;
            }
        }
        if (!domain_desc_estimated_resident_bytes(desc, &estimated_bytes)) return false;
        if (estimated_bytes <= SIM_RUNTIME_3D_RESIDENT_BYTES_BUDGET) return true;
    }

    return estimated_bytes <= SIM_RUNTIME_3D_RESIDENT_BYTES_BUDGET;
}

static bool fill_desc_from_world_bounds(float min_x,
                                        float min_y,
                                        float min_z,
                                        float max_x,
                                        float max_y,
                                        float max_z,
                                        int requested_grid_x_cells,
                                        int requested_grid_y_cells,
                                        int requested_depth_cells,
                                        SimRuntime3DDepthPolicy depth_policy,
                                        SimRuntime3DDomainDesc *out_desc) {
    SimRuntime3DDomainDesc desc = {0};
    float extent_x = max_x - min_x;
    float extent_y = max_y - min_y;
    float extent_z = max_z - min_z;
    float max_extent = 1.0f;
    float voxel_size_x = 1.0f;
    float voxel_size_y = 1.0f;
    float voxel_size_z = 0.0f;
    int applied_grid_x_cells = 0;
    int applied_grid_y_cells = 0;
    int applied_depth_cells = 0;

    if (!out_desc) return false;
    extent_x = sanitize_extent(extent_x);
    extent_y = sanitize_extent(extent_y);
    extent_z = sanitize_extent(extent_z);

    max_extent = extent_x;
    if (extent_y > max_extent) max_extent = extent_y;
    if (extent_z > max_extent) max_extent = extent_z;
    if (max_extent <= 0.0f) max_extent = 1.0f;
    requested_grid_x_cells =
        requested_grid_x_cells > 0 ? requested_grid_x_cells : SIM_RUNTIME_3D_MAJOR_AXIS_DEFAULT;
    requested_grid_y_cells =
        requested_grid_y_cells > 0 ? requested_grid_y_cells : SIM_RUNTIME_3D_MAJOR_AXIS_DEFAULT;
    desc.requested_major_axis_cells =
        requested_grid_x_cells > requested_grid_y_cells
            ? requested_grid_x_cells
            : requested_grid_y_cells;
    applied_grid_x_cells =
        sim_runtime_3d_applied_major_axis_cells_for_requested(requested_grid_x_cells);
    applied_grid_y_cells =
        sim_runtime_3d_applied_major_axis_cells_for_requested(requested_grid_y_cells);
    desc.applied_major_axis_cells =
        applied_grid_x_cells > applied_grid_y_cells
            ? applied_grid_x_cells
            : applied_grid_y_cells;
    desc.requested_depth_cells = requested_depth_cells > 0 ? requested_depth_cells : 0;
    desc.depth_policy = depth_policy;
    if (desc.applied_major_axis_cells < SIM_RUNTIME_3D_GRID_MIN) {
        desc.applied_major_axis_cells = SIM_RUNTIME_3D_GRID_MIN;
    }
    applied_depth_cells =
        sim_runtime_3d_applied_depth_cells_for_requested(desc.requested_depth_cells);

    voxel_size_x = extent_x / (float)applied_grid_x_cells;
    voxel_size_y = extent_y / (float)applied_grid_y_cells;
    desc.voxel_size = voxel_size_x > voxel_size_y ? voxel_size_x : voxel_size_y;
    if (applied_depth_cells > 0) {
        voxel_size_z = extent_z / (float)applied_depth_cells;
        if (voxel_size_z > desc.voxel_size) {
            desc.voxel_size = voxel_size_z;
        }
    }
    if (desc.voxel_size <= 0.0f) {
        desc.voxel_size = max_extent / (float)desc.applied_major_axis_cells;
    }
    if (desc.voxel_size <= 0.0f) {
        desc.voxel_size = 1.0f / (float)desc.applied_major_axis_cells;
    }

    if (!fill_counts_from_voxel_size(extent_x, extent_y, extent_z, &desc)) {
        return false;
    }
    if (!apply_resident_budget_limit(extent_x, extent_y, extent_z, &desc)) {
        return false;
    }

    desc.world_min_x = min_x;
    desc.world_min_y = min_y;
    desc.world_min_z = min_z;
    desc.world_max_x = min_x + extent_x;
    desc.world_max_y = min_y + extent_y;
    desc.world_max_z = min_z + extent_z;

    *out_desc = desc;
    return true;
}

size_t sim_runtime_3d_domain_estimated_resident_bytes(const SimRuntime3DDomainDesc *desc) {
    size_t bytes = 0;
    if (!domain_desc_estimated_resident_bytes(desc, &bytes)) return 0;
    return bytes;
}

size_t sim_runtime_3d_domain_resident_bytes_budget(void) {
    return SIM_RUNTIME_3D_RESIDENT_BYTES_BUDGET;
}

int sim_runtime_3d_requested_major_axis_cells_for_config(const AppConfig *cfg) {
    int custom_major_axis = 0;
    if (!cfg) return SIM_RUNTIME_3D_MAJOR_AXIS_DEFAULT;
    custom_major_axis = cfg->grid_w > cfg->grid_h ? cfg->grid_w : cfg->grid_h;
    if (custom_major_axis <= 0) {
        return SIM_RUNTIME_3D_MAJOR_AXIS_DEFAULT;
    }
    return custom_major_axis;
}

int sim_runtime_3d_applied_major_axis_cells_for_requested(int requested_major_axis_cells) {
    if (requested_major_axis_cells <= 0) {
        return SIM_RUNTIME_3D_MAJOR_AXIS_DEFAULT;
    }
    return clamp_int(requested_major_axis_cells,
                     SIM_RUNTIME_3D_GRID_MIN,
                     SIM_RUNTIME_3D_MAJOR_AXIS_MAX);
}

int sim_runtime_3d_major_axis_cells_for_config(const AppConfig *cfg) {
    return sim_runtime_3d_applied_major_axis_cells_for_requested(
        sim_runtime_3d_requested_major_axis_cells_for_config(cfg));
}

int sim_runtime_3d_requested_depth_cells_for_config(const AppConfig *cfg) {
    if (!cfg || cfg->grid_d <= 0) return 0;
    return cfg->grid_d;
}

int sim_runtime_3d_applied_depth_cells_for_requested(int requested_depth_cells) {
    if (requested_depth_cells <= 0) {
        return 0;
    }
    return clamp_int(requested_depth_cells,
                     SIM_RUNTIME_3D_GRID_MIN,
                     SIM_RUNTIME_3D_MAJOR_AXIS_MAX);
}

const char *sim_runtime_3d_depth_policy_label(SimRuntime3DDepthPolicy policy) {
    switch (policy) {
    case SIM_RUNTIME_3D_DEPTH_POLICY_CONFIGURED_DEPTH_CELLS:
        return "Configured Z cells";
    case SIM_RUNTIME_3D_DEPTH_POLICY_SCENE_DOMAIN_BOUNDS:
        return "Scene bounds Z";
    case SIM_RUNTIME_3D_DEPTH_POLICY_RETAINED_SCENE_BOUNDS:
        return "Retained bounds Z";
    case SIM_RUNTIME_3D_DEPTH_POLICY_LEGACY_MIN_XY:
        return "Legacy min(X,Y)";
    case SIM_RUNTIME_3D_DEPTH_POLICY_WATER_BASIN_SQUARE_XZ:
        return "Water basin square X/Z";
    case SIM_RUNTIME_3D_DEPTH_POLICY_NONE:
    default:
        return "Unspecified";
    }
}

bool sim_runtime_3d_domain_desc_resolve(const AppConfig *cfg,
                                        const FluidScenePreset *preset,
                                        const PhysicsSimRuntimeVisualBootstrap *runtime_visual,
                                        SimRuntime3DDomainDesc *out_desc) {
    const PhysicsSimRetainedRuntimeScene *retained = NULL;
    int requested_grid_x_cells = 0;
    int requested_grid_y_cells = 0;
    int requested_depth_cells = 0;
    if (!cfg || !out_desc) return false;

    requested_grid_x_cells =
        cfg->grid_w > 0 ? cfg->grid_w : SIM_RUNTIME_3D_MAJOR_AXIS_DEFAULT;
    requested_grid_y_cells =
        cfg->grid_h > 0 ? cfg->grid_h : SIM_RUNTIME_3D_MAJOR_AXIS_DEFAULT;
    requested_depth_cells = sim_runtime_3d_requested_depth_cells_for_config(cfg);
    if (runtime_visual && runtime_visual->scene_domain.enabled) {
        return fill_desc_from_world_bounds((float)runtime_visual->scene_domain.min.x,
                                           (float)runtime_visual->scene_domain.min.y,
                                           (float)runtime_visual->scene_domain.min.z,
                                           (float)runtime_visual->scene_domain.max.x,
                                           (float)runtime_visual->scene_domain.max.y,
                                           (float)runtime_visual->scene_domain.max.z,
                                           requested_grid_x_cells,
                                           requested_grid_y_cells,
                                           requested_depth_cells,
                                           requested_depth_cells > 0
                                               ? SIM_RUNTIME_3D_DEPTH_POLICY_CONFIGURED_DEPTH_CELLS
                                               : SIM_RUNTIME_3D_DEPTH_POLICY_SCENE_DOMAIN_BOUNDS,
                                           out_desc);
    }

    retained = runtime_visual ? &runtime_visual->retained_scene : NULL;
    if (retained &&
        retained->has_line_drawing_scene3d &&
        retained->bounds.enabled) {
        return fill_desc_from_world_bounds((float)retained->bounds.min.x,
                                           (float)retained->bounds.min.y,
                                           (float)retained->bounds.min.z,
                                           (float)retained->bounds.max.x,
                                           (float)retained->bounds.max.y,
                                           (float)retained->bounds.max.z,
                                           requested_grid_x_cells,
                                           requested_grid_y_cells,
                                           requested_depth_cells,
                                           requested_depth_cells > 0
                                               ? SIM_RUNTIME_3D_DEPTH_POLICY_CONFIGURED_DEPTH_CELLS
                                               : SIM_RUNTIME_3D_DEPTH_POLICY_RETAINED_SCENE_BOUNDS,
                                           out_desc);
    }

    return sim_runtime_3d_domain_desc_from_legacy(cfg, preset, out_desc);
}

bool sim_runtime_3d_domain_desc_from_legacy(const AppConfig *cfg,
                                            const FluidScenePreset *preset,
                                            SimRuntime3DDomainDesc *out_desc) {
    float extent_x = 1.0f;
    float extent_y = 1.0f;
    float extent_z = 1.0f;
    int requested_grid_x_cells = 0;
    int requested_grid_y_cells = 0;
    int requested_depth_cells = 0;

    if (!cfg || !out_desc) return false;

    if (preset) {
        extent_x = sanitize_extent(preset->domain_width);
        extent_y = sanitize_extent(preset->domain_height);
        if (preset->domain == SCENE_DOMAIN_WATER) {
            extent_z = extent_x;
        } else {
            extent_z = sanitize_extent(preset->domain_width < preset->domain_height
                                           ? preset->domain_width
                                           : preset->domain_height);
        }
    }

    requested_grid_x_cells =
        cfg->grid_w > 0 ? cfg->grid_w : SIM_RUNTIME_3D_MAJOR_AXIS_DEFAULT;
    requested_grid_y_cells =
        cfg->grid_h > 0 ? cfg->grid_h : SIM_RUNTIME_3D_MAJOR_AXIS_DEFAULT;
    requested_depth_cells = sim_runtime_3d_requested_depth_cells_for_config(cfg);
    return fill_desc_from_world_bounds(0.0f,
                                       0.0f,
                                       0.0f,
                                       extent_x,
                                       extent_y,
                                       extent_z,
                                       requested_grid_x_cells,
                                       requested_grid_y_cells,
                                       requested_depth_cells,
                                       requested_depth_cells > 0
                                           ? SIM_RUNTIME_3D_DEPTH_POLICY_CONFIGURED_DEPTH_CELLS
                                           : (preset && preset->domain == SCENE_DOMAIN_WATER
                                                  ? SIM_RUNTIME_3D_DEPTH_POLICY_WATER_BASIN_SQUARE_XZ
                                                  : SIM_RUNTIME_3D_DEPTH_POLICY_LEGACY_MIN_XY),
                                       out_desc);
}

bool sim_runtime_3d_volume_init(SimRuntime3DVolume *volume,
                                const SimRuntime3DDomainDesc *desc) {
    SimRuntime3DVolume next = {0};

    if (!volume || !desc) return false;
    if (desc->grid_w < SIM_RUNTIME_3D_GRID_MIN ||
        desc->grid_h < SIM_RUNTIME_3D_GRID_MIN ||
        desc->grid_d < SIM_RUNTIME_3D_GRID_MIN ||
        desc->cell_count == 0) {
        return false;
    }

    next.desc = *desc;
    next.density = (float *)calloc(desc->cell_count, sizeof(float));
    next.velocity_x = (float *)calloc(desc->cell_count, sizeof(float));
    next.velocity_y = (float *)calloc(desc->cell_count, sizeof(float));
    next.velocity_z = (float *)calloc(desc->cell_count, sizeof(float));
    next.pressure = (float *)calloc(desc->cell_count, sizeof(float));
    if (!next.density ||
        !next.velocity_x ||
        !next.velocity_y ||
        !next.velocity_z ||
        !next.pressure) {
        sim_runtime_3d_volume_destroy(&next);
        return false;
    }

    *volume = next;
    return true;
}

void sim_runtime_3d_volume_destroy(SimRuntime3DVolume *volume) {
    if (!volume) return;
    free(volume->density);
    free(volume->velocity_x);
    free(volume->velocity_y);
    free(volume->velocity_z);
    free(volume->pressure);
    memset(volume, 0, sizeof(*volume));
}

void sim_runtime_3d_volume_clear(SimRuntime3DVolume *volume) {
    if (!volume || volume->desc.cell_count == 0) return;
    if (volume->density) memset(volume->density, 0, volume->desc.cell_count * sizeof(float));
    if (volume->velocity_x) memset(volume->velocity_x, 0, volume->desc.cell_count * sizeof(float));
    if (volume->velocity_y) memset(volume->velocity_y, 0, volume->desc.cell_count * sizeof(float));
    if (volume->velocity_z) memset(volume->velocity_z, 0, volume->desc.cell_count * sizeof(float));
    if (volume->pressure) memset(volume->pressure, 0, volume->desc.cell_count * sizeof(float));
}

size_t sim_runtime_3d_volume_index(const SimRuntime3DDomainDesc *desc,
                                   int x,
                                   int y,
                                   int z) {
    if (!desc || x < 0 || y < 0 || z < 0) return 0;
    return ((size_t)z * desc->slice_cell_count) +
           ((size_t)y * (size_t)desc->grid_w) +
           (size_t)x;
}
