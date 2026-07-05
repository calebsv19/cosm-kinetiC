#include "app/editor/scene_editor_wind_setup.h"

#include <string.h>

static const char *scene_editor_wind_outlet_policy_label(WindTunnel3DOutletPolicy policy) {
    switch (policy) {
        case WIND_TUNNEL_3D_OUTLET_CLEAR: return "clear";
        case WIND_TUNNEL_3D_OUTLET_RECEIVE:
        default: return "receive";
    }
}

static const char *scene_editor_wind_wall_policy_label(WindTunnel3DWallPolicy policy) {
    switch (policy) {
        case WIND_TUNNEL_3D_WALL_OPEN: return "open";
        case WIND_TUNNEL_3D_WALL_SLIP: return "slip";
        case WIND_TUNNEL_3D_WALL_NO_SLIP:
        default: return "no slip";
    }
}

WindTunnel3DFace scene_editor_wind_setup_opposite_face(WindTunnel3DFace face) {
    switch (face) {
        case WIND_TUNNEL_3D_FACE_LEFT: return WIND_TUNNEL_3D_FACE_RIGHT;
        case WIND_TUNNEL_3D_FACE_RIGHT: return WIND_TUNNEL_3D_FACE_LEFT;
        case WIND_TUNNEL_3D_FACE_TOP: return WIND_TUNNEL_3D_FACE_BOTTOM;
        case WIND_TUNNEL_3D_FACE_BOTTOM: return WIND_TUNNEL_3D_FACE_TOP;
        case WIND_TUNNEL_3D_FACE_FRONT: return WIND_TUNNEL_3D_FACE_BACK;
        case WIND_TUNNEL_3D_FACE_BACK: return WIND_TUNNEL_3D_FACE_FRONT;
        case WIND_TUNNEL_3D_FACE_NONE:
        default: return WIND_TUNNEL_3D_FACE_NONE;
    }
}

SceneEditorWindSetupSummary scene_editor_wind_setup_summary(const AppConfig *cfg,
                                                            const PhysicsSimEditorSession *session) {
    SceneEditorWindSetupSummary summary;
    const WindTunnel3DConfig *authored = NULL;
    memset(&summary, 0, sizeof(summary));

    summary.wind_route_active = cfg && wind_tunnel_3d_route_active(cfg->sim_mode, cfg->space_mode);
    summary.retained_scene_active = session && session->has_retained_scene;
    summary.active = summary.wind_route_active && summary.retained_scene_active;
    summary.config = wind_tunnel_3d_config_default(cfg);
    summary.source_label = "menu defaults";

    authored = (session && session->has_retained_scene && session->has_wind_tunnel_config)
                   ? &session->wind_tunnel
                   : NULL;
    if (authored && wind_tunnel_3d_config_validate(authored)) {
        summary.config = *authored;
        summary.authored_config = session->wind_tunnel_authored;
        summary.source_label = summary.authored_config ? "scene extension" : "session config";
    }

    summary.inlet_label = wind_tunnel_3d_face_label(summary.config.inlet_face);
    summary.outlet_label = wind_tunnel_3d_face_label(summary.config.outlet_face);
    summary.outlet_policy_label = scene_editor_wind_outlet_policy_label(summary.config.outlet_policy);
    summary.wall_policy_label = scene_editor_wind_wall_policy_label(summary.config.wall_policy);
    return summary;
}
