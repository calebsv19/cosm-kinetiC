#ifndef STRUCTURAL_PRESET_EDITOR_INTERNAL_H
#define STRUCTURAL_PRESET_EDITOR_INTERNAL_H

#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "app/app_config.h"
#include "app/editor/scene_editor_widgets.h"
#include "app/structural/structural_editor.h"
#include "physics/structural/structural_scene.h"
#include "physics/structural/structural_solver.h"

typedef struct StructuralPresetEditor {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font_main;
    TTF_Font     *font_small;
    AppConfig     cfg;
    StructuralScene scene;
    StructuralEditor editor;
    StructuralSolveResult last_result;
    bool scale_initialized;
    float scale_stress;
    float scale_moment;
    float scale_shear;
    float scale_combined;
    bool solve_requested;
    bool running;
    bool applied;
    int pointer_x;
    int pointer_y;

    int canvas_x;
    int canvas_y;
    int canvas_w;
    int canvas_h;
    int panel_x;
    int panel_y;
    int panel_w;
    int panel_h;
    int preview_x;
    int preview_y;
    int preview_w;
    int preview_h;
    float scale;
    float ground_y;
    float ground_snap_dist;
    bool  ground_snap_enabled;

    EditorButton btn_save;
    EditorButton btn_cancel;
    EditorButton btn_ground;
    EditorButton btn_gravity;
    EditorButton btn_gravity_minus;
    EditorButton btn_gravity_plus;

    char preset_path[256];
} StructuralPresetEditor;

#endif // STRUCTURAL_PRESET_EDITOR_INTERNAL_H
