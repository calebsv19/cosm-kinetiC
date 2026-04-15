#include "app/editor/scene_editor.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_import.h"
#include "app/editor/scene_editor_input.h"
#include "app/editor/scene_editor_model.h"
#include "app/editor/scene_editor_panel.h"
#include "app/editor/scene_editor_precision.h"
#include "app/menu/shared_theme_font_adapter.h"
#include "app/sim_mode.h"
#include "app/data_paths.h"

#include "config/config_loader.h"
#include "font_paths.h"
#include "input/input.h"
#include "render/text_upload_policy.h"
#include <stdio.h>

#include "vk_renderer.h"

static int editor_scaled_font_size(const AppConfig *cfg,
                                   int base_point_size,
                                   int min_point_size) {
    return app_config_scale_text_point_size(cfg, base_point_size, min_point_size);
}

static TTF_Font *editor_open_body_font(const AppConfig *cfg, SDL_Renderer *renderer) {
    char shared_path[256];
    int shared_size = 22;
    if (physics_sim_shared_font_resolve_menu_body(shared_path, sizeof(shared_path), &shared_size)) {
        shared_size = editor_scaled_font_size(cfg, shared_size, 6);
        shared_size = physics_sim_text_raster_point_size(renderer, shared_size, 6);
        return TTF_OpenFont(shared_path, shared_size);
    }
    shared_size = editor_scaled_font_size(cfg, 22, 6);
    shared_size = physics_sim_text_raster_point_size(renderer, shared_size, 6);
    {
        TTF_Font *font = TTF_OpenFont(FONT_BODY_PATH_1, shared_size);
        if (font) return font;
    }
    return TTF_OpenFont(FONT_BODY_PATH_2, shared_size);
}

static TTF_Font *editor_open_small_font(const AppConfig *cfg, SDL_Renderer *renderer) {
    char shared_path[256];
    int shared_size = 18;
    if (physics_sim_shared_font_resolve_menu_small(shared_path, sizeof(shared_path), &shared_size)) {
        shared_size = editor_scaled_font_size(cfg, shared_size, 6);
        shared_size = physics_sim_text_raster_point_size(renderer, shared_size, 6);
        return TTF_OpenFont(shared_path, shared_size);
    }
    shared_size = editor_scaled_font_size(cfg, 18, 6);
    shared_size = physics_sim_text_raster_point_size(renderer, shared_size, 6);
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
    new_main = editor_open_body_font(&state->cfg, state->renderer);
    new_small = editor_open_small_font(&state->cfg, state->renderer);
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
            font_h = physics_sim_text_logical_pixels(state->renderer, font_h);
            row_height = font_h + 10;
            if (row_height < 28) row_height = 28;
        }
    }
    state->list_view.row_height = row_height;
    state->import_view.row_height = row_height;
}

static int editor_font_height(SDL_Renderer *renderer, TTF_Font *font, int fallback) {
    if (!font) return fallback;
    {
        int h = TTF_FontHeight(font);
        if (h > 0) {
            return physics_sim_text_logical_pixels(renderer, h);
        }
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
    int left_x = 0;
    int right_x = 0;
    int left_field_w = 0;
    int right_field_w = 0;
    int main_h = 24;
    int small_h = 18;
    int field_h = 32;
    int button_h = 34;
    int button_w = 0;
    int button_row_y = 0;
    int import_y = 0;
    int boundary_y = 0;
    int save_h = 40;
    int apply_h = 40;
    int cancel_h = 36;
    int action_gap = 8;
    int bottom_button_w = 0;
    int cancel_y = 0;
    int save_y = 0;
    int apply_y = 0;
    int list_top = 0;
    int list_height = 0;
    int info_card_h = 0;
    int bottom_pad = 8;
    int inspector_top = 0;
    int overlay_top = 0;
    int overlay_gap = 6;
    int overlay_row_button_w = 0;
    int velocity_button_w = 0;
    int overlay_button_h = 0;
    int velocity_button_h = 0;
    int summary_top = 0;
    int info_line_step = 0;
    if (!state) return;

    left_x = state->panel_rect.x + 12;
    right_x = state->right_panel_rect.x + 12;
    left_field_w = state->panel_rect.w - 24;
    right_field_w = state->right_panel_rect.w - 24;
    if (left_field_w < 140) left_field_w = 140;
    if (right_field_w < 140) right_field_w = 140;

    main_h = editor_font_height(state->renderer, state->font_main, 24);
    if (main_h < 20) main_h = 20;
    small_h = editor_font_height(state->renderer, state->font_small, 18);
    field_h = small_h + 16;
    if (field_h < 32) field_h = 32;
    button_h = small_h + 14;
    if (button_h < 34) button_h = 34;
    overlay_button_h = small_h + 8;
    if (overlay_button_h < 28) overlay_button_h = 28;
    velocity_button_h = small_h + 6;
    if (velocity_button_h < 26) velocity_button_h = 26;

    inspector_top = state->height_rect.y + state->height_rect.h + 20;
    state->radius_field.rect = (SDL_Rect){right_x, inspector_top + 24, right_field_w, field_h};
    state->radius_field.label = "Radius";
    state->radius_field.target = FIELD_RADIUS;
    state->strength_field.rect = (SDL_Rect){right_x,
                                            state->radius_field.rect.y + state->radius_field.rect.h + 14,
                                            right_field_w,
                                            field_h};
    state->strength_field.label = "Strength";
    state->strength_field.target = FIELD_STRENGTH;

    overlay_top = state->strength_field.rect.y + state->strength_field.rect.h + 18;
    overlay_row_button_w = (right_field_w - overlay_gap * 2) / 3;
    if (overlay_row_button_w < 52) overlay_row_button_w = 52;
    velocity_button_w = (right_field_w - overlay_gap * 5) / 6;
    if (velocity_button_w < 24) velocity_button_w = 24;

    state->btn_overlay_dynamic.rect = (SDL_Rect){right_x, overlay_top, overlay_row_button_w, overlay_button_h};
    state->btn_overlay_dynamic.label = "Dynamic";
    state->btn_overlay_dynamic.enabled = false;
    state->btn_overlay_static.rect = (SDL_Rect){right_x + overlay_row_button_w + overlay_gap,
                                                overlay_top,
                                                overlay_row_button_w,
                                                overlay_button_h};
    state->btn_overlay_static.label = "Static";
    state->btn_overlay_static.enabled = false;
    state->btn_overlay_vel_reset.rect = (SDL_Rect){right_x + (overlay_row_button_w + overlay_gap) * 2,
                                                   overlay_top,
                                                   overlay_row_button_w,
                                                   overlay_button_h};
    state->btn_overlay_vel_reset.label = "Reset Vel";
    state->btn_overlay_vel_reset.enabled = false;

    overlay_top += overlay_button_h + overlay_gap;
    state->btn_overlay_vel_x_pos.rect = (SDL_Rect){right_x, overlay_top, velocity_button_w, velocity_button_h};
    state->btn_overlay_vel_x_pos.label = "X+";
    state->btn_overlay_vel_x_pos.enabled = false;
    state->btn_overlay_vel_y_pos.rect = (SDL_Rect){right_x + velocity_button_w + overlay_gap,
                                                   overlay_top,
                                                   velocity_button_w,
                                                   velocity_button_h};
    state->btn_overlay_vel_y_pos.label = "Y+";
    state->btn_overlay_vel_y_pos.enabled = false;
    state->btn_overlay_vel_z_pos.rect = (SDL_Rect){right_x + (velocity_button_w + overlay_gap) * 2,
                                                   overlay_top,
                                                   velocity_button_w,
                                                   velocity_button_h};
    state->btn_overlay_vel_z_pos.label = "Z+";
    state->btn_overlay_vel_z_pos.enabled = false;
    state->btn_overlay_vel_x_neg.rect = (SDL_Rect){right_x + (velocity_button_w + overlay_gap) * 3,
                                                   overlay_top,
                                                   velocity_button_w,
                                                   velocity_button_h};
    state->btn_overlay_vel_x_neg.label = "X-";
    state->btn_overlay_vel_x_neg.enabled = false;
    state->btn_overlay_vel_y_neg.rect = (SDL_Rect){right_x + (velocity_button_w + overlay_gap) * 4,
                                                   overlay_top,
                                                   velocity_button_w,
                                                   velocity_button_h};
    state->btn_overlay_vel_y_neg.label = "Y-";
    state->btn_overlay_vel_y_neg.enabled = false;
    state->btn_overlay_vel_z_neg.rect = (SDL_Rect){right_x + (velocity_button_w + overlay_gap) * 5,
                                                   overlay_top,
                                                   velocity_button_w,
                                                   velocity_button_h};
    state->btn_overlay_vel_z_neg.label = "Z-";
    state->btn_overlay_vel_z_neg.enabled = false;
    summary_top = overlay_top + velocity_button_h + 14;
    state->overlay_summary_rect = (SDL_Rect){right_x, summary_top, right_field_w, 0};

    button_row_y = state->panel_rect.y + 12 + main_h + small_h + 20;
    button_w = (left_field_w - 24) / 3;
    if (button_w < 60) button_w = 60;
    state->btn_add_source.rect = (SDL_Rect){left_x, button_row_y, button_w, button_h};
    state->btn_add_source.label = "Source";
    state->btn_add_source.enabled = true;
    state->btn_add_jet.rect = (SDL_Rect){left_x + button_w + 12, button_row_y, button_w, button_h};
    state->btn_add_jet.label = "Jet";
    state->btn_add_jet.enabled = true;
    state->btn_add_sink.rect = (SDL_Rect){left_x + (button_w + 12) * 2, button_row_y, button_w, button_h};
    state->btn_add_sink.label = "Sink";
    state->btn_add_sink.enabled = true;

    import_y = button_row_y + button_h + 12;
    state->btn_add_import.rect = (SDL_Rect){left_x, import_y, left_field_w, button_h};
    state->btn_add_import.label = "Add from JSON";
    state->btn_add_import.enabled = true;
    state->btn_import_back.rect = state->btn_add_import.rect;
    state->btn_import_back.label = "Back to Objects";
    state->btn_import_back.enabled = true;
    state->btn_import_delete.rect = (SDL_Rect){left_x,
                                               import_y + button_h + 8,
                                               left_field_w,
                                               button_h - 4};
    if (state->btn_import_delete.rect.h < 30) state->btn_import_delete.rect.h = 30;
    state->btn_import_delete.label = "Delete Selected";
    state->btn_import_delete.enabled = true;

    boundary_y = import_y + button_h + 10;
    state->btn_boundary.rect = (SDL_Rect){left_x, boundary_y, left_field_w, button_h};
    state->btn_boundary.label = "Air Flow Mode";
    state->btn_boundary.enabled = true;

    save_h = small_h + 18;
    if (save_h < 40) save_h = 40;
    apply_h = save_h;
    cancel_h = small_h + 16;
    if (cancel_h < 36) cancel_h = 36;
    cancel_y = state->right_panel_rect.y + state->right_panel_rect.h - bottom_pad - cancel_h;
    save_y = cancel_y - 6 - save_h;
    apply_y = save_y - 6 - apply_h;
    if (apply_y < summary_top + 220) {
        apply_y = summary_top + 220;
        save_y = apply_y + apply_h + 6;
        cancel_y = save_y + save_h + 6;
    }
    if (cancel_y + cancel_h > state->right_panel_rect.y + state->right_panel_rect.h - bottom_pad) {
        cancel_y = state->right_panel_rect.y + state->right_panel_rect.h - bottom_pad - cancel_h;
        save_y = cancel_y - 6 - save_h;
        apply_y = save_y - 6 - apply_h;
    }
    state->btn_apply_overlay.rect = (SDL_Rect){right_x, apply_y, right_field_w, apply_h};
    state->btn_apply_overlay.label = "Apply Overlay";
    state->btn_apply_overlay.enabled = false;
    state->btn_save.rect = (SDL_Rect){right_x, save_y, right_field_w, save_h};
    bottom_button_w = (right_field_w - action_gap) / 2;
    if (bottom_button_w < 64) bottom_button_w = 64;
    state->btn_cancel.rect = (SDL_Rect){right_x, cancel_y, bottom_button_w, cancel_h};
    state->btn_menu.rect = (SDL_Rect){right_x + bottom_button_w + action_gap,
                                      cancel_y,
                                      right_field_w - bottom_button_w - action_gap,
                                      cancel_h};
    state->btn_save.label = "Save Changes";
    state->btn_save.enabled = true;
    state->btn_cancel.label = "Cancel";
    state->btn_cancel.enabled = true;
    state->btn_menu.label = "Menu";
    state->btn_menu.enabled = true;

    list_top = boundary_y + button_h + 18;
    info_line_step = small_h + 6;
    if (info_line_step < 18) info_line_step = 18;
    info_card_h = physics_sim_editor_session_has_retained_scene(&state->session)
                      ? (16 + info_line_step * 6)
                      : 0;
    if (info_card_h > 0) {
        state->object_info_rect = (SDL_Rect){left_x, list_top, left_field_w, info_card_h};
        list_top += info_card_h + 18;
    } else {
        state->object_info_rect = (SDL_Rect){left_x, list_top, left_field_w, 0};
    }
    list_height = state->panel_rect.y + state->panel_rect.h - 16 - list_top;
    if (list_height < 80) list_height = 80;
    state->list_rect = (SDL_Rect){left_x, list_top, left_field_w, list_height};
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
                      const SceneEditorBootstrap *bootstrap,
                      SceneEditorResult *result,
                      InputContextManager *ctx_mgr,
                      const ShapeAssetLibrary *shape_library,
                      char *name_buffer,
                      size_t name_capacity) {
    if (!window || !renderer || !cfg || !preset) return false;
    if (result) {
        memset(result, 0, sizeof(*result));
    }

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
        .session = {0},
        .context_mgr = ctx_mgr,
        .owns_context_mgr = false,
        .name_buffer = name_buffer,
        .name_capacity = name_capacity,
        .renaming_name = false,
        .last_name_click = 0,
        .selected_emitter = -1,
        .hover_emitter = -1,
        .drag_mode = DRAG_NONE,
        .dragging = false,
        .drag_offset_x = 0.0f,
        .drag_offset_y = 0.0f,
        .selected_object = -1,
        .hover_object = -1,
        .dragging_object = false,
        .object_drag_offset_x = 0.0f,
        .object_drag_offset_y = 0.0f,
        .dragging_object_handle = false,
        .object_handle_ratio = 1.0f,
        .handle_initial_length = 0.0f,
        .handle_resize_started = false,
        .last_canvas_click = 0,
        .selection_kind = SELECTION_NONE,
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
    physics_sim_editor_session_init(&state.session, &state.working, bootstrap);
    physics_sim_editor_scene_library_refresh(&state.scene_library,
                                             &state.working,
                                             &state.session,
                                             physics_sim_default_runtime_scene_sample_dir(),
                                             state.retained_runtime_scene_path);
    state.save_scene_success = false;
    snprintf(state.save_scene_diagnostics,
             sizeof(state.save_scene_diagnostics),
             "%s",
             physics_sim_editor_session_has_retained_scene(&state.session)
                 ? "Scene not yet saved in this session."
                 : "");
    if (preset->emitter_count > 0) {
        scene_editor_select_emitter(&state, 0, -1, -1);
    } else if (preset->object_count > 0) {
        scene_editor_select_object(&state, 0);
    } else {
        scene_editor_select_none(&state);
    }
    {
        SimModeRoute editor_route = sim_mode_resolve_route(state.cfg.sim_mode, state.cfg.space_mode);
        scene_editor_viewport_init(&state.viewport,
                                   editor_route.requested_space_mode,
                                   editor_route.projection_space_mode);
        scene_editor_canvas_set_mode_route(&editor_route);
        scene_editor_canvas_set_viewport_state(&state.viewport);
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
    if (bootstrap && bootstrap->has_retained_scene && bootstrap->retained_runtime_scene_path[0]) {
        char diagnostics[256];
        if (!editor_load_runtime_scene_fixture(&state,
                                              bootstrap->retained_runtime_scene_path,
                                              diagnostics,
                                              sizeof(diagnostics))) {
            fprintf(stderr, "[editor] Retained scene bootstrap load failed: %s\n", diagnostics);
        }
    }
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
    editor_frame_viewport_to_scene(&state);
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
        bool retained_scene_active = physics_sim_editor_session_has_retained_scene(&state.session);
        bool overlay_object_active = physics_sim_editor_session_selected_object_overlay(&state.session) != NULL;
        scene_editor_viewport_set_modes(&state.viewport,
                                        editor_route.requested_space_mode,
                                        editor_route.projection_space_mode);
        scene_editor_canvas_set_mode_route(&editor_route);
        scene_editor_canvas_set_viewport_state(&state.viewport);
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

        state.btn_add_source.enabled = !retained_scene_active &&
                                       state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_jet.enabled = !retained_scene_active &&
                                    state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_sink.enabled = !retained_scene_active &&
                                     state.working.emitter_count < MAX_FLUID_EMITTERS;
        state.btn_add_import.enabled = !retained_scene_active;
        state.btn_import_back.enabled = !retained_scene_active;
        state.btn_import_delete.enabled = !retained_scene_active &&
                                          (state.working.import_shape_count > 0) &&
                                          (state.selected_row >= 0) &&
                                          (state.selected_row < (int)state.working.import_shape_count);
        state.btn_boundary.enabled = !retained_scene_active;
        state.btn_apply_overlay.label = "Apply Overlay";
        state.btn_apply_overlay.enabled = retained_scene_active &&
                                          state.dirty &&
                                          state.retained_runtime_scene_json != NULL &&
                                          physics_sim_editor_session_has_physics_overlay(&state.session);
        state.btn_save.label = retained_scene_active ? "Save Scene" : "Save Changes";
        state.btn_save.enabled = retained_scene_active
                                     ? (!state.dirty &&
                                        state.retained_runtime_scene_json != NULL &&
                                        state.retained_runtime_scene_json[0] != '\0')
                                     : true;
        state.btn_menu.enabled = true;
        state.btn_overlay_dynamic.enabled = retained_scene_active && overlay_object_active;
        state.btn_overlay_static.enabled = retained_scene_active && overlay_object_active;
        state.btn_overlay_vel_x_neg.enabled = retained_scene_active && overlay_object_active;
        state.btn_overlay_vel_x_pos.enabled = retained_scene_active && overlay_object_active;
        state.btn_overlay_vel_y_neg.enabled = retained_scene_active && overlay_object_active;
        state.btn_overlay_vel_y_pos.enabled = retained_scene_active && overlay_object_active;
        state.btn_overlay_vel_z_neg.enabled = retained_scene_active && overlay_object_active;
        state.btn_overlay_vel_z_pos.enabled = retained_scene_active && overlay_object_active;
        state.btn_overlay_vel_reset.enabled = retained_scene_active && overlay_object_active;
        if (retained_scene_active) {
            state.showing_import_picker = false;
            state.hover_import_row = -1;
            state.selected_import_row = -1;
            state.boundary_mode = false;
            state.boundary_hover_edge = -1;
            state.boundary_selected_edge = -1;
        }

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

    if (result) {
        result->applied = state.applied;
        result->has_retained_scene = physics_sim_editor_session_has_retained_scene(&state.session);
        if (result->has_retained_scene && state.retained_runtime_scene_path[0]) {
            snprintf(result->retained_runtime_scene_path,
                     sizeof(result->retained_runtime_scene_path),
                     "%s",
                     state.retained_runtime_scene_path);
        }
    }

    editor_close_owned_fonts(&state);
    free(state.retained_runtime_scene_json);
    free(state.last_applied_overlay_json);

    return state.applied;
}
