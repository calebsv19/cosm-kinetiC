#include "app/editor/scene_editor.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_import.h"
#include "app/editor/scene_editor_input.h"
#include "app/editor/scene_editor_panel.h"
#include "app/editor/scene_editor_precision.h"
#include "app/menu/shared_theme_font_adapter.h"
#include "app/sim_mode.h"
#include "app/data_paths.h"

#include "config/config_loader.h"
#include "font_paths.h"
#include "input/input.h"
#include <stdio.h>

#include "vk_renderer.h"

static int editor_scaled_font_size(const AppConfig *cfg,
                                   int base_point_size,
                                   int min_point_size) {
    return app_config_scale_text_point_size(cfg, base_point_size, min_point_size);
}

static TTF_Font *editor_open_body_font(const AppConfig *cfg) {
    char shared_path[256];
    int shared_size = 22;
    if (physics_sim_shared_font_resolve_menu_body(shared_path, sizeof(shared_path), &shared_size)) {
        shared_size = editor_scaled_font_size(cfg, shared_size, 6);
        return TTF_OpenFont(shared_path, shared_size);
    }
    shared_size = editor_scaled_font_size(cfg, 22, 6);
    {
        TTF_Font *font = TTF_OpenFont(FONT_BODY_PATH_1, shared_size);
        if (font) return font;
    }
    return TTF_OpenFont(FONT_BODY_PATH_2, shared_size);
}

static TTF_Font *editor_open_small_font(const AppConfig *cfg) {
    char shared_path[256];
    int shared_size = 18;
    if (physics_sim_shared_font_resolve_menu_small(shared_path, sizeof(shared_path), &shared_size)) {
        shared_size = editor_scaled_font_size(cfg, shared_size, 6);
        return TTF_OpenFont(shared_path, shared_size);
    }
    shared_size = editor_scaled_font_size(cfg, 18, 6);
    {
        TTF_Font *font = TTF_OpenFont(FONT_BODY_PATH_1, shared_size);
        if (font) return font;
    }
    return TTF_OpenFont(FONT_BODY_PATH_2, shared_size);
}

static void editor_close_owned_fonts(SceneEditorState *state) {
    if (!state) return;
    if (state->owns_font_main && state->font_main) {
        TTF_CloseFont(state->font_main);
        state->font_main = NULL;
        state->owns_font_main = false;
    }
    if (state->owns_font_small && state->font_small) {
        TTF_CloseFont(state->font_small);
        state->font_small = NULL;
        state->owns_font_small = false;
    }
}

static bool editor_reload_fonts(SceneEditorState *state) {
    TTF_Font *new_main = NULL;
    TTF_Font *new_small = NULL;
    if (!state) return false;
    new_main = editor_open_body_font(&state->cfg);
    new_small = editor_open_small_font(&state->cfg);
    if (!new_main || !new_small) {
        if (new_main) TTF_CloseFont(new_main);
        if (new_small) TTF_CloseFont(new_small);
        return false;
    }
    editor_close_owned_fonts(state);
    state->font_main = new_main;
    state->font_small = new_small;
    state->owns_font_main = true;
    state->owns_font_small = true;
    return true;
}

static void editor_refresh_list_metrics(SceneEditorState *state) {
    int row_height = 28;
    if (!state) return;
    if (state->font_small) {
        int font_h = TTF_FontHeight(state->font_small);
        if (font_h > 0) {
            row_height = font_h + 10;
            if (row_height < 28) row_height = 28;
        }
    }
    state->list_view.row_height = row_height;
    state->import_view.row_height = row_height;
}

static int editor_font_height(TTF_Font *font, int fallback) {
    if (!font) return fallback;
    {
        int h = TTF_FontHeight(font);
        if (h > 0) return h;
    }
    return fallback;
}

static void editor_update_list_track(EditorListView *view,
                                     const SDL_Rect *list_rect) {
    if (!view || !list_rect) return;
    view->scroll.track = (SDL_Rect){
        list_rect->x + list_rect->w - 10,
        list_rect->y,
        8,
        list_rect->h
    };
    editor_scroll_set_view(&view->scroll, (float)list_rect->h);
    editor_scroll_set_content(&view->scroll,
                              (float)view->row_count * (float)view->row_height);
}

static void editor_layout_controls(SceneEditorState *state) {
    int inner_x = 0;
    int panel_width = 0;
    int field_w = 0;
    int small_h = 18;
    int field_h = 32;
    int button_h = 34;
    int button_w = 0;
    int label_gap = 12;
    int y_cursor = 0;
    int button_row_y = 0;
    int import_y = 0;
    int boundary_y = 0;
    int save_h = 40;
    int cancel_h = 36;
    int cancel_y = 0;
    int save_y = 0;
    int list_top = 0;
    int list_height = 0;
    int bottom_pad = 8;
    if (!state) return;

    inner_x = state->panel_rect.x + 12;
    panel_width = state->panel_rect.w;
    field_w = panel_width - 24;
    if (field_w < 120) field_w = 120;

    small_h = editor_font_height(state->font_small, 18);
    field_h = small_h + 16;
    if (field_h < 32) field_h = 32;
    button_h = small_h + 14;
    if (button_h < 34) button_h = 34;
    label_gap = small_h + 8;
    if (label_gap < 12) label_gap = 12;

    y_cursor = state->panel_rect.y + 12;
    state->radius_field.rect = (SDL_Rect){inner_x, y_cursor + label_gap, field_w, field_h};
    state->radius_field.label = "Radius";
    state->radius_field.target = FIELD_RADIUS;
    state->strength_field.rect = (SDL_Rect){inner_x,
                                            state->radius_field.rect.y + state->radius_field.rect.h + 12,
                                            field_w,
                                            field_h};
    state->strength_field.label = "Strength";
    state->strength_field.target = FIELD_STRENGTH;

    button_row_y = state->strength_field.rect.y + state->strength_field.rect.h + 14;
    button_w = (field_w - 24) / 3;
    if (button_w < 60) button_w = 60;
    state->btn_add_source.rect = (SDL_Rect){inner_x, button_row_y, button_w, button_h};
    state->btn_add_source.label = "Source";
    state->btn_add_source.enabled = true;
    state->btn_add_jet.rect = (SDL_Rect){inner_x + button_w + 12, button_row_y, button_w, button_h};
    state->btn_add_jet.label = "Jet";
    state->btn_add_jet.enabled = true;
    state->btn_add_sink.rect = (SDL_Rect){inner_x + (button_w + 12) * 2, button_row_y, button_w, button_h};
    state->btn_add_sink.label = "Sink";
    state->btn_add_sink.enabled = true;

    import_y = button_row_y + button_h + 12;
    state->btn_add_import.rect = (SDL_Rect){inner_x, import_y, field_w, button_h};
    state->btn_add_import.label = "Add from JSON";
    state->btn_add_import.enabled = true;
    state->btn_import_back.rect = state->btn_add_import.rect;
    state->btn_import_back.label = "Back to Objects";
    state->btn_import_back.enabled = true;
    state->btn_import_delete.rect = (SDL_Rect){inner_x,
                                               import_y + button_h + 8,
                                               field_w,
                                               button_h - 4};
    if (state->btn_import_delete.rect.h < 30) state->btn_import_delete.rect.h = 30;
    state->btn_import_delete.label = "Delete Selected";
    state->btn_import_delete.enabled = true;

    boundary_y = import_y + button_h + 10;
    state->btn_boundary.rect = (SDL_Rect){inner_x, boundary_y, field_w, button_h};
    state->btn_boundary.label = "Air Flow Mode";
    state->btn_boundary.enabled = true;

    save_h = small_h + 18;
    if (save_h < 40) save_h = 40;
    cancel_h = small_h + 16;
    if (cancel_h < 36) cancel_h = 36;
    cancel_y = state->panel_rect.y + state->panel_rect.h - bottom_pad - cancel_h;
    save_y = cancel_y - 6 - save_h;
    if (save_y < boundary_y + button_h + 60) {
        save_y = boundary_y + button_h + 60;
        cancel_y = save_y + save_h + 6;
    }
    if (cancel_y + cancel_h > state->panel_rect.y + state->panel_rect.h - bottom_pad) {
        cancel_y = state->panel_rect.y + state->panel_rect.h - bottom_pad - cancel_h;
        save_y = cancel_y - 6 - save_h;
    }
    state->btn_save.rect = (SDL_Rect){inner_x, save_y, field_w, save_h};
    state->btn_cancel.rect = (SDL_Rect){inner_x, cancel_y, field_w, cancel_h};
    state->btn_save.label = "Save Changes";
    state->btn_save.enabled = true;
    state->btn_cancel.label = "Cancel";
    state->btn_cancel.enabled = true;

    list_top = boundary_y + button_h + 14;
    list_height = state->btn_save.rect.y - 12 - list_top;
    if (list_height < 80) list_height = 80;
    state->list_rect = (SDL_Rect){inner_x, list_top, field_w, list_height};
    state->import_rect = state->list_rect;
    editor_update_list_track(&state->list_view, &state->list_rect);
    editor_update_list_track(&state->import_view, &state->import_rect);

}

static void editor_reflow_layout(SceneEditorState *state) {
    if (!state) return;
    editor_update_canvas_layout(state);
    editor_layout_controls(state);
}

static bool editor_text_entry_active(const SceneEditorState *state) {
    if (!state) return false;
    if (state->renaming_name) return true;
    if (state->editing_width || state->editing_height) return true;
    if (state->active_field && state->active_field->editing) return true;
    return false;
}

static bool editor_apply_text_zoom_shortcut(SceneEditorState *state,
                                            const InputCommands *cmds) {
    int next_step = 0;
    AppConfig *persist_cfg = NULL;
    const char *runtime_config_path = physics_sim_runtime_config_path();
    if (!state || !cmds) return false;
    if (!(cmds->text_zoom_in_requested ||
          cmds->text_zoom_out_requested ||
          cmds->text_zoom_reset_requested)) {
        return false;
    }
    if (editor_text_entry_active(state)) return false;

    next_step = state->cfg.text_zoom_step;
    if (cmds->text_zoom_reset_requested) {
        next_step = 0;
    } else {
        if (cmds->text_zoom_in_requested) next_step += 1;
        if (cmds->text_zoom_out_requested) next_step -= 1;
    }
    next_step = app_config_text_zoom_step_clamp(next_step);
    if (next_step == state->cfg.text_zoom_step) return false;

    state->cfg.text_zoom_step = next_step;
    if (state->cfg_live) {
        state->cfg_live->text_zoom_step = next_step;
    }
    persist_cfg = state->cfg_live ? state->cfg_live : &state->cfg;
    if (!config_loader_save(persist_cfg, runtime_config_path)) {
        fprintf(stderr, "[editor] Failed to persist runtime config to %s\n",
                runtime_config_path);
    }
    if (!editor_reload_fonts(state)) {
        fprintf(stderr, "[editor] Failed to reload fonts after zoom update.\n");
    }
    editor_refresh_list_metrics(state);
    editor_reflow_layout(state);
    return true;
}

bool scene_editor_run(SDL_Window *window,
                      SDL_Renderer *renderer,
                      TTF_Font *font_main,
                      TTF_Font *font_small,
                      AppConfig *cfg,
                      FluidScenePreset *preset,
                      InputContextManager *ctx_mgr,
                      const ShapeAssetLibrary *shape_library,
                      char *name_buffer,
                      size_t name_capacity) {
    if (!window || !renderer || !cfg || !preset) return false;

    SceneEditorState state = {
        .window = window,
        .renderer = renderer,
        .font_main = font_main,
        .font_small = font_small,
        .owns_font_main = false,
        .owns_font_small = false,
        .cfg = *cfg,
        .cfg_live = cfg,
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
        .shape_library = shape_library,
        .layout_win_w = -1,
        .layout_win_h = -1
    };
    state.shape_library = shape_library;
    if (state.working.domain_width <= 0.0f) state.working.domain_width = 1.0f;
    if (state.working.domain_height <= 0.0f) state.working.domain_height = 1.0f;
    {
        SimModeRoute editor_route = sim_mode_resolve_route(state.cfg.sim_mode, state.cfg.space_mode);
        scene_editor_canvas_set_mode_route(&editor_route);
    }

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

    editor_reflow_layout(&state);
    editor_list_view_init(&state.list_view,
                          (SDL_Rect){state.list_rect.x + state.list_rect.w - 10,
                                     state.list_rect.y,
                                     8,
                                     state.list_rect.h},
                          28);
    editor_list_view_init(&state.import_view,
                          (SDL_Rect){state.import_rect.x + state.import_rect.w - 10,
                                     state.import_rect.y,
                                     8,
                                     state.import_rect.h},
                          28);
    if (!editor_reload_fonts(&state)) {
        fprintf(stderr, "[editor] Falling back to inherited fonts (zoom reload failed).\n");
    }
    editor_refresh_list_metrics(&state);
    editor_reflow_layout(&state);
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
        SimModeRoute editor_route = sim_mode_resolve_route(state.cfg.sim_mode, state.cfg.space_mode);
        scene_editor_canvas_set_mode_route(&editor_route);
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
        (void)editor_apply_text_zoom_shortcut(&state, &cmds);

        state.btn_add_source.enabled = state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_jet.enabled = state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_sink.enabled = state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_import.enabled = true;
        state.btn_import_delete.enabled = (state.working.import_shape_count > 0) &&
                                          (state.selected_row >= 0) &&
                                          (state.selected_row < (int)state.working.import_shape_count);

        int win_w = 0;
        int win_h = 0;
        SDL_GetWindowSize(window, &win_w, &win_h);
        if (win_w <= 0 || win_h <= 0) {
            win_w = state.panel_rect.x + state.panel_rect.w;
            win_h = state.panel_rect.y + state.panel_rect.h;
        }
        if (win_w != state.layout_win_w || win_h != state.layout_win_h) {
            state.layout_win_w = win_w;
            state.layout_win_h = win_h;
            editor_reflow_layout(&state);
        }

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFramebuffer fb = VK_NULL_HANDLE;
        VkExtent2D extent = {0};
        VkResult frame = vk_renderer_begin_frame((VkRenderer *)renderer, &cmd, &fb, &extent);
        if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
            vk_renderer_recreate_swapchain((VkRenderer *)renderer, window);
            vk_renderer_set_logical_size((VkRenderer *)renderer, (float)win_w, (float)win_h);
            continue;
        } else if (frame != VK_SUCCESS) {
            fprintf(stderr, "[editor] vk_renderer_begin_frame failed: %d\n", frame);
            continue;
        }

        vk_renderer_set_logical_size((VkRenderer *)renderer, (float)win_w, (float)win_h);
        scene_editor_panel_draw(&state);
        VkResult end = vk_renderer_end_frame((VkRenderer *)renderer, cmd);
        if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
            vk_renderer_recreate_swapchain((VkRenderer *)renderer, window);
            vk_renderer_set_logical_size((VkRenderer *)renderer, (float)win_w, (float)win_h);
        } else if (end != VK_SUCCESS) {
            fprintf(stderr, "[editor] vk_renderer_end_frame failed: %d\n", end);
        }
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

    editor_close_owned_fonts(&state);

    return state.applied;
}
