#include "app/sim_runtime_3d_domain.h"

#include "import/runtime_scene_bridge.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

enum {
    SIM_RUNTIME_3D_GRID_MIN = 4,
    SIM_RUNTIME_3D_MAJOR_AXIS_PREVIEW = 40,
    SIM_RUNTIME_3D_MAJOR_AXIS_BALANCED = 56,
    SIM_RUNTIME_3D_MAJOR_AXIS_HIGH = 72,
    SIM_RUNTIME_3D_MAJOR_AXIS_DEEP = 96,
    SIM_RUNTIME_3D_MAJOR_AXIS_KARMAN = 72,
    SIM_RUNTIME_3D_MAJOR_AXIS_MAX = 96
};

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

static bool fill_desc_from_world_bounds(float min_x,
                                        float min_y,
                                        float min_z,
                                        float max_x,
                                        float max_y,
                                        float max_z,
                                        int major_axis_cells,
                                        SimRuntime3DDomainDesc *out_desc) {
    SimRuntime3DDomainDesc desc = {0};
    float extent_x = max_x - min_x;
    float extent_y = max_y - min_y;
    float extent_z = max_z - min_z;
    float max_extent = 1.0f;

    if (!out_desc) return false;
    extent_x = sanitize_extent(extent_x);
    extent_y = sanitize_extent(extent_y);
    extent_z = sanitize_extent(extent_z);

    max_extent = extent_x;
    if (extent_y > max_extent) max_extent = extent_y;
    if (extent_z > max_extent) max_extent = extent_z;
    if (max_extent <= 0.0f) max_extent = 1.0f;
    if (major_axis_cells < SIM_RUNTIME_3D_GRID_MIN) {
        major_axis_cells = SIM_RUNTIME_3D_GRID_MIN;
    }

    desc.voxel_size = max_extent / (float)major_axis_cells;
    if (desc.voxel_size <= 0.0f) {
        desc.voxel_size = 1.0f / (float)major_axis_cells;
    }

    desc.grid_w = clamp_int((int)ceilf(extent_x / desc.voxel_size),
                            SIM_RUNTIME_3D_GRID_MIN,
                            INT_MAX);
    desc.grid_h = clamp_int((int)ceilf(extent_y / desc.voxel_size),
                            SIM_RUNTIME_3D_GRID_MIN,
                            INT_MAX);
    desc.grid_d = clamp_int((int)ceilf(extent_z / desc.voxel_size),
                            SIM_RUNTIME_3D_GRID_MIN,
                            INT_MAX);
    if (!compute_cell_counts(desc.grid_w,
                             desc.grid_h,
                             desc.grid_d,
                             &desc.slice_cell_count,
                             &desc.cell_count)) {
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

int sim_runtime_3d_major_axis_cells_for_config(const AppConfig *cfg) {
    int custom_major_axis = 0;
    if (!cfg) return SIM_RUNTIME_3D_MAJOR_AXIS_BALANCED;
    switch (cfg->quality_index) {
    case 0: return SIM_RUNTIME_3D_MAJOR_AXIS_PREVIEW;
    case 1: return SIM_RUNTIME_3D_MAJOR_AXIS_BALANCED;
    case 2: return SIM_RUNTIME_3D_MAJOR_AXIS_HIGH;
    case 3: return SIM_RUNTIME_3D_MAJOR_AXIS_DEEP;
    case 4: return SIM_RUNTIME_3D_MAJOR_AXIS_KARMAN;
    default: break;
    }
    custom_major_axis = cfg->grid_w > cfg->grid_h ? cfg->grid_w : cfg->grid_h;
    custom_major_axis = (int)lroundf((float)custom_major_axis * 0.5f);
    return clamp_int(custom_major_axis,
                     SIM_RUNTIME_3D_GRID_MIN,
                     SIM_RUNTIME_3D_MAJOR_AXIS_MAX);
}

bool sim_runtime_3d_domain_desc_resolve(const AppConfig *cfg,
                                        const FluidScenePreset *preset,
                                        const PhysicsSimRuntimeVisualBootstrap *runtime_visual,
                                        SimRuntime3DDomainDesc *out_desc) {
    const PhysicsSimRetainedRuntimeScene *retained = NULL;
    int major_axis_cells = 0;
    if (!cfg || !out_desc) return false;

    major_axis_cells = sim_runtime_3d_major_axis_cells_for_config(cfg);
    if (runtime_visual && runtime_visual->scene_domain.enabled) {
        return fill_desc_from_world_bounds((float)runtime_visual->scene_domain.min.x,
                                           (float)runtime_visual->scene_domain.min.y,
                                           (float)runtime_visual->scene_domain.min.z,
                                           (float)runtime_visual->scene_domain.max.x,
                                           (float)runtime_visual->scene_domain.max.y,
                                           (float)runtime_visual->scene_domain.max.z,
                                           major_axis_cells,
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
                                           major_axis_cells,
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
    int major_axis_cells = 0;

    if (!cfg || !out_desc) return false;

    if (preset) {
        extent_x = sanitize_extent(preset->domain_width);
        extent_y = sanitize_extent(preset->domain_height);
        extent_z = sanitize_extent(preset->domain_width < preset->domain_height
                                       ? preset->domain_width
                                       : preset->domain_height);
    }

    major_axis_cells = sim_runtime_3d_major_axis_cells_for_config(cfg);
    return fill_desc_from_world_bounds(0.0f,
                                       0.0f,
                                       0.0f,
                                       extent_x,
                                       extent_y,
                                       extent_z,
                                       major_axis_cells,
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
