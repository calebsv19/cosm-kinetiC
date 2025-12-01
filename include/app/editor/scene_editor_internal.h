#ifndef SCENE_EDITOR_INTERNAL_H
#define SCENE_EDITOR_INTERNAL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "app/app_config.h"
#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_widgets.h"
#include "app/editor/scene_editor_scroll.h"
#include "app/preset_io.h"
#include "app/scene_presets.h"
#include "input/input_context.h"
#include "geo/shape_library.h"
#include "ui/text_input.h"

#define DOUBLE_CLICK_MS 350
#define OBJECT_DELETE_MARGIN 0.15f
#define DEFAULT_BOUNDARY_STRENGTH 25.0f
#define MAX_IMPORT_FILES 256

typedef enum EditorSelectionKind {
    SELECTION_NONE = 0,
    SELECTION_EMITTER,
    SELECTION_OBJECT,
    SELECTION_IMPORT
} EditorSelectionKind;

typedef struct SceneEditorState {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font_main;
    TTF_Font     *font_small;
    AppConfig     cfg;
    FluidScenePreset working;
    FluidScenePreset *target;
    InputContextManager *context_mgr;
    bool owns_context_mgr;
    char *name_buffer;
    size_t name_capacity;
    char   local_name[CUSTOM_PRESET_NAME_MAX];
    char  *name_edit_ptr;
    size_t name_edit_capacity;
    TextInputField name_input;
    bool renaming_name;
    Uint32 last_name_click;
    bool editing_width;
    bool editing_height;
    Uint32 last_width_click;
    Uint32 last_height_click;
    TextInputField width_input;
    TextInputField height_input;

    int canvas_x;
    int canvas_y;
    int canvas_width;
    int canvas_height;
    SDL_Rect panel_rect;
    SDL_Rect width_rect;
    SDL_Rect height_rect;

    int  selected_emitter;
    int  hover_emitter;
    EditorDragMode drag_mode;
    bool dragging;
    float drag_offset_x;
    float drag_offset_y;
    int   selected_object;
    int   hover_object;
    bool  dragging_object;
    float object_drag_offset_x;
    float object_drag_offset_y;
    bool  dragging_object_handle;
    float object_handle_ratio;
    float handle_initial_length;
    bool  handle_resize_started;
    EditorSelectionKind selection_kind;
    Uint32 last_canvas_click;

    EditorButton btn_save;
    EditorButton btn_cancel;
    EditorButton btn_add_source;
    EditorButton btn_add_jet;
    EditorButton btn_add_sink;
    EditorButton btn_add_import;
    EditorButton btn_import_back;
    EditorButton btn_import_delete;
    EditorButton btn_boundary;

    NumericField radius_field;
    NumericField strength_field;
    NumericField *active_field;

    EditorListView list_view;
    EditorListView import_view;
    bool showing_import_picker;
    int  hover_row;
    int  selected_row;
    int  hover_import_row;
    int  selected_import_row;
    bool dragging_import_new;
    int  dragging_import_index;
    bool dragging_import_handle;
    float import_handle_start_dist;
    float import_handle_start_scale;
    float import_drag_pos_x;
    float import_drag_pos_y;
    int   emitter_object_map[MAX_FLUID_EMITTERS];
    SDL_Rect list_rect;
    SDL_Rect import_rect;
    char import_files[MAX_IMPORT_FILES][256];
    int  import_file_count;
    Uint32 last_import_click;

    bool boundary_mode;
    int  boundary_hover_edge;
    int  boundary_selected_edge;

    int pointer_x;
    int pointer_y;

    bool running;
    bool applied;
    bool dirty;

    const ShapeAssetLibrary *shape_library;
} SceneEditorState;

void set_dirty(SceneEditorState *state);
float sanitize_domain_dimension(float value);
void editor_update_dimension_rects(SceneEditorState *state);
void editor_update_canvas_layout(SceneEditorState *state);
SDL_Rect editor_name_rect(const SceneEditorState *state);
void editor_begin_name_edit(SceneEditorState *state);
void editor_finish_name_edit(SceneEditorState *state, bool apply);
void editor_begin_dimension_edit(SceneEditorState *state, bool editing_width);
void editor_finish_dimension_edit(SceneEditorState *state, bool editing_width, bool apply);
float clamp01(float v);
void canvas_to_normalized_unclamped(const SceneEditorState *state,
                                    int sx,
                                    int sy,
                                    float *out_x,
                                    float *out_y);

#endif // SCENE_EDITOR_INTERNAL_H
