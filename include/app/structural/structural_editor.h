#ifndef STRUCTURAL_EDITOR_H
#define STRUCTURAL_EDITOR_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "input/input_context.h"
#include "physics/structural/structural_scene.h"

typedef enum StructuralTool {
    STRUCT_TOOL_SELECT = 0,
    STRUCT_TOOL_ADD_NODE,
    STRUCT_TOOL_ADD_EDGE,
    STRUCT_TOOL_ADD_LOAD,
    STRUCT_TOOL_ADD_MOMENT
} StructuralTool;

typedef struct StructuralEditor {
    StructuralScene *scene;
    StructuralTool   tool;

    bool  snap_to_grid;
    float grid_size;

    int   edge_start_node_id;
    int   drag_node_id;
    bool  dragging;
    float drag_offset_x;
    float drag_offset_y;

    bool  box_selecting;
    int   box_start_x;
    int   box_start_y;
    int   box_end_x;
    int   box_end_y;

    bool  load_dragging;
    int   load_node_id;
    float load_start_x;
    float load_start_y;

    bool  moment_dragging;
    int   moment_node_id;
    float moment_start_x;
    float moment_start_y;

    int   active_material;

    bool  show_ids;
    bool  show_constraints;
    bool  show_loads;
    bool  show_deformed;
    bool  show_stress;
    bool  show_bending;
    bool  show_shear;
    bool  show_combined;
    bool  scale_use_percentile;
    bool  scale_freeze;
    bool  scale_thickness;
    float scale_gamma;
    float scale_percentile;
    float thickness_gain;
    float deform_scale;

    char  status_message[128];
} StructuralEditor;

void structural_editor_init(StructuralEditor *editor, StructuralScene *scene);
void structural_editor_set_status(StructuralEditor *editor, const char *msg);

void structural_editor_handle_pointer_down(StructuralEditor *editor,
                                           const InputPointerState *state,
                                           SDL_Keymod mod);
void structural_editor_handle_pointer_up(StructuralEditor *editor,
                                         const InputPointerState *state,
                                         SDL_Keymod mod);
void structural_editor_handle_pointer_move(StructuralEditor *editor,
                                           const InputPointerState *state,
                                           SDL_Keymod mod);
void structural_editor_handle_key_down(StructuralEditor *editor,
                                       SDL_Keycode key,
                                       SDL_Keymod mod);

void structural_editor_render_box(const StructuralEditor *editor,
                                  SDL_Renderer *renderer);

#endif // STRUCTURAL_EDITOR_H
