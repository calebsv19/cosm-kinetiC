#ifndef WIND_TUNNEL_3D_ANALYSIS_H
#define WIND_TUNNEL_3D_ANALYSIS_H

#include <stdbool.h>
#include <stddef.h>

#include "app/sim_runtime_backend.h"
#include "app/wind_tunnel_3d.h"

typedef struct WindTunnel3DAnalysisReport {
    bool valid;
    size_t sampled_cells;
    float inlet_pressure_avg;
    float outlet_pressure_avg;
    float pressure_delta;
    float inlet_throughput;
    float outlet_throughput;
    float throughput_delta;
    float drag_pressure_proxy;
    float vorticity_avg;
    float vorticity_max;
} WindTunnel3DAnalysisReport;

bool wind_tunnel_3d_analyze_volume(const SceneFluidVolumeExportView3D *volume,
                                   const WindTunnel3DConfig *config,
                                   WindTunnel3DAnalysisReport *out_report);

#endif
