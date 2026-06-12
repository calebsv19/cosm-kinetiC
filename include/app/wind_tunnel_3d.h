#ifndef WIND_TUNNEL_3D_H
#define WIND_TUNNEL_3D_H

#include <stdbool.h>

#include "app/app_config.h"

#define WIND_TUNNEL_3D_MAX_PROBE_PLANES 4

typedef enum WindTunnel3DFace {
    WIND_TUNNEL_3D_FACE_NONE = 0,
    WIND_TUNNEL_3D_FACE_LEFT,
    WIND_TUNNEL_3D_FACE_RIGHT,
    WIND_TUNNEL_3D_FACE_TOP,
    WIND_TUNNEL_3D_FACE_BOTTOM,
    WIND_TUNNEL_3D_FACE_FRONT,
    WIND_TUNNEL_3D_FACE_BACK
} WindTunnel3DFace;

typedef enum WindTunnel3DOutletPolicy {
    WIND_TUNNEL_3D_OUTLET_RECEIVE = 0,
    WIND_TUNNEL_3D_OUTLET_CLEAR
} WindTunnel3DOutletPolicy;

typedef enum WindTunnel3DWallPolicy {
    WIND_TUNNEL_3D_WALL_NO_SLIP = 0,
    WIND_TUNNEL_3D_WALL_OPEN,
    WIND_TUNNEL_3D_WALL_SLIP
} WindTunnel3DWallPolicy;

typedef struct WindTunnel3DProbePlane {
    bool active;
    char id[32];
    char axis;
    float position;
} WindTunnel3DProbePlane;

typedef struct WindTunnel3DConfig {
    bool active;
    WindTunnel3DFace inlet_face;
    WindTunnel3DFace outlet_face;
    float inflow_speed;
    float inflow_density;
    int inlet_slab_cells;
    WindTunnel3DOutletPolicy outlet_policy;
    WindTunnel3DWallPolicy wall_policy;
    int probe_count;
    WindTunnel3DProbePlane probes[WIND_TUNNEL_3D_MAX_PROBE_PLANES];
} WindTunnel3DConfig;

bool wind_tunnel_3d_route_active(SimulationMode mode, SpaceMode space_mode);
WindTunnel3DConfig wind_tunnel_3d_config_default(const AppConfig *cfg);
bool wind_tunnel_3d_config_validate(const WindTunnel3DConfig *config);
const char *wind_tunnel_3d_face_label(WindTunnel3DFace face);
bool wind_tunnel_3d_face_from_label(const char *label, WindTunnel3DFace *out_face);
bool wind_tunnel_3d_outlet_policy_from_label(const char *label,
                                             WindTunnel3DOutletPolicy *out_policy);
bool wind_tunnel_3d_wall_policy_from_label(const char *label,
                                           WindTunnel3DWallPolicy *out_policy);

#endif // WIND_TUNNEL_3D_H
