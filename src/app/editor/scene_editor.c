#include "app/editor/scene_editor.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_import.h"
#include "app/editor/scene_editor_input.h"
#include "app/editor/scene_editor_panel.h"
#include "app/editor/scene_editor_precision.h"

#include "input/input.h"
#include <stdio.h>

bool scene_editor_run(SDL_Window *window,
                      SDL_Renderer *renderer,
                      TTF_Font *font_main,
                      TTF_Font *font_small,
                      const AppConfig *cfg,
                      FluidScenePreset *preset,
                      InputContextManager *ctx_mgr,
                      const ShapeAssetLibrary *shape_library,
                      char *name_buffer,
                      size_t name_capacity) {
    if (!window || !renderer || !preset) return false;

    SceneEditorState state = {
        .window = window,
        .renderer = renderer,
        .font_main = font_main,
        .font_small = font_small,
        .cfg = *cfg,
        .working = *preset,
        .target = preset,
        .context_mgr = ctx_mgr,
        .owns_context_mgr = false,
        .name_buffer = name_buffer,
        .name_capacity = name_capacity,
        .renaming_name = false,
        .last_name_click = 0,
        .selected_emitter = (preset->emitter_count > 0) ? 0 : -1,
        .hover_emitter = -1,
        .drag_mode = DRAG_NONE,
        .dragging = false,
        .drag_offset_x = 0.0f,
        .drag_offset_y = 0.0f,
        .selected_object = (preset->emitter_count == 0 && preset->object_count > 0) ? 0 : -1,
        .hover_object = -1,
        .dragging_object = false,
        .object_drag_offset_x = 0.0f,
        .object_drag_offset_y = 0.0f,
        .dragging_object_handle = false,
        .object_handle_ratio = 1.0f,
        .handle_initial_length = 0.0f,
        .handle_resize_started = false,
        .last_canvas_click = 0,
        .selection_kind = (preset->emitter_count > 0)
                              ? SELECTION_EMITTER
                              : ((preset->object_count > 0) ? SELECTION_OBJECT : SELECTION_NONE),
        .active_field = NULL,
        .boundary_mode = false,
        .boundary_hover_edge = -1,
        .boundary_selected_edge = -1,
        .pointer_x = -1,
        .pointer_y = -1,
        .running = true,
        .applied = false,
        .dirty = false,
        .shape_library = shape_library
    };
    state.shape_library = shape_library;
    if (state.working.domain_width <= 0.0f) state.working.domain_width = 1.0f;
    if (state.working.domain_height <= 0.0f) state.working.domain_height = 1.0f;

    if (state.name_buffer) {
        state.name_edit_ptr = state.name_buffer;
        state.name_edit_capacity = state.name_capacity;
    } else {
        state.name_edit_ptr = state.local_name;
        state.name_edit_capacity = sizeof(state.local_name);
    }
    if (preset->name) {
        snprintf(state.name_edit_ptr, state.name_edit_capacity, "%s", preset->name);
    } else {
        state.name_edit_ptr[0] = '\0';
    }
    state.working.name = state.name_edit_ptr;
    state.import_file_count = 0;
    state.last_import_click = 0;
    state.dragging_import_new = false;
    state.dragging_import_index = -1;
    state.import_drag_pos_x = 0.5f;
    state.import_drag_pos_y = 0.5f;
    for (int i = 0; i < MAX_FLUID_EMITTERS; ++i) {
        state.emitter_object_map[i] = -1;
        state.emitter_import_map[i] = -1;
    }
    for (size_t i = 0; i < state.working.emitter_count && i < MAX_FLUID_EMITTERS; ++i) {
        FluidEmitter *em = &state.working.emitters[i];
        if (em->attached_object < 0 ||
            em->attached_object >= (int)state.working.object_count) {
            em->attached_object = -1;
        }
        if (em->attached_import < 0 ||
            em->attached_import >= (int)state.working.import_shape_count) {
            em->attached_import = -1;
        }
        state.emitter_object_map[i] = em->attached_object;
        state.emitter_import_map[i] = em->attached_import;
    }

    InputContextManager local_mgr;
    if (!state.context_mgr) {
        input_context_manager_init(&local_mgr);
        state.context_mgr = &local_mgr;
        state.owns_context_mgr = true;
    }

    editor_update_canvas_layout(&state);

    int inner_x = state.panel_rect.x + 12;
    int panel_width = state.panel_rect.w;
    int field_w = panel_width - 24;
    int y_cursor = state.panel_rect.y + 12;

    state.radius_field = (NumericField){
        .rect = {inner_x, y_cursor + 40, field_w, 32},
        .label = "Radius",
        .target = FIELD_RADIUS,
        .editing = false
    };
    state.strength_field = (NumericField){
        .rect = {inner_x, y_cursor + 40 + 32 + 12, field_w, 32},
        .label = "Strength",
        .target = FIELD_STRENGTH,
        .editing = false
    };

    int button_row_y = state.strength_field.rect.y + state.strength_field.rect.h + 14;
    int button_h = 34;
    int button_w = (field_w - 24) / 3;
    state.btn_add_source = (EditorButton){
        .rect = {inner_x, button_row_y, button_w, button_h},
        .label = "Source",
        .enabled = true
    };
    state.btn_add_jet = (EditorButton){
        .rect = {inner_x + button_w + 12, button_row_y, button_w, button_h},
        .label = "Jet",
        .enabled = true
    };
    state.btn_add_sink = (EditorButton){
        .rect = {inner_x + (button_w + 12) * 2, button_row_y, button_w, button_h},
        .label = "Sink",
        .enabled = true
    };
    int import_y = button_row_y + button_h + 12;
    state.btn_add_import = (EditorButton){
        .rect = {inner_x, import_y, field_w, 34},
        .label = "Add from JSON",
        .enabled = true
    };
    state.btn_import_back = (EditorButton){
        .rect = {inner_x, import_y, field_w, 34},
        .label = "Back to Objects",
        .enabled = true
    };
    state.btn_import_delete = (EditorButton){
        .rect = {inner_x, import_y + 38, field_w, 30},
        .label = "Delete Selected",
        .enabled = true
    };

    int boundary_y = import_y + 34 + 10;
    state.btn_boundary = (EditorButton){
        .rect = {inner_x, boundary_y, field_w, 34},
        .label = "Air Flow Mode",
        .enabled = true
    };

    state.btn_save = (EditorButton){
        .rect = {inner_x,
                 state.panel_rect.y + state.panel_rect.h - 90,
                 field_w,
                 40},
        .label = "Save Changes",
        .enabled = true
    };
    state.btn_cancel = (EditorButton){
        .rect = {inner_x,
                 state.panel_rect.y + state.panel_rect.h - 44,
                 field_w,
                 36},
        .label = "Cancel",
        .enabled = true
    };

    int list_top = boundary_y + 34 + 14;
    int list_height = state.btn_save.rect.y - 12 - list_top;
    if (list_height < 80) list_height = 80;
    state.list_rect = (SDL_Rect){inner_x, list_top, field_w, list_height};
    state.import_rect = state.list_rect;
    editor_list_view_init(&state.list_view, (SDL_Rect){state.list_rect.x + state.list_rect.w - 10, state.list_rect.y, 8, state.list_rect.h}, 28);
    editor_list_view_init(&state.import_view, (SDL_Rect){state.import_rect.x + state.import_rect.w - 10, state.import_rect.y, 8, state.import_rect.h}, 28);
    scene_editor_refresh_import_files(&state);

    InputContext editor_ctx = {
        .on_pointer_down = editor_pointer_down,
        .on_pointer_up = editor_pointer_up,
        .on_pointer_move = editor_pointer_move,
        .on_wheel = editor_on_wheel,
        .on_key_down = editor_key_down,
        .on_key_up = editor_key_up,
        .on_text_input = editor_text_input,
        .user_data = &state
    };
    input_context_manager_push(state.context_mgr, &editor_ctx);

    Uint32 prev_ticks = SDL_GetTicks();
    while (state.running) {
        Uint32 now = SDL_GetTicks();
        double dt = (double)(now - prev_ticks) / 1000.0;
        prev_ticks = now;
        text_input_update(&state.name_input, dt);
        text_input_update(&state.width_input, dt);
        text_input_update(&state.height_input, dt);

        InputCommands cmds;
        if (!input_poll_events(&cmds, NULL, state.context_mgr)) {
            state.running = false;
            break;
        }
        if (cmds.quit) {
            state.running = false;
            break;
        }

        state.btn_add_source.enabled = state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_jet.enabled = state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_sink.enabled = state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_import.enabled = true;
        state.btn_import_delete.enabled = (state.working.import_shape_count > 0) &&
                                          (state.selected_row >= 0) &&
                                          (state.selected_row < (int)state.working.import_shape_count);

        scene_editor_panel_draw(&state);
        SDL_RenderPresent(renderer);
    }

    input_context_manager_pop(state.context_mgr);

    // Flush pending mouse events so menu clicks don't immediately re-trigger after closing.
    SDL_FlushEvent(SDL_MOUSEBUTTONDOWN);
    SDL_FlushEvent(SDL_MOUSEBUTTONUP);
    SDL_FlushEvent(SDL_MOUSEMOTION);

    if (state.owns_context_mgr) {
        // nothing extra to clean up beyond stack pop
    }

    if (state.renaming_name) {
        editor_finish_name_edit(&state, false);
    }

    if (state.applied && state.target) {
        state.working.is_custom = true;
        *state.target = state.working;
    }

    return state.applied;
}
