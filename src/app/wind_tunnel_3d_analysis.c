#include "app/wind_tunnel_3d_analysis.h"

#include <math.h>
#include <string.h>

static size_t wind_volume_index(const SceneFluidVolumeExportView3D *volume, int x, int y, int z) {
    return ((size_t)z * (size_t)volume->height + (size_t)y) * (size_t)volume->width + (size_t)x;
}

static float wind_face_normal_velocity(WindTunnel3DFace face, float vx, float vy, float vz) {
    switch (face) {
    case WIND_TUNNEL_3D_FACE_LEFT:
        return vx;
    case WIND_TUNNEL_3D_FACE_RIGHT:
        return -vx;
    case WIND_TUNNEL_3D_FACE_BOTTOM:
        return vy;
    case WIND_TUNNEL_3D_FACE_TOP:
        return -vy;
    case WIND_TUNNEL_3D_FACE_FRONT:
        return vz;
    case WIND_TUNNEL_3D_FACE_BACK:
        return -vz;
    case WIND_TUNNEL_3D_FACE_NONE:
    default:
        return 0.0f;
    }
}

static float wind_face_outward_velocity(WindTunnel3DFace face, float vx, float vy, float vz) {
    return -wind_face_normal_velocity(face, vx, vy, vz);
}

static bool wind_face_range(const SceneFluidVolumeExportView3D *volume,
                            WindTunnel3DFace face,
                            int slab_cells,
                            int *min_x,
                            int *max_x,
                            int *min_y,
                            int *max_y,
                            int *min_z,
                            int *max_z) {
    int slab = slab_cells > 0 ? slab_cells : 1;
    if (!volume || !min_x || !max_x || !min_y || !max_y || !min_z || !max_z) return false;
    *min_x = 0;
    *max_x = volume->width;
    *min_y = 0;
    *max_y = volume->height;
    *min_z = 0;
    *max_z = volume->depth;
    switch (face) {
    case WIND_TUNNEL_3D_FACE_LEFT:
        if (slab > volume->width) slab = volume->width;
        *max_x = slab;
        return true;
    case WIND_TUNNEL_3D_FACE_RIGHT:
        if (slab > volume->width) slab = volume->width;
        *min_x = volume->width - slab;
        return true;
    case WIND_TUNNEL_3D_FACE_BOTTOM:
        if (slab > volume->height) slab = volume->height;
        *max_y = slab;
        return true;
    case WIND_TUNNEL_3D_FACE_TOP:
        if (slab > volume->height) slab = volume->height;
        *min_y = volume->height - slab;
        return true;
    case WIND_TUNNEL_3D_FACE_FRONT:
        if (slab > volume->depth) slab = volume->depth;
        *max_z = slab;
        return true;
    case WIND_TUNNEL_3D_FACE_BACK:
        if (slab > volume->depth) slab = volume->depth;
        *min_z = volume->depth - slab;
        return true;
    case WIND_TUNNEL_3D_FACE_NONE:
    default:
        return false;
    }
}

static bool wind_face_stats(const SceneFluidVolumeExportView3D *volume,
                            WindTunnel3DFace face,
                            int slab_cells,
                            bool outward_positive,
                            float *out_pressure_avg,
                            float *out_throughput,
                            size_t *out_sampled_cells) {
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
    int min_z = 0;
    int max_z = 0;
    double pressure_sum = 0.0;
    double throughput_sum = 0.0;
    size_t count = 0u;
    float cell_area = 1.0f;

    if (!volume || !out_pressure_avg || !out_throughput || !out_sampled_cells ||
        !wind_face_range(volume, face, slab_cells, &min_x, &max_x, &min_y, &max_y, &min_z, &max_z)) {
        return false;
    }
    cell_area = volume->voxel_size > 0.0f ? volume->voxel_size * volume->voxel_size : 1.0f;
    for (int z = min_z; z < max_z; ++z) {
        for (int y = min_y; y < max_y; ++y) {
            for (int x = min_x; x < max_x; ++x) {
                size_t idx = wind_volume_index(volume, x, y, z);
                float density = volume->density ? volume->density[idx] : 0.0f;
                float pressure = volume->pressure ? volume->pressure[idx] : 0.0f;
                float vx = volume->velocity_x ? volume->velocity_x[idx] : 0.0f;
                float vy = volume->velocity_y ? volume->velocity_y[idx] : 0.0f;
                float vz = volume->velocity_z ? volume->velocity_z[idx] : 0.0f;
                float normal_v = 0.0f;
                normal_v = outward_positive
                               ? wind_face_outward_velocity(face, vx, vy, vz)
                               : wind_face_normal_velocity(face, vx, vy, vz);
                pressure_sum += pressure;
                throughput_sum += (double)density * (double)normal_v * (double)cell_area;
                ++count;
            }
        }
    }
    if (count == 0u) return false;
    *out_pressure_avg = (float)(pressure_sum / (double)count);
    *out_throughput = (float)throughput_sum;
    *out_sampled_cells = count;
    return true;
}

static void wind_vorticity_stats(const SceneFluidVolumeExportView3D *volume,
                                 float *out_avg,
                                 float *out_max,
                                 size_t *in_out_samples) {
    double sum = 0.0;
    float max_vorticity = 0.0f;
    size_t count = 0u;
    float inv_two_h = 0.5f;
    if (!volume || !out_avg || !out_max || volume->width < 3 || volume->height < 3 || volume->depth < 3 ||
        !volume->velocity_x || !volume->velocity_y || !volume->velocity_z) {
        if (out_avg) *out_avg = 0.0f;
        if (out_max) *out_max = 0.0f;
        return;
    }
    if (volume->voxel_size > 0.0f) inv_two_h = 0.5f / volume->voxel_size;
    for (int z = 1; z < volume->depth - 1; ++z) {
        for (int y = 1; y < volume->height - 1; ++y) {
            for (int x = 1; x < volume->width - 1; ++x) {
                size_t idx = wind_volume_index(volume, x, y, z);
                float d_w_dy = 0.0f;
                float d_v_dz = 0.0f;
                float d_u_dz = 0.0f;
                float d_w_dx = 0.0f;
                float d_v_dx = 0.0f;
                float d_u_dy = 0.0f;
                float curl_x = 0.0f;
                float curl_y = 0.0f;
                float curl_z = 0.0f;
                float magnitude = 0.0f;
                if (volume->solid_mask && volume->solid_mask[idx]) continue;
                d_w_dy = (volume->velocity_z[wind_volume_index(volume, x, y + 1, z)] -
                          volume->velocity_z[wind_volume_index(volume, x, y - 1, z)]) * inv_two_h;
                d_v_dz = (volume->velocity_y[wind_volume_index(volume, x, y, z + 1)] -
                          volume->velocity_y[wind_volume_index(volume, x, y, z - 1)]) * inv_two_h;
                d_u_dz = (volume->velocity_x[wind_volume_index(volume, x, y, z + 1)] -
                          volume->velocity_x[wind_volume_index(volume, x, y, z - 1)]) * inv_two_h;
                d_w_dx = (volume->velocity_z[wind_volume_index(volume, x + 1, y, z)] -
                          volume->velocity_z[wind_volume_index(volume, x - 1, y, z)]) * inv_two_h;
                d_v_dx = (volume->velocity_y[wind_volume_index(volume, x + 1, y, z)] -
                          volume->velocity_y[wind_volume_index(volume, x - 1, y, z)]) * inv_two_h;
                d_u_dy = (volume->velocity_x[wind_volume_index(volume, x, y + 1, z)] -
                          volume->velocity_x[wind_volume_index(volume, x, y - 1, z)]) * inv_two_h;
                curl_x = d_w_dy - d_v_dz;
                curl_y = d_u_dz - d_w_dx;
                curl_z = d_v_dx - d_u_dy;
                magnitude = sqrtf(curl_x * curl_x + curl_y * curl_y + curl_z * curl_z);
                sum += magnitude;
                if (magnitude > max_vorticity) max_vorticity = magnitude;
                ++count;
            }
        }
    }
    *out_avg = count > 0u ? (float)(sum / (double)count) : 0.0f;
    *out_max = max_vorticity;
    if (in_out_samples) *in_out_samples += count;
}

static float wind_tunnel_cross_section_area(const SceneFluidVolumeExportView3D *volume,
                                            WindTunnel3DFace face) {
    float cell_area = volume && volume->voxel_size > 0.0f
                          ? volume->voxel_size * volume->voxel_size
                          : 1.0f;
    if (!volume) return 0.0f;
    switch (face) {
    case WIND_TUNNEL_3D_FACE_LEFT:
    case WIND_TUNNEL_3D_FACE_RIGHT:
        return (float)(volume->height * volume->depth) * cell_area;
    case WIND_TUNNEL_3D_FACE_BOTTOM:
    case WIND_TUNNEL_3D_FACE_TOP:
        return (float)(volume->width * volume->depth) * cell_area;
    case WIND_TUNNEL_3D_FACE_FRONT:
    case WIND_TUNNEL_3D_FACE_BACK:
        return (float)(volume->width * volume->height) * cell_area;
    case WIND_TUNNEL_3D_FACE_NONE:
    default:
        return 0.0f;
    }
}

bool wind_tunnel_3d_analyze_volume(const SceneFluidVolumeExportView3D *volume,
                                   const WindTunnel3DConfig *config,
                                   WindTunnel3DAnalysisReport *out_report) {
    WindTunnel3DAnalysisReport report;
    size_t inlet_cells = 0u;
    size_t outlet_cells = 0u;
    if (!out_report) return false;
    memset(&report, 0, sizeof(report));
    *out_report = report;
    if (!volume || !config || !config->active || !wind_tunnel_3d_config_validate(config) ||
        volume->width <= 0 || volume->height <= 0 || volume->depth <= 0 ||
        !volume->density || !volume->velocity_x || !volume->velocity_y || !volume->velocity_z ||
        !volume->pressure) {
        return false;
    }
    if (!wind_face_stats(volume,
                         config->inlet_face,
                         config->inlet_slab_cells,
                         false,
                         &report.inlet_pressure_avg,
                         &report.inlet_throughput,
                         &inlet_cells)) {
        return false;
    }
    if (!wind_face_stats(volume,
                         config->outlet_face,
                         config->inlet_slab_cells,
                         true,
                         &report.outlet_pressure_avg,
                         &report.outlet_throughput,
                         &outlet_cells)) {
        return false;
    }
    report.pressure_delta = report.inlet_pressure_avg - report.outlet_pressure_avg;
    report.throughput_delta = report.inlet_throughput - report.outlet_throughput;
    report.drag_pressure_proxy =
        report.pressure_delta * wind_tunnel_cross_section_area(volume, config->inlet_face);
    report.sampled_cells = inlet_cells + outlet_cells;
    wind_vorticity_stats(volume, &report.vorticity_avg, &report.vorticity_max, &report.sampled_cells);
    report.valid = true;
    *out_report = report;
    return true;
}
