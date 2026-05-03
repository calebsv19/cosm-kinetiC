#include "app/editor/scene_editor_pane_host.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    SCENE_EDITOR_PANE_ID_LEFT = 2101u,
    SCENE_EDITOR_PANE_ID_CENTER = 2102u,
    SCENE_EDITOR_PANE_ID_RIGHT = 2103u
};

enum {
    SCENE_EDITOR_PANE_SPLITTER_HANDLE_THICKNESS = 8u
};

static void scene_editor_pane_host_set_error(SceneEditorPaneHost *host, const char *fmt, ...) {
    va_list args;
    if (!host || !fmt) return;
    host->last_error[0] = '\0';
    va_start(args, fmt);
    (void)vsnprintf(host->last_error, sizeof(host->last_error), fmt, args);
    va_end(args);
}

static uint32_t scene_editor_pane_host_id_for_role(SceneEditorPaneRole role) {
    switch (role) {
        case SCENE_EDITOR_PANE_LEFT: return SCENE_EDITOR_PANE_ID_LEFT;
        case SCENE_EDITOR_PANE_CENTER: return SCENE_EDITOR_PANE_ID_CENTER;
        case SCENE_EDITOR_PANE_RIGHT: return SCENE_EDITOR_PANE_ID_RIGHT;
        default: return 0u;
    }
}

static CorePaneRect scene_editor_pane_host_bounds_rect(float width, float height) {
    return (CorePaneRect){ 0.0f, 0.0f, width, height };
}

static float pane_host_clamp(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool scene_editor_pane_host_find_rect_for_pane_id(const SceneEditorPaneHost *host,
                                                          uint32_t pane_id,
                                                          CorePaneRect *out_rect) {
    uint32_t i = 0u;

    if (!host || !out_rect || pane_id == 0u) return false;
    for (i = 0u; i < host->leaf_count; ++i) {
        if (host->leaves[i].id == pane_id) {
            *out_rect = host->leaves[i].rect;
            return true;
        }
    }
    return false;
}

static void scene_editor_pane_host_sync_targets_from_leaves(SceneEditorPaneHost *host) {
    CorePaneRect rect = {0};

    if (!host) return;
    if (scene_editor_pane_host_find_rect_for_pane_id(host, SCENE_EDITOR_PANE_ID_LEFT, &rect)) {
        host->target_left_width = rect.width;
    }
    if (scene_editor_pane_host_find_rect_for_pane_id(host, SCENE_EDITOR_PANE_ID_RIGHT, &rect)) {
        host->target_right_width = rect.width;
    }
}

static bool scene_editor_pane_host_solve_current(SceneEditorPaneHost *host, float width, float height) {
    CorePaneRect bounds;
    CorePaneValidationReport report;

    if (!host) return false;

    bounds = scene_editor_pane_host_bounds_rect(width, height);
    memset(&report, 0, sizeof(report));
    if (!core_pane_validate_graph(host->nodes,
                                  host->node_count,
                                  host->root_index,
                                  bounds,
                                  &report)) {
        scene_editor_pane_host_set_error(host,
                                         "pane graph invalid code=%s node=%u rel=%u",
                                         core_pane_validation_code_string(report.code),
                                         report.node_index,
                                         report.related_index);
        return false;
    }

    if (!core_pane_solve(host->nodes,
                         host->node_count,
                         host->root_index,
                         bounds,
                         host->leaves,
                         SCENE_EDITOR_PANE_LEAF_CAPACITY,
                         &host->leaf_count)) {
        scene_editor_pane_host_set_error(host, "pane solve failed");
        return false;
    }

    scene_editor_pane_host_sync_targets_from_leaves(host);
    host->bounds_width = width;
    host->bounds_height = height;
    host->initialized = true;
    host->last_error[0] = '\0';
    return true;
}

static void scene_editor_pane_host_seed_graph(SceneEditorPaneHost *host) {
    if (!host) return;

    host->node_count = 5u;
    host->root_index = 0u;

    host->nodes[0] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 1u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.22f,
        .child_a = 1u,
        .child_b = 2u,
        .constraints = { 240.0f, 620.0f }
    };
    host->nodes[1] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = SCENE_EDITOR_PANE_ID_LEFT
    };
    host->nodes[2] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 2u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.70f,
        .child_a = 3u,
        .child_b = 4u,
        .constraints = { 360.0f, 260.0f }
    };
    host->nodes[3] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = SCENE_EDITOR_PANE_ID_CENTER
    };
    host->nodes[4] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = SCENE_EDITOR_PANE_ID_RIGHT
    };
}

bool scene_editor_pane_host_rebuild(SceneEditorPaneHost *host, float width, float height) {
    float target_left = 0.0f;
    float target_right = 0.0f;
    float remaining = 0.0f;
    float max_left = 0.0f;
    float max_right = 0.0f;
    float target_center = 0.0f;

    if (!host) return false;
    if (width < 320.0f || height < 220.0f) {
        scene_editor_pane_host_set_error(host, "invalid bounds %.2fx%.2f", width, height);
        return false;
    }

    target_left = (host->target_left_width > 0.0f) ? host->target_left_width : 284.0f;
    target_right = (host->target_right_width > 0.0f) ? host->target_right_width : 284.0f;
    max_left = fmaxf(220.0f, width - (360.0f + 220.0f));
    max_right = fmaxf(220.0f, width - (360.0f + 220.0f));
    target_left = pane_host_clamp(target_left, 220.0f, max_left);
    host->nodes[0].constraints = (CorePaneConstraints){ 220.0f, 580.0f };
    host->nodes[0].ratio_01 = pane_host_clamp(target_left / width, 0.16f, 0.32f);

    remaining = width - target_left;
    if (remaining < 580.0f) remaining = 580.0f;
    target_right = pane_host_clamp(target_right, 220.0f, max_right);
    target_center = remaining - target_right;
    if (target_center < 360.0f) {
        target_center = 360.0f;
    }
    host->nodes[2].constraints = (CorePaneConstraints){ 360.0f, 220.0f };
    host->nodes[2].ratio_01 = pane_host_clamp(target_center / remaining, 0.55f, 0.82f);

    return scene_editor_pane_host_solve_current(host, width, height);
}

bool scene_editor_pane_host_init(SceneEditorPaneHost *host, float width, float height) {
    if (!host) return false;
    memset(host, 0, sizeof(*host));
    host->target_left_width = 284.0f;
    host->target_right_width = 284.0f;
    kit_pane_splitter_interaction_init(&host->splitter_interaction,
                                       (float)SCENE_EDITOR_PANE_SPLITTER_HANDLE_THICKNESS);
    scene_editor_pane_host_seed_graph(host);
    return scene_editor_pane_host_rebuild(host, width, height);
}

void scene_editor_pane_host_set_targets(SceneEditorPaneHost *host,
                                        float left_width,
                                        float right_width) {
    if (!host) return;
    if (left_width > 0.0f) host->target_left_width = left_width;
    if (right_width > 0.0f) host->target_right_width = right_width;
}

void scene_editor_pane_host_update_pointer(SceneEditorPaneHost *host,
                                           float pointer_x,
                                           float pointer_y) {
    CoreResult result;

    if (!host || !host->initialized) return;
    result = kit_pane_splitter_interaction_set_hover(&host->splitter_interaction,
                                                     host->nodes,
                                                     host->node_count,
                                                     host->root_index,
                                                     scene_editor_pane_host_bounds_rect(host->bounds_width,
                                                                                        host->bounds_height),
                                                     pointer_x,
                                                     pointer_y);
    if (result.code != CORE_OK && result.code != CORE_ERR_NOT_FOUND) {
        scene_editor_pane_host_set_error(host, "splitter hover failed (%d)", (int)result.code);
    }
}

bool scene_editor_pane_host_begin_splitter_drag(SceneEditorPaneHost *host,
                                                float pointer_x,
                                                float pointer_y) {
    CoreResult result;

    if (!host || !host->initialized) return false;
    result = kit_pane_splitter_interaction_begin_drag(&host->splitter_interaction,
                                                      host->nodes,
                                                      host->node_count,
                                                      host->root_index,
                                                      scene_editor_pane_host_bounds_rect(host->bounds_width,
                                                                                         host->bounds_height),
                                                      pointer_x,
                                                      pointer_y);
    return result.code == CORE_OK;
}

bool scene_editor_pane_host_update_splitter_drag(SceneEditorPaneHost *host,
                                                 float pointer_x,
                                                 float pointer_y) {
    CoreResult result;
    int changed = 0;

    if (!host || !host->initialized || !host->splitter_interaction.drag_active) return false;
    result = kit_pane_splitter_interaction_update_drag(&host->splitter_interaction,
                                                       host->nodes,
                                                       host->node_count,
                                                       pointer_x,
                                                       pointer_y,
                                                       &changed);
    if (result.code != CORE_OK) {
        scene_editor_pane_host_set_error(host, "splitter drag update failed (%d)", (int)result.code);
        return false;
    }
    if (changed) {
        if (!scene_editor_pane_host_solve_current(host, host->bounds_width, host->bounds_height)) {
            return false;
        }
    }
    return changed != 0;
}

void scene_editor_pane_host_end_splitter_drag(SceneEditorPaneHost *host) {
    if (!host) return;
    kit_pane_splitter_interaction_end_drag(&host->splitter_interaction);
}

bool scene_editor_pane_host_splitter_drag_active(const SceneEditorPaneHost *host) {
    if (!host) return false;
    return host->splitter_interaction.drag_active != 0;
}

bool scene_editor_pane_host_visible_splitter(const SceneEditorPaneHost *host,
                                             CorePaneRect *out_rect,
                                             bool *out_hovered,
                                             bool *out_active) {
    int hovered = 0;
    int active = 0;

    if (out_hovered) *out_hovered = false;
    if (out_active) *out_active = false;
    if (!host) return false;
    if (!kit_pane_splitter_interaction_current(&host->splitter_interaction,
                                               out_rect,
                                               &hovered,
                                               &active)) {
        return false;
    }
    if (out_hovered) *out_hovered = hovered != 0;
    if (out_active) *out_active = active != 0;
    return true;
}

bool scene_editor_pane_host_get_rect_for_role(const SceneEditorPaneHost *host,
                                              SceneEditorPaneRole role,
                                              CorePaneRect *out_rect) {
    uint32_t pane_id = 0u;

    if (!host || !out_rect || !host->initialized) return false;
    pane_id = scene_editor_pane_host_id_for_role(role);
    return scene_editor_pane_host_find_rect_for_pane_id(host, pane_id, out_rect);
}

const char *scene_editor_pane_host_last_error(const SceneEditorPaneHost *host) {
    if (!host || host->last_error[0] == '\0') return "";
    return host->last_error;
}
