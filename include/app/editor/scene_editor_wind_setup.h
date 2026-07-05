#ifndef PHYSICS_SIM_SCENE_EDITOR_WIND_SETUP_H
#define PHYSICS_SIM_SCENE_EDITOR_WIND_SETUP_H

#include <stdbool.h>

#include "app/app_config.h"
#include "app/editor/scene_editor_session.h"
#include "app/wind_tunnel_3d.h"

typedef struct SceneEditorWindSetupSummary {
    bool active;
    bool wind_route_active;
    bool retained_scene_active;
    bool authored_config;
    WindTunnel3DConfig config;
    const char *inlet_label;
    const char *outlet_label;
    const char *outlet_policy_label;
    const char *wall_policy_label;
    const char *source_label;
} SceneEditorWindSetupSummary;

SceneEditorWindSetupSummary scene_editor_wind_setup_summary(const AppConfig *cfg,
                                                            const PhysicsSimEditorSession *session);
WindTunnel3DFace scene_editor_wind_setup_opposite_face(WindTunnel3DFace face);

#endif
