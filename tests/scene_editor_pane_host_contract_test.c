#include "app/editor/scene_editor_pane_host.h"

#include <assert.h>

static void test_pane_host_solves_left_center_right_shell(void) {
    SceneEditorPaneHost host = {0};
    CorePaneRect left = {0};
    CorePaneRect center = {0};
    CorePaneRect right = {0};

    assert(scene_editor_pane_host_init(&host, 1280.0f, 760.0f));
    assert(scene_editor_pane_host_get_rect_for_role(&host, SCENE_EDITOR_PANE_LEFT, &left));
    assert(scene_editor_pane_host_get_rect_for_role(&host, SCENE_EDITOR_PANE_CENTER, &center));
    assert(scene_editor_pane_host_get_rect_for_role(&host, SCENE_EDITOR_PANE_RIGHT, &right));

    assert(left.width >= 220.0f);
    assert(right.width >= 220.0f);
    assert(center.width > left.width);
    assert(center.width > right.width);
    assert(left.height == 760.0f);
    assert(center.height == 760.0f);
    assert(right.height == 760.0f);
}

static void test_pane_host_rebuild_respects_targets_and_minima(void) {
    SceneEditorPaneHost host = {0};
    CorePaneRect left = {0};
    CorePaneRect center = {0};
    CorePaneRect right = {0};

    assert(scene_editor_pane_host_init(&host, 920.0f, 600.0f));
    scene_editor_pane_host_set_targets(&host, 320.0f, 280.0f);
    assert(scene_editor_pane_host_rebuild(&host, 920.0f, 600.0f));
    assert(scene_editor_pane_host_get_rect_for_role(&host, SCENE_EDITOR_PANE_LEFT, &left));
    assert(scene_editor_pane_host_get_rect_for_role(&host, SCENE_EDITOR_PANE_CENTER, &center));
    assert(scene_editor_pane_host_get_rect_for_role(&host, SCENE_EDITOR_PANE_RIGHT, &right));

    assert(left.width >= 220.0f);
    assert(right.width >= 220.0f);
    assert(center.width >= 360.0f || center.width > left.width);
}

static void test_pane_host_splitter_drag_updates_shell_widths(void) {
    SceneEditorPaneHost host = {0};
    CorePaneRect left_before = {0};
    CorePaneRect center_before = {0};
    CorePaneRect left_after = {0};
    CorePaneRect center_after = {0};
    CorePaneRect splitter_rect = {0};
    bool hovered = false;
    bool active = false;
    float pointer_x = 0.0f;
    float pointer_y = 0.0f;

    assert(scene_editor_pane_host_init(&host, 1280.0f, 760.0f));
    assert(scene_editor_pane_host_get_rect_for_role(&host, SCENE_EDITOR_PANE_LEFT, &left_before));
    assert(scene_editor_pane_host_get_rect_for_role(&host, SCENE_EDITOR_PANE_CENTER, &center_before));

    pointer_x = left_before.x + left_before.width;
    pointer_y = left_before.y + left_before.height * 0.5f;
    scene_editor_pane_host_update_pointer(&host, pointer_x, pointer_y);
    assert(scene_editor_pane_host_visible_splitter(&host, &splitter_rect, &hovered, &active));
    assert(hovered);
    assert(!active);
    assert(scene_editor_pane_host_begin_splitter_drag(&host, pointer_x, pointer_y));
    assert(scene_editor_pane_host_update_splitter_drag(&host, pointer_x + 48.0f, pointer_y));
    assert(scene_editor_pane_host_visible_splitter(&host, &splitter_rect, &hovered, &active));
    assert(hovered);
    assert(active);
    scene_editor_pane_host_end_splitter_drag(&host);

    assert(scene_editor_pane_host_get_rect_for_role(&host, SCENE_EDITOR_PANE_LEFT, &left_after));
    assert(scene_editor_pane_host_get_rect_for_role(&host, SCENE_EDITOR_PANE_CENTER, &center_after));
    assert(left_after.width > left_before.width);
    assert(center_after.width < center_before.width);
}

int main(void) {
    test_pane_host_solves_left_center_right_shell();
    test_pane_host_rebuild_respects_targets_and_minima();
    test_pane_host_splitter_drag_updates_shell_widths();
    return 0;
}
