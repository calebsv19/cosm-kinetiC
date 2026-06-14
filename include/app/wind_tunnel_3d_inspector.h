#ifndef WIND_TUNNEL_3D_INSPECTOR_H
#define WIND_TUNNEL_3D_INSPECTOR_H

#include <stdbool.h>
#include <stddef.h>

#include "app/sim_runtime_backend.h"

typedef enum WindTunnel3DInspectorFieldMode {
    WIND_TUNNEL_3D_INSPECTOR_FIELD_SPEED = 0,
    WIND_TUNNEL_3D_INSPECTOR_FIELD_PRESSURE,
    WIND_TUNNEL_3D_INSPECTOR_FIELD_VORTICITY,
    WIND_TUNNEL_3D_INSPECTOR_FIELD_DYE_DENSITY,
    WIND_TUNNEL_3D_INSPECTOR_FIELD_SOLID_MASK
} WindTunnel3DInspectorFieldMode;

typedef enum WindTunnel3DInspectorSliceAxis {
    WIND_TUNNEL_3D_INSPECTOR_AXIS_X = 0,
    WIND_TUNNEL_3D_INSPECTOR_AXIS_Y,
    WIND_TUNNEL_3D_INSPECTOR_AXIS_Z
} WindTunnel3DInspectorSliceAxis;

typedef struct WindTunnel3DInspectorState {
    WindTunnel3DInspectorFieldMode field_mode;
    WindTunnel3DInspectorSliceAxis slice_axis;
    float normalized_slice_depth;
} WindTunnel3DInspectorState;

typedef struct WindTunnel3DInspectorSnapshot {
    bool available;
    bool analysis_available;
    bool speed_available;
    bool pressure_available;
    bool vorticity_available;
    bool dye_density_available;
    bool solid_mask_available;
    WindTunnel3DInspectorFieldMode field_mode;
    WindTunnel3DInspectorSliceAxis slice_axis;
    float normalized_slice_depth;
    int slice_index;
    int domain_w;
    int domain_h;
    int domain_d;
    size_t cell_count;
    const char *field_label;
    const char *slice_axis_label;
    const char *inlet_label;
    const char *outlet_label;
    float inflow_speed;
    float inflow_density;
    float slice_min;
    float slice_max;
    float slice_avg;
    size_t slice_sampled_cells;
    size_t slice_solid_cells;
    size_t solid_cells;
    float inlet_pressure_avg;
    float outlet_pressure_avg;
    float pressure_delta;
    float inlet_throughput;
    float outlet_throughput;
    float throughput_delta;
    float drag_pressure_proxy;
    bool object_readout_available;
    size_t object_solid_cells;
    float object_projected_area;
    float object_upstream_pressure_avg;
    float object_downstream_pressure_avg;
    float object_pressure_delta;
    float object_drag_pressure_proxy;
    float vorticity_avg;
    float vorticity_max;
} WindTunnel3DInspectorSnapshot;

WindTunnel3DInspectorState wind_tunnel_3d_inspector_default_state(void);
const char *wind_tunnel_3d_inspector_field_label(WindTunnel3DInspectorFieldMode mode);
const char *wind_tunnel_3d_inspector_axis_label(WindTunnel3DInspectorSliceAxis axis);
bool wind_tunnel_3d_inspector_snapshot_from_backend(
    const SimRuntimeBackend *backend,
    const WindTunnel3DInspectorState *state,
    WindTunnel3DInspectorSnapshot *out_snapshot);

#endif // WIND_TUNNEL_3D_INSPECTOR_H
