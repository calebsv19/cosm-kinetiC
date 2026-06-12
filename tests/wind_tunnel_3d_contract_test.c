#include "app/wind_tunnel_3d.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool nearly_equal(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static bool test_route_activation_is_wind_3d_only(void) {
    if (!wind_tunnel_3d_route_active(SIM_MODE_WIND_TUNNEL, SPACE_MODE_3D)) return false;
    if (wind_tunnel_3d_route_active(SIM_MODE_WIND_TUNNEL, SPACE_MODE_2D)) return false;
    if (wind_tunnel_3d_route_active(SIM_MODE_BOX, SPACE_MODE_3D)) return false;
    if (wind_tunnel_3d_route_active(SIM_MODE_ATMOSPHERIC, SPACE_MODE_3D)) return false;
    return true;
}

static bool test_default_config_uses_app_inflow_fields(void) {
    AppConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.tunnel_inflow_speed = 42.0f;
    cfg.tunnel_inflow_density = 0.65f;

    WindTunnel3DConfig config = wind_tunnel_3d_config_default(&cfg);
    if (!config.active) return false;
    if (config.inlet_face != WIND_TUNNEL_3D_FACE_LEFT) return false;
    if (config.outlet_face != WIND_TUNNEL_3D_FACE_RIGHT) return false;
    if (!nearly_equal(config.inflow_speed, 42.0f)) return false;
    if (!nearly_equal(config.inflow_density, 0.65f)) return false;
    if (config.inlet_slab_cells != 2) return false;
    if (config.outlet_policy != WIND_TUNNEL_3D_OUTLET_RECEIVE) return false;
    if (config.wall_policy != WIND_TUNNEL_3D_WALL_NO_SLIP) return false;
    if (config.probe_count != 0) return false;
    return wind_tunnel_3d_config_validate(&config);
}

static bool test_default_config_falls_back_from_invalid_values(void) {
    AppConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.tunnel_inflow_speed = -1.0f;
    cfg.tunnel_inflow_density = -2.0f;

    WindTunnel3DConfig config = wind_tunnel_3d_config_default(&cfg);
    if (!nearly_equal(config.inflow_speed, 35.0f)) return false;
    if (!nearly_equal(config.inflow_density, 1.0f)) return false;
    return wind_tunnel_3d_config_validate(&config);
}

static bool test_validation_rejects_bad_contracts(void) {
    WindTunnel3DConfig config = wind_tunnel_3d_config_default(NULL);
    config.outlet_face = config.inlet_face;
    if (wind_tunnel_3d_config_validate(&config)) return false;

    config = wind_tunnel_3d_config_default(NULL);
    config.inflow_speed = 0.0f;
    if (wind_tunnel_3d_config_validate(&config)) return false;

    config = wind_tunnel_3d_config_default(NULL);
    config.inflow_density = -0.1f;
    if (wind_tunnel_3d_config_validate(&config)) return false;

    config = wind_tunnel_3d_config_default(NULL);
    config.inlet_slab_cells = 0;
    if (wind_tunnel_3d_config_validate(&config)) return false;

    config = wind_tunnel_3d_config_default(NULL);
    config.probe_count = WIND_TUNNEL_3D_MAX_PROBE_PLANES + 1;
    if (wind_tunnel_3d_config_validate(&config)) return false;

    config = wind_tunnel_3d_config_default(NULL);
    config.probe_count = 1;
    config.probes[0].active = true;
    config.probes[0].axis = 'q';
    config.probes[0].position = 0.5f;
    if (wind_tunnel_3d_config_validate(&config)) return false;

    config = wind_tunnel_3d_config_default(NULL);
    config.probe_count = 1;
    config.probes[0].active = true;
    config.probes[0].axis = 'z';
    config.probes[0].position = 0.5f;
    if (!wind_tunnel_3d_config_validate(&config)) return false;
    return true;
}

static bool test_face_labels_are_stable(void) {
    return strcmp(wind_tunnel_3d_face_label(WIND_TUNNEL_3D_FACE_LEFT), "left") == 0 &&
           strcmp(wind_tunnel_3d_face_label(WIND_TUNNEL_3D_FACE_BACK), "back") == 0 &&
           strcmp(wind_tunnel_3d_face_label(WIND_TUNNEL_3D_FACE_NONE), "none") == 0;
}

int main(void) {
    if (!test_route_activation_is_wind_3d_only()) {
        fprintf(stderr, "wind_tunnel_3d_contract_test: route activation failed\n");
        return 1;
    }
    if (!test_default_config_uses_app_inflow_fields()) {
        fprintf(stderr, "wind_tunnel_3d_contract_test: default config failed\n");
        return 1;
    }
    if (!test_default_config_falls_back_from_invalid_values()) {
        fprintf(stderr, "wind_tunnel_3d_contract_test: invalid value fallback failed\n");
        return 1;
    }
    if (!test_validation_rejects_bad_contracts()) {
        fprintf(stderr, "wind_tunnel_3d_contract_test: validation failed\n");
        return 1;
    }
    if (!test_face_labels_are_stable()) {
        fprintf(stderr, "wind_tunnel_3d_contract_test: face labels failed\n");
        return 1;
    }
    fprintf(stdout, "wind_tunnel_3d_contract_test: success\n");
    return 0;
}
