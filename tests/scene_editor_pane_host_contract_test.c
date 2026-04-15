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

int main(void) {
    test_pane_host_solves_left_center_right_shell();
    test_pane_host_rebuild_respects_targets_and_minima();
    return 0;
}
