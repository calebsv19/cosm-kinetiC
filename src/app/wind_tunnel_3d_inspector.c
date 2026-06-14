#include "app/wind_tunnel_3d_inspector.h"

#include <math.h>
#include <string.h>

static size_t wind_inspector_index(const SceneFluidVolumeExportView3D *volume,
                                   int x,
                                   int y,
                                   int z) {
    return ((size_t)z * (size_t)volume->height + (size_t)y) * (size_t)volume->width + (size_t)x;
}

static int wind_inspector_axis_extent(const SceneFluidVolumeExportView3D *volume,
                                      WindTunnel3DInspectorSliceAxis axis) {
    if (!volume) return 0;
    switch (axis) {
    case WIND_TUNNEL_3D_INSPECTOR_AXIS_X:
        return volume->width;
    case WIND_TUNNEL_3D_INSPECTOR_AXIS_Y:
        return volume->height;
    case WIND_TUNNEL_3D_INSPECTOR_AXIS_Z:
    default:
        return volume->depth;
    }
}

static int wind_inspector_clamped_slice(const SceneFluidVolumeExportView3D *volume,
                                        WindTunnel3DInspectorSliceAxis axis,
                                        float normalized_depth) {
    int extent = wind_inspector_axis_extent(volume, axis);
    float t = normalized_depth;
    int index = 0;
    if (extent <= 1) return 0;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    index = (int)lroundf(t * (float)(extent - 1));
    if (index < 0) index = 0;
    if (index >= extent) index = extent - 1;
    return index;
}

static bool wind_inspector_cell_on_slice(WindTunnel3DInspectorSliceAxis axis,
                                         int slice_index,
                                         int x,
                                         int y,
                                         int z) {
    switch (axis) {
    case WIND_TUNNEL_3D_INSPECTOR_AXIS_X:
        return x == slice_index;
    case WIND_TUNNEL_3D_INSPECTOR_AXIS_Y:
        return y == slice_index;
    case WIND_TUNNEL_3D_INSPECTOR_AXIS_Z:
    default:
        return z == slice_index;
    }
}

static float wind_inspector_vorticity_at(const SceneFluidVolumeExportView3D *volume,
                                         int x,
                                         int y,
                                         int z) {
    float inv_two_h = 0.5f;
    float d_w_dy = 0.0f;
    float d_v_dz = 0.0f;
    float d_u_dz = 0.0f;
    float d_w_dx = 0.0f;
    float d_v_dx = 0.0f;
    float d_u_dy = 0.0f;
    float curl_x = 0.0f;
    float curl_y = 0.0f;
    float curl_z = 0.0f;
    if (!volume || x <= 0 || y <= 0 || z <= 0 ||
        x >= volume->width - 1 || y >= volume->height - 1 || z >= volume->depth - 1 ||
        !volume->velocity_x || !volume->velocity_y || !volume->velocity_z) {
        return 0.0f;
    }
    if (volume->voxel_size > 0.0f) inv_two_h = 0.5f / volume->voxel_size;
    d_w_dy = (volume->velocity_z[wind_inspector_index(volume, x, y + 1, z)] -
              volume->velocity_z[wind_inspector_index(volume, x, y - 1, z)]) * inv_two_h;
    d_v_dz = (volume->velocity_y[wind_inspector_index(volume, x, y, z + 1)] -
              volume->velocity_y[wind_inspector_index(volume, x, y, z - 1)]) * inv_two_h;
    d_u_dz = (volume->velocity_x[wind_inspector_index(volume, x, y, z + 1)] -
              volume->velocity_x[wind_inspector_index(volume, x, y, z - 1)]) * inv_two_h;
    d_w_dx = (volume->velocity_z[wind_inspector_index(volume, x + 1, y, z)] -
              volume->velocity_z[wind_inspector_index(volume, x - 1, y, z)]) * inv_two_h;
    d_v_dx = (volume->velocity_y[wind_inspector_index(volume, x + 1, y, z)] -
              volume->velocity_y[wind_inspector_index(volume, x - 1, y, z)]) * inv_two_h;
    d_u_dy = (volume->velocity_x[wind_inspector_index(volume, x, y + 1, z)] -
              volume->velocity_x[wind_inspector_index(volume, x, y - 1, z)]) * inv_two_h;
    curl_x = d_w_dy - d_v_dz;
    curl_y = d_u_dz - d_w_dx;
    curl_z = d_v_dx - d_u_dy;
    return sqrtf(curl_x * curl_x + curl_y * curl_y + curl_z * curl_z);
}

static bool wind_inspector_sample_value(const SceneFluidVolumeExportView3D *volume,
                                        WindTunnel3DInspectorFieldMode mode,
                                        size_t idx,
                                        int x,
                                        int y,
                                        int z,
                                        float *out_value) {
    if (!volume || !out_value) return false;
    switch (mode) {
    case WIND_TUNNEL_3D_INSPECTOR_FIELD_SPEED:
        if (!volume->velocity_x || !volume->velocity_y || !volume->velocity_z) return false;
        *out_value = sqrtf(volume->velocity_x[idx] * volume->velocity_x[idx] +
                           volume->velocity_y[idx] * volume->velocity_y[idx] +
                           volume->velocity_z[idx] * volume->velocity_z[idx]);
        return true;
    case WIND_TUNNEL_3D_INSPECTOR_FIELD_PRESSURE:
        if (!volume->pressure) return false;
        *out_value = volume->pressure[idx];
        return true;
    case WIND_TUNNEL_3D_INSPECTOR_FIELD_VORTICITY:
        if (!volume->velocity_x || !volume->velocity_y || !volume->velocity_z) return false;
        *out_value = wind_inspector_vorticity_at(volume, x, y, z);
        return true;
    case WIND_TUNNEL_3D_INSPECTOR_FIELD_DYE_DENSITY:
        if (!volume->density) return false;
        *out_value = volume->density[idx];
        return true;
    case WIND_TUNNEL_3D_INSPECTOR_FIELD_SOLID_MASK:
        if (!volume->solid_mask) return false;
        *out_value = volume->solid_mask[idx] ? 1.0f : 0.0f;
        return true;
    default:
        return false;
    }
}

static void wind_inspector_sample_slice(const SceneFluidVolumeExportView3D *volume,
                                        WindTunnel3DInspectorFieldMode mode,
                                        WindTunnel3DInspectorSliceAxis axis,
                                        int slice_index,
                                        WindTunnel3DInspectorSnapshot *snapshot) {
    double sum = 0.0;
    bool have_value = false;
    if (!volume || !snapshot) return;
    for (int z = 0; z < volume->depth; ++z) {
        for (int y = 0; y < volume->height; ++y) {
            for (int x = 0; x < volume->width; ++x) {
                size_t idx = 0u;
                float value = 0.0f;
                if (!wind_inspector_cell_on_slice(axis, slice_index, x, y, z)) continue;
                idx = wind_inspector_index(volume, x, y, z);
                if (volume->solid_mask && volume->solid_mask[idx]) {
                    snapshot->slice_solid_cells++;
                }
                if (!wind_inspector_sample_value(volume, mode, idx, x, y, z, &value)) continue;
                if (!have_value) {
                    snapshot->slice_min = value;
                    snapshot->slice_max = value;
                    have_value = true;
                }
                if (value < snapshot->slice_min) snapshot->slice_min = value;
                if (value > snapshot->slice_max) snapshot->slice_max = value;
                sum += value;
                snapshot->slice_sampled_cells++;
            }
        }
    }
    snapshot->slice_avg = snapshot->slice_sampled_cells > 0u
                              ? (float)(sum / (double)snapshot->slice_sampled_cells)
                              : 0.0f;
}

WindTunnel3DInspectorState wind_tunnel_3d_inspector_default_state(void) {
    return (WindTunnel3DInspectorState){
        .field_mode = WIND_TUNNEL_3D_INSPECTOR_FIELD_SPEED,
        .slice_axis = WIND_TUNNEL_3D_INSPECTOR_AXIS_Z,
        .normalized_slice_depth = 0.5f,
    };
}

const char *wind_tunnel_3d_inspector_field_label(WindTunnel3DInspectorFieldMode mode) {
    switch (mode) {
    case WIND_TUNNEL_3D_INSPECTOR_FIELD_SPEED:
        return "Speed";
    case WIND_TUNNEL_3D_INSPECTOR_FIELD_PRESSURE:
        return "Pressure";
    case WIND_TUNNEL_3D_INSPECTOR_FIELD_VORTICITY:
        return "Vorticity";
    case WIND_TUNNEL_3D_INSPECTOR_FIELD_DYE_DENSITY:
        return "Dye";
    case WIND_TUNNEL_3D_INSPECTOR_FIELD_SOLID_MASK:
        return "Solid mask";
    default:
        return "Unknown";
    }
}

const char *wind_tunnel_3d_inspector_axis_label(WindTunnel3DInspectorSliceAxis axis) {
    switch (axis) {
    case WIND_TUNNEL_3D_INSPECTOR_AXIS_X:
        return "X";
    case WIND_TUNNEL_3D_INSPECTOR_AXIS_Y:
        return "Y";
    case WIND_TUNNEL_3D_INSPECTOR_AXIS_Z:
    default:
        return "Z";
    }
}

bool wind_tunnel_3d_inspector_snapshot_from_backend(
    const SimRuntimeBackend *backend,
    const WindTunnel3DInspectorState *state,
    WindTunnel3DInspectorSnapshot *out_snapshot) {
    WindTunnel3DInspectorState fallback_state = wind_tunnel_3d_inspector_default_state();
    WindTunnel3DInspectorSnapshot snapshot = {0};
    SceneFluidVolumeExportView3D volume = {0};
    SimRuntimeBackendReport report = {0};
    if (!out_snapshot) return false;
    *out_snapshot = snapshot;
    if (!state) state = &fallback_state;
    if (!sim_runtime_backend_get_report(backend, &report) ||
        !report.wind_tunnel_active ||
        !sim_runtime_backend_get_volume_export_view_3d(backend, &volume) ||
        volume.width <= 0 || volume.height <= 0 || volume.depth <= 0) {
        return false;
    }

    snapshot.available = true;
    snapshot.analysis_available = report.wind_analysis_available;
    snapshot.speed_available = volume.velocity_x && volume.velocity_y && volume.velocity_z;
    snapshot.pressure_available = volume.pressure != NULL;
    snapshot.vorticity_available = snapshot.speed_available &&
                                   volume.width > 2 && volume.height > 2 && volume.depth > 2;
    snapshot.dye_density_available = volume.density != NULL;
    snapshot.solid_mask_available = volume.solid_mask != NULL;
    snapshot.field_mode = state->field_mode;
    snapshot.slice_axis = state->slice_axis;
    snapshot.normalized_slice_depth = state->normalized_slice_depth;
    snapshot.slice_index = wind_inspector_clamped_slice(&volume,
                                                        snapshot.slice_axis,
                                                        snapshot.normalized_slice_depth);
    snapshot.domain_w = volume.width;
    snapshot.domain_h = volume.height;
    snapshot.domain_d = volume.depth;
    snapshot.cell_count = volume.cell_count;
    snapshot.field_label = wind_tunnel_3d_inspector_field_label(snapshot.field_mode);
    snapshot.slice_axis_label = wind_tunnel_3d_inspector_axis_label(snapshot.slice_axis);
    snapshot.inlet_label = wind_tunnel_3d_face_label(report.wind_tunnel_inlet_face);
    snapshot.outlet_label = wind_tunnel_3d_face_label(report.wind_tunnel_outlet_face);
    snapshot.inflow_speed = report.wind_tunnel_inflow_speed;
    snapshot.inflow_density = report.wind_tunnel_inflow_density;
    snapshot.solid_cells = report.debug_volume_solid_cells;
    snapshot.inlet_pressure_avg = report.wind_analysis_inlet_pressure_avg;
    snapshot.outlet_pressure_avg = report.wind_analysis_outlet_pressure_avg;
    snapshot.pressure_delta = report.wind_analysis_pressure_delta;
    snapshot.inlet_throughput = report.wind_analysis_inlet_throughput;
    snapshot.outlet_throughput = report.wind_analysis_outlet_throughput;
    snapshot.throughput_delta = report.wind_analysis_throughput_delta;
    snapshot.drag_pressure_proxy = report.wind_analysis_drag_pressure_proxy;
    snapshot.object_readout_available = report.wind_analysis_object_drag_available;
    snapshot.object_solid_cells = report.wind_analysis_object_solid_cells;
    snapshot.object_projected_area = report.wind_analysis_object_projected_area;
    snapshot.object_upstream_pressure_avg = report.wind_analysis_object_upstream_pressure_avg;
    snapshot.object_downstream_pressure_avg = report.wind_analysis_object_downstream_pressure_avg;
    snapshot.object_pressure_delta = report.wind_analysis_object_pressure_delta;
    snapshot.object_drag_pressure_proxy = report.wind_analysis_object_drag_pressure_proxy;
    snapshot.vorticity_avg = report.wind_analysis_vorticity_avg;
    snapshot.vorticity_max = report.wind_analysis_vorticity_max;
    wind_inspector_sample_slice(&volume,
                                snapshot.field_mode,
                                snapshot.slice_axis,
                                snapshot.slice_index,
                                &snapshot);
    *out_snapshot = snapshot;
    return true;
}
