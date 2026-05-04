#ifndef PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_MODEL_H
#define PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_MODEL_H

#include <stdint.h>

#include "app/editor/scene_editor_pane_host.h"
#include "core_pane.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PhysicsSimWorkspaceAuthoringPaneRow {
    CorePaneRect pane_rect;
    SceneEditorPaneRole role;
    const char *pane_label;
    const char *module_key;
    const char *module_label;
} PhysicsSimWorkspaceAuthoringPaneRow;

uint32_t physics_sim_workspace_authoring_overlay_build_pane_rows(
    int width,
    int height,
    PhysicsSimWorkspaceAuthoringPaneRow *out_rows,
    uint32_t cap);

#ifdef __cplusplus
}
#endif

#endif
