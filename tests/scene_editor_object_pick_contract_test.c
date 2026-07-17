#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/editor/scene_editor_canvas.h"

int main(void) {
    FluidScenePreset preset;
    SceneEditorHit hits[8];
    int center_x = 0;
    int center_y = 0;
    int hit_count = 0;

    memset(&preset, 0, sizeof(preset));
    preset.object_count = 2;
    preset.objects[0].position_x = 0.5f;
    preset.objects[0].position_y = 0.5f;
    preset.objects[1].position_x = 0.5f;
    preset.objects[1].position_y = 0.5f;
    scene_editor_canvas_project(0, 0, 640, 480, 0.5f, 0.5f, &center_x, &center_y);

    assert(scene_editor_canvas_hit_object(
               &preset, 0, 0, 640, 480, center_x, center_y) == 0);
    assert(scene_editor_canvas_hit_object(
               &preset, 0, 0, 640, 480, center_x + 80, center_y) == -1);

    memset(hits, 0, sizeof(hits));
    hit_count = scene_editor_canvas_collect_hits(&preset,
                                                 NULL,
                                                 0,
                                                 0,
                                                 640,
                                                 480,
                                                 center_x,
                                                 center_y,
                                                 NULL,
                                                 NULL,
                                                 hits,
                                                 8);
    assert(hit_count >= 2);
    assert(hits[0].kind == HIT_OBJECT && hits[0].index == 0);
    assert(hits[1].kind == HIT_OBJECT && hits[1].index == 1);

    puts("scene_editor_object_pick_contract_test: ok");
    return 0;
}
