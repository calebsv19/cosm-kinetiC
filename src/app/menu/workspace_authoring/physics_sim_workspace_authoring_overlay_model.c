#include "app/menu/workspace_authoring/physics_sim_workspace_authoring_overlay_model.h"

typedef struct PhysicsSimWorkspaceAuthoringPaneDef {
    SceneEditorPaneRole role;
    const char *pane_label;
    const char *module_key;
    const char *module_label;
} PhysicsSimWorkspaceAuthoringPaneDef;

static const PhysicsSimWorkspaceAuthoringPaneDef k_physics_sim_authoring_panes[] = {
    { SCENE_EDITOR_PANE_LEFT, "P1 Preset Library", "physics.presets", "Presets" },
    { SCENE_EDITOR_PANE_CENTER, "P2 Simulation Settings", "physics.settings", "Settings" },
    { SCENE_EDITOR_PANE_RIGHT, "P3 Data I/O + Launch", "physics.io", "I/O + Launch" }
};

uint32_t physics_sim_workspace_authoring_overlay_build_pane_rows(
    int width,
    int height,
    PhysicsSimWorkspaceAuthoringPaneRow *out_rows,
    uint32_t cap) {
    SceneEditorPaneHost host;
    uint32_t count = 0u;
    uint32_t i = 0u;
    if (!out_rows || cap == 0u || width <= 0 || height <= 0) return 0u;
    if (!scene_editor_pane_host_init(&host, (float)width, (float)height)) return 0u;

    for (i = 0u;
         i < (uint32_t)(sizeof(k_physics_sim_authoring_panes) /
                        sizeof(k_physics_sim_authoring_panes[0])) &&
         count < cap;
         ++i) {
        CorePaneRect rect = {0};
        const PhysicsSimWorkspaceAuthoringPaneDef *def = &k_physics_sim_authoring_panes[i];
        if (!scene_editor_pane_host_get_rect_for_role(&host, def->role, &rect)) {
            continue;
        }
        out_rows[count].pane_rect = rect;
        out_rows[count].role = def->role;
        out_rows[count].pane_label = def->pane_label;
        out_rows[count].module_key = def->module_key;
        out_rows[count].module_label = def->module_label;
        ++count;
    }
    return count;
}
