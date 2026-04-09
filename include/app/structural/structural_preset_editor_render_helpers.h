#ifndef STRUCTURAL_PRESET_EDITOR_RENDER_HELPERS_H
#define STRUCTURAL_PRESET_EDITOR_RENDER_HELPERS_H

#include <stdbool.h>

#include "app/structural/structural_preset_editor_internal.h"

void structural_preset_editor_world_to_screen(const StructuralPresetEditor *editor,
                                              float wx,
                                              float wy,
                                              float *sx,
                                              float *sy);
void structural_preset_editor_screen_to_world(const StructuralPresetEditor *editor,
                                              int sx,
                                              int sy,
                                              float *wx,
                                              float *wy);
void structural_preset_editor_apply_snap(const StructuralPresetEditor *editor, float *x, float *y);
void structural_preset_editor_apply_ground_snap(const StructuralPresetEditor *editor, float *x, float *y);
bool structural_preset_editor_point_in_preview(const StructuralPresetEditor *editor, int x, int y);
void structural_preset_editor_render_scene(StructuralPresetEditor *editor);

#endif // STRUCTURAL_PRESET_EDITOR_RENDER_HELPERS_H
