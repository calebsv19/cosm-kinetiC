#include "app/wind_tunnel_3d.h"

#include <math.h>
#include <string.h>

static float positive_or(float value, float fallback) {
    return (isfinite(value) && value > 0.0f) ? value : fallback;
}

static float non_negative_or(float value, float fallback) {
    return (isfinite(value) && value >= 0.0f) ? value : fallback;
}

bool wind_tunnel_3d_route_active(SimulationMode mode, SpaceMode space_mode) {
    return mode == SIM_MODE_WIND_TUNNEL && space_mode == SPACE_MODE_3D;
}

WindTunnel3DConfig wind_tunnel_3d_config_default(const AppConfig *cfg) {
    WindTunnel3DConfig config;
    memset(&config, 0, sizeof(config));
    config.active = true;
    config.inlet_face = WIND_TUNNEL_3D_FACE_LEFT;
    config.outlet_face = WIND_TUNNEL_3D_FACE_RIGHT;
    config.inflow_speed = positive_or(cfg ? cfg->tunnel_inflow_speed : 0.0f, 35.0f);
    config.inflow_density = non_negative_or(cfg ? cfg->tunnel_inflow_density : 0.0f, 1.0f);
    config.inlet_slab_cells = 2;
    config.outlet_policy = WIND_TUNNEL_3D_OUTLET_RECEIVE;
    config.wall_policy = WIND_TUNNEL_3D_WALL_NO_SLIP;
    config.probe_count = 0;
    return config;
}

const char *wind_tunnel_3d_face_label(WindTunnel3DFace face) {
    switch (face) {
    case WIND_TUNNEL_3D_FACE_LEFT: return "left";
    case WIND_TUNNEL_3D_FACE_RIGHT: return "right";
    case WIND_TUNNEL_3D_FACE_TOP: return "top";
    case WIND_TUNNEL_3D_FACE_BOTTOM: return "bottom";
    case WIND_TUNNEL_3D_FACE_FRONT: return "front";
    case WIND_TUNNEL_3D_FACE_BACK: return "back";
    case WIND_TUNNEL_3D_FACE_NONE:
    default:
        return "none";
    }
}

bool wind_tunnel_3d_face_from_label(const char *label, WindTunnel3DFace *out_face) {
    if (!label || !out_face) return false;
    if (strcmp(label, "Left") == 0 || strcmp(label, "left") == 0) {
        *out_face = WIND_TUNNEL_3D_FACE_LEFT;
        return true;
    }
    if (strcmp(label, "Right") == 0 || strcmp(label, "right") == 0) {
        *out_face = WIND_TUNNEL_3D_FACE_RIGHT;
        return true;
    }
    if (strcmp(label, "Top") == 0 || strcmp(label, "top") == 0) {
        *out_face = WIND_TUNNEL_3D_FACE_TOP;
        return true;
    }
    if (strcmp(label, "Bottom") == 0 || strcmp(label, "bottom") == 0) {
        *out_face = WIND_TUNNEL_3D_FACE_BOTTOM;
        return true;
    }
    if (strcmp(label, "Front") == 0 || strcmp(label, "front") == 0) {
        *out_face = WIND_TUNNEL_3D_FACE_FRONT;
        return true;
    }
    if (strcmp(label, "Back") == 0 || strcmp(label, "back") == 0) {
        *out_face = WIND_TUNNEL_3D_FACE_BACK;
        return true;
    }
    return false;
}

bool wind_tunnel_3d_outlet_policy_from_label(const char *label,
                                             WindTunnel3DOutletPolicy *out_policy) {
    if (!label || !out_policy) return false;
    if (strcmp(label, "Receive") == 0 || strcmp(label, "receive") == 0) {
        *out_policy = WIND_TUNNEL_3D_OUTLET_RECEIVE;
        return true;
    }
    if (strcmp(label, "Clear") == 0 || strcmp(label, "clear") == 0) {
        *out_policy = WIND_TUNNEL_3D_OUTLET_CLEAR;
        return true;
    }
    return false;
}

bool wind_tunnel_3d_wall_policy_from_label(const char *label,
                                           WindTunnel3DWallPolicy *out_policy) {
    if (!label || !out_policy) return false;
    if (strcmp(label, "NoSlip") == 0 || strcmp(label, "no_slip") == 0) {
        *out_policy = WIND_TUNNEL_3D_WALL_NO_SLIP;
        return true;
    }
    if (strcmp(label, "Open") == 0 || strcmp(label, "open") == 0) {
        *out_policy = WIND_TUNNEL_3D_WALL_OPEN;
        return true;
    }
    if (strcmp(label, "Slip") == 0 || strcmp(label, "slip") == 0) {
        *out_policy = WIND_TUNNEL_3D_WALL_SLIP;
        return true;
    }
    return false;
}

static bool wind_tunnel_3d_face_valid(WindTunnel3DFace face) {
    return face >= WIND_TUNNEL_3D_FACE_LEFT && face <= WIND_TUNNEL_3D_FACE_BACK;
}

static bool wind_tunnel_3d_probe_axis_valid(char axis) {
    return axis == 'x' || axis == 'X' ||
           axis == 'y' || axis == 'Y' ||
           axis == 'z' || axis == 'Z';
}

bool wind_tunnel_3d_config_validate(const WindTunnel3DConfig *config) {
    if (!config || !config->active) return false;
    if (!wind_tunnel_3d_face_valid(config->inlet_face)) return false;
    if (!wind_tunnel_3d_face_valid(config->outlet_face)) return false;
    if (config->inlet_face == config->outlet_face) return false;
    if (!isfinite(config->inflow_speed) || config->inflow_speed <= 0.0f) return false;
    if (!isfinite(config->inflow_density) || config->inflow_density < 0.0f) return false;
    if (config->inlet_slab_cells <= 0) return false;
    if (config->probe_count < 0 || config->probe_count > WIND_TUNNEL_3D_MAX_PROBE_PLANES) return false;
    for (int i = 0; i < config->probe_count; ++i) {
        const WindTunnel3DProbePlane *probe = &config->probes[i];
        if (!probe->active) continue;
        if (!wind_tunnel_3d_probe_axis_valid(probe->axis)) return false;
        if (!isfinite(probe->position)) return false;
    }
    return true;
}
