#ifndef PHYSICS_SIM_SCENE_EDITOR_PANE_HOST_H
#define PHYSICS_SIM_SCENE_EDITOR_PANE_HOST_H

#include <stdbool.h>
#include <stdint.h>

#include "core_pane.h"

typedef enum SceneEditorPaneRole {
    SCENE_EDITOR_PANE_LEFT = 0,
    SCENE_EDITOR_PANE_CENTER = 1,
    SCENE_EDITOR_PANE_RIGHT = 2
} SceneEditorPaneRole;

enum {
    SCENE_EDITOR_PANE_NODE_CAPACITY = 5u,
    SCENE_EDITOR_PANE_LEAF_CAPACITY = 3u
};

typedef struct SceneEditorPaneHost {
    CorePaneNode nodes[SCENE_EDITOR_PANE_NODE_CAPACITY];
    uint32_t node_count;
    uint32_t root_index;
    CorePaneLeafRect leaves[SCENE_EDITOR_PANE_LEAF_CAPACITY];
    uint32_t leaf_count;
    float bounds_width;
    float bounds_height;
    float target_left_width;
    float target_right_width;
    bool initialized;
    char last_error[160];
} SceneEditorPaneHost;

bool scene_editor_pane_host_init(SceneEditorPaneHost *host, float width, float height);
bool scene_editor_pane_host_rebuild(SceneEditorPaneHost *host, float width, float height);
void scene_editor_pane_host_set_targets(SceneEditorPaneHost *host,
                                        float left_width,
                                        float right_width);
bool scene_editor_pane_host_get_rect_for_role(const SceneEditorPaneHost *host,
                                              SceneEditorPaneRole role,
                                              CorePaneRect *out_rect);
const char *scene_editor_pane_host_last_error(const SceneEditorPaneHost *host);

#endif
