#ifndef SCENE_EDITOR_INTERNAL_H
#define SCENE_EDITOR_INTERNAL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "app/app_config.h"
#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_pane_host.h"
#include "app/editor/scene_editor_retained_document.h"
#include "app/editor/scene_editor_scene_library.h"
#include "app/editor/scene_editor_session.h"
#include "app/editor/scene_editor_viewport.h"
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

typedef struct SceneEditorState {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font_main;
    TTF_Font     *font_small;
    bool          owns_font_main;
    bool          owns_font_small;
    AppConfig     cfg;
    AppConfig    *cfg_live;
    FluidScenePreset working;
    FluidScenePreset *target;
    PhysicsSimEditorSession session;
    PhysicsSimEditorSceneLibrary scene_library;
    SceneEditorViewportState viewport;
    SceneEditorPaneHost pane_host;
    char *retained_runtime_scene_json;
    char retained_runtime_scene_source_path[512];
    char retained_runtime_scene_path[512];
    char retained_scene_provenance_id[128];
    char *last_applied_overlay_json;
    char overlay_apply_diagnostics[256];
    bool overlay_apply_success;
    char save_scene_diagnostics[256];
    bool save_scene_success;
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
    SDL_Rect center_pane_rect;
    SDL_Rect center_title_rect;
    SDL_Rect center_name_rect;
    SDL_Rect center_summary_rect;
    SDL_Rect viewport_surface_rect;
    SDL_Rect right_panel_rect;
    SDL_Rect overlay_summary_rect;
    SDL_Rect width_rect;
    SDL_Rect height_rect;
    int layout_win_w;
    int layout_win_h;

    int  selected_emitter;
    int  hover_emitter;
    EditorDragMode drag_mode;
    bool dragging;
    float drag_offset_x;
    float drag_offset_y;
    float emitter_handle_offset_px;
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
    EditorButton btn_apply_overlay;
    EditorButton btn_cancel;
    EditorButton btn_add_source;
    EditorButton btn_add_jet;
    EditorButton btn_add_sink;
    EditorButton btn_add_import;
    EditorButton btn_import_back;
    EditorButton btn_import_delete;
    EditorButton btn_boundary;
    EditorButton btn_overlay_dynamic;
    EditorButton btn_overlay_static;
    EditorButton btn_overlay_vel_x_neg;
    EditorButton btn_overlay_vel_x_pos;
    EditorButton btn_overlay_vel_y_neg;
    EditorButton btn_overlay_vel_y_pos;
    EditorButton btn_overlay_vel_z_neg;
    EditorButton btn_overlay_vel_z_pos;
    EditorButton btn_overlay_vel_reset;

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
    bool dragging_import_body;
    bool dragging_import_handle;
    float import_body_drag_off_x;
    float import_body_drag_off_y;
    float import_handle_start_dist;
    float import_handle_start_scale;
    float import_drag_pos_x;
    float import_drag_pos_y;
    int   emitter_object_map[MAX_FLUID_EMITTERS];
    int   emitter_import_map[MAX_FLUID_EMITTERS];
    SDL_Rect list_rect;
    SDL_Rect object_info_rect;
    SDL_Rect import_rect;
    char import_files[MAX_IMPORT_FILES][256];
    int  import_file_count;
    Uint32 last_import_click;

    bool boundary_mode;
    int  boundary_hover_edge;
    int  boundary_selected_edge;

    int pointer_x;
    int pointer_y;
    int pointer_down_x;
    int pointer_down_y;
    bool pointer_down_in_canvas;
    bool pointer_drag_started;
    SceneEditorHit hit_stack[32];
    int  hit_stack_count;
    int  hit_stack_base;

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
SDL_Rect editor_active_viewport_rect(const SceneEditorState *state);
void editor_begin_name_edit(SceneEditorState *state);
void editor_finish_name_edit(SceneEditorState *state, bool apply);
void editor_begin_dimension_edit(SceneEditorState *state, bool editing_width);
void editor_finish_dimension_edit(SceneEditorState *state, bool editing_width, bool apply);
void editor_frame_viewport_to_scene(SceneEditorState *state);
bool editor_load_runtime_scene_fixture(SceneEditorState *state,
                                       const char *runtime_scene_path,
                                       char *out_diagnostics,
                                       size_t out_diagnostics_size);
float clamp01(float v);
void canvas_to_normalized_unclamped(const SceneEditorState *state,
                                    int sx,
                                    int sy,
                                    float *out_x,
                                    float *out_y);

#endif // SCENE_EDITOR_INTERNAL_H
