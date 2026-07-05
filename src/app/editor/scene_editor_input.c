#include "app/editor/scene_editor_input.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_canvas_retained.h"
#include "app/editor/scene_editor_input_button_actions.h"
#include "app/editor/scene_editor_input_common.h"
#include "app/editor/scene_editor_input_hit_helpers.h"
#include "app/editor/scene_editor_import.h"
#include "app/editor/scene_editor_input_import_helpers.h"
#include "app/editor/scene_editor_model.h"
#include "app/editor/scene_editor_precision.h"
#include "app/editor/scene_editor_wind_setup.h"
#include "vk_renderer.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const int DRAG_THRESHOLD_PX = 4;

static bool editor_wind_face_modifier_down(void) {
    SDL_Keymod mod = SDL_GetModState();
    return (mod & KMOD_SHIFT) != 0 && (mod & (KMOD_CTRL | KMOD_GUI)) != 0;
}

static void editor_clear_viewport_hover_state(SceneEditorState *state) {
    if (!state) return;
    state->hover_emitter = -1;
    state->hover_object = -1;
    state->boundary_hover_edge = -1;
    state->wind_face_hover = WIND_TUNNEL_3D_FACE_NONE;
}

static bool editor_update_wind_face_hover(SceneEditorState *state, int x, int y) {
    WindTunnel3DFace face = WIND_TUNNEL_3D_FACE_NONE;
    SceneEditorWindSetupSummary wind_setup;
    if (!state) return false;
    state->wind_face_hover = WIND_TUNNEL_3D_FACE_NONE;
    wind_setup = scene_editor_wind_setup_summary(&state->cfg, &state->session);
    if (!wind_setup.active || !editor_wind_face_modifier_down()) return false;
    if (!scene_editor_input_point_in_editor_active_viewport(state, x, y)) return false;
    if (!scene_editor_canvas_retained_wind_face_at(state,
                                                   physics_sim_editor_session_scene_domain(&state->session),
                                                   x,
                                                   y,
                                                   &face)) {
        return false;
    }
    state->wind_face_hover = face;
    return true;
}

static bool editor_commit_wind_face_hover(SceneEditorState *state) {
    WindTunnel3DFace outlet_face = WIND_TUNNEL_3D_FACE_NONE;
    if (!state || state->wind_face_hover == WIND_TUNNEL_3D_FACE_NONE) return false;
    outlet_face = scene_editor_wind_setup_opposite_face(state->wind_face_hover);
    if (outlet_face == WIND_TUNNEL_3D_FACE_NONE) return false;
    if (!physics_sim_editor_session_set_wind_tunnel_faces(&state->session,
                                                          &state->cfg,
                                                          state->wind_face_hover,
                                                          outlet_face)) {
        return false;
    }
    set_dirty(state);
    return true;
}

void editor_pointer_down(void *user, const InputPointerState *ptr) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr) return;
    int x = ptr->x;
    int y = ptr->y;

    state->pointer_x = x;
    state->pointer_y = y;
    SDL_Rect viewport_rect = editor_active_viewport_rect(state);
    bool in_canvas = scene_editor_input_point_in_rect(&viewport_rect, x, y);
    state->pointer_down_in_canvas = in_canvas;
    state->pointer_drag_started = false;
    state->hit_stack_count = 0;
    state->hit_stack_base = 0;
    if (in_canvas) {
        state->pointer_down_x = x;
        state->pointer_down_y = y;
    }

    if (ptr->button == SDL_BUTTON_LEFT &&
        scene_editor_pane_host_begin_splitter_drag(&state->pane_host, (float)x, (float)y)) {
        return;
    }

    if (scene_editor_input_try_begin_viewport_navigation(state, ptr)) {
        return;
    }

    if (ptr->button != SDL_BUTTON_LEFT) return;

    if (state->name_edit_ptr) {
        SDL_Rect name_rect = editor_name_rect(state);
        if (scene_editor_input_point_in_rect(&name_rect, x, y)) {
            Uint32 now = SDL_GetTicks();
            bool double_click =
                (now - state->last_name_click) <= DOUBLE_CLICK_MS;
            state->last_name_click = now;
            if (double_click) {
                editor_begin_name_edit(state);
                return;
            }
        } else if (state->renaming_name) {
            editor_finish_name_edit(state, false);
        }
    }

    if (state->active_field &&
        !scene_editor_input_point_in_rect(&state->active_field->rect, x, y)) {
        commit_field_edit(state);
    }

    if (state->width_rect.w > 0 && state->width_rect.h > 0) {
        if (scene_editor_input_point_in_rect(&state->width_rect, x, y)) {
            Uint32 now = SDL_GetTicks();
            bool double_click = (now - state->last_width_click) <= DOUBLE_CLICK_MS;
            state->last_width_click = now;
            if (double_click) {
                editor_begin_dimension_edit(state, SCENE_EDITOR_DIMENSION_WIDTH);
                return;
            }
        } else if (state->editing_width) {
            editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_WIDTH, false);
        }
    }

    if (state->height_rect.w > 0 && state->height_rect.h > 0) {
        if (scene_editor_input_point_in_rect(&state->height_rect, x, y)) {
            Uint32 now = SDL_GetTicks();
            bool double_click = (now - state->last_height_click) <= DOUBLE_CLICK_MS;
            state->last_height_click = now;
            if (double_click) {
                editor_begin_dimension_edit(state, SCENE_EDITOR_DIMENSION_HEIGHT);
                return;
            }
        } else if (state->editing_height) {
            editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_HEIGHT, false);
        }
    }

    if (state->depth_rect.w > 0 && state->depth_rect.h > 0) {
        if (scene_editor_input_point_in_rect(&state->depth_rect, x, y)) {
            Uint32 now = SDL_GetTicks();
            bool double_click = (now - state->last_depth_click) <= DOUBLE_CLICK_MS;
            state->last_depth_click = now;
            if (double_click) {
                editor_begin_dimension_edit(state, SCENE_EDITOR_DIMENSION_DEPTH);
                return;
            }
        } else if (state->editing_depth) {
            editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_DEPTH, false);
        }
    }

    if (scene_editor_input_handle_button_actions(state, x, y)) {
        return;
    }

    if (state->showing_import_picker &&
        scene_editor_input_point_in_rect(&state->import_rect, x, y)) {
        scene_editor_select_none(state);
        int row = editor_list_view_row_at(&state->import_view,
                                          x, y,
                                          state->import_rect.x, state->import_rect.y,
                                          state->import_rect.w, state->import_rect.h);
        if (row >= 0 && row < state->import_file_count) {
            Uint32 now = SDL_GetTicks();
            bool double_click = (now - state->last_import_click) <= DOUBLE_CLICK_MS;
            state->last_import_click = now;
            state->selected_import_row = row;
            if (double_click) {
                scene_editor_input_add_import_from_picker(state, row);
            } else {
                state->dragging_import_new = true;
                state->dragging_import_index = row;
            }
            return;
        }
    }

    if (!state->showing_import_picker &&
        scene_editor_input_point_in_rect(&state->list_rect, x, y)) {
        int row = editor_list_view_row_at(&state->list_view,
                                          x, y,
                                          state->list_rect.x, state->list_rect.y,
                                          state->list_rect.w, state->list_rect.h);
        if (physics_sim_editor_session_has_retained_scene(&state->session)) {
            int retained_count = physics_sim_editor_session_retained_object_count(&state->session);
            if (row >= 0 && row < retained_count) {
                physics_sim_editor_session_select_retained_index(&state->session, row);
                scene_editor_select_none(state);
                state->hover_row = row;
            } else {
                physics_sim_editor_session_select_retained_index(&state->session, -1);
                scene_editor_select_none(state);
            }
        } else {
            if (row >= 0 && row < (int)state->working.object_count) {
                scene_editor_select_object(state, row);
                state->hover_object = row;
            } else {
                scene_editor_select_none(state);
            }
        }
        return;
    }

    {
        NumericField *fields[] = {
            &state->radius_field,
            &state->strength_field,
            &state->thermal_buoyancy_field
        };
        bool retained_scene_active = physics_sim_editor_session_has_retained_scene(&state->session);
        bool retained_emitter_active =
            physics_sim_editor_session_selected_object_emitter(&state->session) != NULL ||
            physics_sim_editor_session_selected_runtime_mesh_emitter(&state->session) != NULL;
        bool legacy_emitter_active =
            state->selected_emitter >= 0 && state->selected_emitter < (int)state->working.emitter_count;
        for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
            NumericField *field = fields[i];
            bool enabled = false;
            if (retained_scene_active) {
                enabled = retained_emitter_active &&
                          field->rect.w > 0 &&
                          field->rect.h > 0;
            } else {
                enabled = legacy_emitter_active &&
                          field->rect.w > 0 &&
                          field->rect.h > 0;
            }
            if (enabled && scene_editor_input_point_in_rect(&field->rect, x, y)) {
                begin_field_edit(state, field);
                return;
            }
        }
    }

    if (in_canvas && physics_sim_editor_session_has_retained_scene(&state->session) &&
        editor_wind_face_modifier_down()) {
        (void)editor_update_wind_face_hover(state, x, y);
        (void)editor_commit_wind_face_hover(state);
        scene_editor_input_clear_drag_flags(state);
        state->hit_stack_count = 0;
        state->hit_stack_base = 0;
        return;
    }

    if (in_canvas && physics_sim_editor_session_has_retained_scene(&state->session)) {
        scene_editor_input_clear_drag_flags(state);
        state->boundary_hover_edge = -1;
        state->hit_stack_count = 0;
        state->hit_stack_base = 0;
        return;
    }

    int edge_hit = scene_editor_canvas_hit_edge(state->canvas_x,
                                                state->canvas_y,
                                                state->canvas_width,
                                                state->canvas_height,
                                                x,
                                                y);
    if (edge_hit >= 0) {
        state->boundary_selected_edge = edge_hit;
        if (state->boundary_mode) {
            if (state->working.boundary_flows[edge_hit].mode == BOUNDARY_FLOW_DISABLED) {
                cycle_boundary_emitter(state, edge_hit);
            }
        }
        return;
    }

    if (in_canvas) {
        Uint32 now = SDL_GetTicks();
        bool double_click = (now - state->last_canvas_click) <= DOUBLE_CLICK_MS;
        state->last_canvas_click = now;
        if (double_click && !physics_sim_editor_session_has_retained_scene(&state->session)) {
            bool precision_dirty = false;
            int selected_obj = state->selected_object;
            int selected_imp = (state->selection_kind == SELECTION_IMPORT) ? state->selected_row : -1;
#if defined(__APPLE__)
            if (state->renderer) {
                vk_renderer_wait_idle((VkRenderer *)state->renderer);
            }
            SDL_HideWindow(state->window);
            SDL_PumpEvents();
            SDL_Delay(80);
#endif
            if (scene_editor_run_precision(&state->cfg,
                                           &state->working,
                                           &selected_obj,
                                           &selected_imp,
                                           state->shape_library,
                                           state->font_small,
                                           state->font_main,
                                           &precision_dirty)) {
                if (selected_imp >= 0) {
                    scene_editor_select_import(state, selected_imp);
                } else if (selected_obj >= 0) {
                    scene_editor_select_object(state, selected_obj);
                } else {
                    scene_editor_select_none(state);
                }
                if (precision_dirty) set_dirty(state);
                editor_update_canvas_layout(state);
            }
#if defined(__APPLE__)
            SDL_ShowWindow(state->window);
            SDL_RaiseWindow(state->window);
#endif
            return;
        }
    }

    if (!scene_editor_input_point_in_editor_side_panels(state, x, y)) {
        if (in_canvas) {
            int max_hits = (int)(sizeof(state->hit_stack) / sizeof(state->hit_stack[0]));
            state->hit_stack_count = scene_editor_canvas_collect_hits(&state->working,
                                                                      state->shape_library,
                                                                      state->canvas_x,
                                                                      state->canvas_y,
                                                                      state->canvas_width,
                                                                      state->canvas_height,
                                                                      x,
                                                                      y,
                                                                      state->emitter_object_map,
                                                                      state->emitter_import_map,
                                                                      state->hit_stack,
                                                                      max_hits);
            state->hit_stack_base = 0;
            // Prefer the top-most hit (emitter handle first) unless we have no hit or a same-priority match.
            for (int i = 0; i < state->hit_stack_count; ++i) {
                if (scene_editor_input_hit_matches_selection(state, &state->hit_stack[i])) {
                    // Only override the first entry if the first entry is not an emitter handle.
                    if (!(state->hit_stack[0].kind == HIT_EMITTER &&
                          state->hit_stack[0].drag_mode == DRAG_DIRECTION)) {
                        state->hit_stack_base = i;
                    }
                    break;
                }
            }
            if (state->hit_stack_count > 0) {
                SceneEditorHit anchor = state->hit_stack[state->hit_stack_base];
                scene_editor_input_select_from_hit(state, &anchor);
                scene_editor_input_prepare_drag_for_hit(state, &anchor, x, y);
            } else {
                scene_editor_input_clear_drag_flags(state);
            }
        }
    }
}

void editor_pointer_up(void *user, const InputPointerState *ptr) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr) return;
    if (ptr->button == SDL_BUTTON_LEFT &&
        scene_editor_pane_host_splitter_drag_active(&state->pane_host)) {
        scene_editor_pane_host_end_splitter_drag(&state->pane_host);
        scene_editor_pane_host_update_pointer(&state->pane_host, (float)ptr->x, (float)ptr->y);
        return;
    }
    if (state->viewport.navigation_active) {
        scene_editor_viewport_end_navigation(&state->viewport);
        scene_editor_canvas_set_viewport_state(&state->viewport);
        return;
    }
    if (ptr->button != SDL_BUTTON_LEFT) return;
    bool cycle_selection = false;
    SceneEditorHit next_hit = {0};
    if (state->pointer_down_in_canvas &&
        !state->pointer_drag_started &&
        state->hit_stack_count > 1) {
        int next = (state->hit_stack_base + 1) % state->hit_stack_count;
        if (next != state->hit_stack_base) {
            next_hit = state->hit_stack[next];
            cycle_selection = true;
        }
    }
    if (state->dragging_import_new) {
        state->dragging_import_new = false;
        if (state->dragging_import_index >= 0 &&
            state->dragging_import_index < state->import_file_count &&
            scene_editor_input_point_in_rect(&(SDL_Rect){state->canvas_x, state->canvas_y, state->canvas_width, state->canvas_height},
                          state->pointer_x, state->pointer_y)) {
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s", state->import_files[state->dragging_import_index]);
            bool exists = false;
            for (size_t i = 0; i < state->working.import_shape_count; ++i) {
                if (strcmp(state->working.import_shapes[i].path, full_path) == 0) {
                    exists = true;
                    scene_editor_select_import(state, (int)i);
                    break;
                }
            }
            if (!exists && state->working.import_shape_count < MAX_IMPORTED_SHAPES) {
                const char *selected_path = full_path;
                char asset_path[512] = {0};
                if (scene_editor_input_path_contains_import_segment(selected_path, state->cfg.input_root)) {
                    if (scene_editor_input_convert_import_to_asset(selected_path,
                                                                   state->cfg.input_root,
                                                                   asset_path,
                                                                   sizeof(asset_path))) {
                        selected_path = asset_path;
                        scene_editor_refresh_import_files(state);
                    }
                }
                ImportedShape *imp = &state->working.import_shapes[state->working.import_shape_count++];
                memset(imp, 0, sizeof(*imp));
                snprintf(imp->path, sizeof(imp->path), "%s", selected_path);
                imp->shape_id = -1;
                imp->position_x = state->import_drag_pos_x;
                imp->position_y = state->import_drag_pos_y;
                imp->position_z = 0.0f;
                imp->scale = 1.0f;
                imp->rotation_deg = 0.0f;
                imp->density = 1.0f;
                imp->friction = 0.2f;
                imp->is_static = true;
                imp->enabled = true;
                scene_editor_select_import(state, (int)state->working.import_shape_count - 1);
                set_dirty(state);
            }
        }
        state->dragging_import_index = -1;
    }
    state->dragging_import_handle = false;
    state->dragging = false;
    state->drag_mode = DRAG_NONE;
    state->dragging_object = false;
    state->dragging_object_handle = false;
    state->handle_resize_started = false;
    state->dragging_import_body = false;
    if (cycle_selection) {
        scene_editor_input_select_from_hit(state, &next_hit);
    }
    state->pointer_down_in_canvas = false;
    state->pointer_drag_started = false;
    state->hit_stack_count = 0;
    state->hit_stack_base = 0;
}

void editor_pointer_move(void *user, const InputPointerState *ptr) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr) return;
    state->pointer_x = ptr->x;
    state->pointer_y = ptr->y;
    if (scene_editor_pane_host_splitter_drag_active(&state->pane_host)) {
        if (scene_editor_pane_host_update_splitter_drag(&state->pane_host, (float)ptr->x, (float)ptr->y)) {
            editor_reflow_layout(state);
        }
        return;
    }
    scene_editor_pane_host_update_pointer(&state->pane_host, (float)ptr->x, (float)ptr->y);
    if (state->viewport.navigation_active) {
        SDL_Rect viewport_rect = editor_active_viewport_rect(state);
        if (scene_editor_viewport_update_navigation(&state->viewport,
                                                    ptr->x,
                                                    ptr->y,
                                                    viewport_rect.w,
                                                    viewport_rect.h)) {
            scene_editor_canvas_set_viewport_state(&state->viewport);
        }
        return;
    }
    if (state->viewport.requested_mode == SPACE_MODE_3D &&
        state->viewport.alt_modifier_down &&
        !ptr->down &&
        scene_editor_input_point_in_editor_active_viewport(state, ptr->x, ptr->y)) {
        scene_editor_viewport_begin_navigation(&state->viewport,
                                               SCENE_EDITOR_VIEWPORT_NAV_ORBIT,
                                               ptr->x,
                                               ptr->y);
        return;
    }
    if (state->pointer_down_in_canvas && !state->pointer_drag_started) {
        int dx = ptr->x - state->pointer_down_x;
        int dy = ptr->y - state->pointer_down_y;
        if ((dx * dx + dy * dy) > (DRAG_THRESHOLD_PX * DRAG_THRESHOLD_PX)) {
            state->pointer_drag_started = true;
        }
    }
    bool allow_drag = (!state->pointer_down_in_canvas) || state->pointer_drag_started;
    if (state->dragging && state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        allow_drag = true; // emitters should respond immediately once drag flag is set
    }
    if (!allow_drag && state->pointer_down_in_canvas &&
        (state->dragging_object || state->dragging_object_handle ||
         state->dragging_import_handle || state->dragging_import_body ||
         state->dragging_import_new || state->dragging)) {
        int dx = ptr->x - state->pointer_down_x;
        int dy = ptr->y - state->pointer_down_y;
        if ((dx * dx + dy * dy) > (DRAG_THRESHOLD_PX * DRAG_THRESHOLD_PX)) {
            state->pointer_drag_started = true;
            allow_drag = true;
        }
    }
    if (state->showing_import_picker) {
        state->hover_import_row = editor_list_view_row_at(&state->import_view,
                                                          ptr->x, ptr->y,
                                                          state->import_rect.x, state->import_rect.y,
                                                          state->import_rect.w, state->import_rect.h);
    } else {
        state->hover_row = editor_list_view_row_at(&state->list_view,
                                                   ptr->x, ptr->y,
                                                   state->list_rect.x, state->list_rect.y,
                                                   state->list_rect.w, state->list_rect.h);
        if (physics_sim_editor_session_has_retained_scene(&state->session)) {
            state->hover_object = -1;
        } else {
            state->hover_object = state->hover_row;
        }
    }
    if (physics_sim_editor_session_has_retained_scene(&state->session)) {
        editor_clear_viewport_hover_state(state);
        if (editor_update_wind_face_hover(state, ptr->x, ptr->y)) {
            return;
        }
        if (scene_editor_input_point_in_editor_active_viewport(state, ptr->x, ptr->y)) {
            return;
        }
        return;
    }
    if (state->dragging_object_handle &&
        allow_drag &&
        state->selected_object >= 0 &&
        state->selected_object < (int)state->working.object_count) {
        PresetObject *obj = &state->working.objects[state->selected_object];
        int cx = 0, cy = 0;
        scene_editor_canvas_project(state->canvas_x,
                                    state->canvas_y,
                                    state->canvas_width,
                                    state->canvas_height,
                                    obj->position_x,
                                    obj->position_y,
                                    &cx,
                                    &cy);
        float dx_px = (float)(ptr->x - cx);
        float dy_px = (float)(ptr->y - cy);
        float len_px = sqrtf(dx_px * dx_px + dy_px * dy_px);
        float min_len_px = (obj->type == PRESET_OBJECT_BOX)
                               ? (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX
                               : (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX;
        float adjusted_px = len_px - (float)SCENE_EDITOR_OBJECT_HANDLE_MARGIN_PX;
        if (adjusted_px < min_len_px) {
            adjusted_px = min_len_px;
        }
        if (!state->handle_resize_started) {
            if (fabsf(adjusted_px - state->handle_initial_length) <= 1.0f) {
                return;
            }
            state->handle_resize_started = true;
        }
        obj->angle = atan2f(dy_px, dx_px);
        if (obj->type == PRESET_OBJECT_BOX) {
            float ratio_px = state->object_handle_ratio;
            if (ratio_px <= 0.01f) ratio_px = 1.0f;
            float half_w_px = adjusted_px;
            float half_h_px = adjusted_px * ratio_px;
            if (half_w_px < (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX) {
                half_w_px = (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX;
            }
            if (half_h_px < (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX) {
                half_h_px = (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX;
            }
            obj->size_x = half_w_px / (float)state->canvas_width;
            obj->size_y = half_h_px / (float)state->canvas_height;
        } else {
            obj->size_x = adjusted_px / (float)state->canvas_width;
            obj->size_y = adjusted_px / (float)state->canvas_width;
        }
        clamp_object(obj);
        sync_emitter_to_object(state, state->selected_object);
        set_dirty(state);
        return;
    }
    if (state->dragging_object && allow_drag && state->selected_object >= 0 &&
        state->selected_object < (int)state->working.object_count) {
        PresetObject *obj = &state->working.objects[state->selected_object];
        float nx = 0.0f, ny = 0.0f;
        canvas_to_normalized_unclamped(state, ptr->x, ptr->y, &nx, &ny);
        nx -= state->object_drag_offset_x;
        ny -= state->object_drag_offset_y;
        if (object_is_outside(nx, ny)) {
            remove_selected_object(state);
            return;
        }
        obj->position_x = clamp01(nx);
        obj->position_y = clamp01(ny);
        clamp_object(obj);
        sync_emitter_to_object(state, state->selected_object);
        set_dirty(state);
        return;
    }
    if (state->dragging_import_handle &&
        allow_drag &&
        state->selected_row >= 0 &&
        state->selected_row < (int)state->working.import_shape_count &&
        state->selection_kind == SELECTION_IMPORT) {
        ImportedShape *imp = &state->working.import_shapes[state->selected_row];
        float nx = 0.0f, ny = 0.0f;
        scene_editor_canvas_to_import_normalized(state->canvas_x,
                                                 state->canvas_y,
                                                 state->canvas_width,
                                                 state->canvas_height,
                                                 ptr->x,
                                                 ptr->y,
                                                 &nx,
                                                 &ny);
        float dx = nx - imp->position_x;
        float dy = ny - imp->position_y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < 0.0001f) dist = 0.0001f;
        float ratio = dist / state->import_handle_start_dist;
        imp->scale = state->import_handle_start_scale * ratio;
        if (imp->scale < 0.01f) imp->scale = 0.01f;
        imp->rotation_deg = atan2f(dy, dx) * 180.0f / (float)M_PI;
        sync_emitter_to_import(state, state->selected_row);
        set_dirty(state);
        return;
    }
    if (state->dragging_import_body &&
        allow_drag &&
        state->selected_row >= 0 &&
        state->selected_row < (int)state->working.import_shape_count &&
        state->selection_kind == SELECTION_IMPORT) {
        ImportedShape *imp = &state->working.import_shapes[state->selected_row];
        float nx = 0.0f, ny = 0.0f;
        scene_editor_canvas_to_import_normalized(state->canvas_x,
                                                 state->canvas_y,
                                                 state->canvas_width,
                                                 state->canvas_height,
                                                 ptr->x,
                                                 ptr->y,
                                                 &nx,
                                                 &ny);
        imp->position_x = nx - state->import_body_drag_off_x;
        imp->position_y = ny - state->import_body_drag_off_y;
        float min_dim = (float)((state->canvas_width < state->canvas_height) ? state->canvas_width : state->canvas_height);
        float span_x = 0.5f * ((float)state->canvas_width / min_dim);
        float span_y = 0.5f * ((float)state->canvas_height / min_dim);
        float min_x = 0.5f - span_x;
        float max_x = 0.5f + span_x;
        float min_y = 0.5f - span_y;
        float max_y = 0.5f + span_y;
        if (imp->position_x < min_x) imp->position_x = min_x;
        if (imp->position_x > max_x) imp->position_x = max_x;
        if (imp->position_y < min_y) imp->position_y = min_y;
        if (imp->position_y > max_y) imp->position_y = max_y;
        // Keep attached emitter in sync.
        sync_emitter_to_import(state, state->selected_row);
        set_dirty(state);
        return;
    }
    if (state->dragging_import_new) {
        if (scene_editor_input_point_in_rect(&(SDL_Rect){state->canvas_x, state->canvas_y, state->canvas_width, state->canvas_height},
                          ptr->x, ptr->y)) {
            float nx, ny;
            canvas_to_normalized_unclamped(state, ptr->x, ptr->y, &nx, &ny);
            state->import_drag_pos_x = clamp01(nx);
            state->import_drag_pos_y = clamp01(ny);
        }
    }
    if (state->dragging && allow_drag && state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        FluidEmitter *em = &state->working.emitters[state->selected_emitter];
        if (state->drag_mode == DRAG_POSITION) {
            float nx, ny;
            scene_editor_canvas_to_normalized(state->canvas_x,
                                              state->canvas_y,
                                              state->canvas_width,
                                              state->canvas_height,
                                              ptr->x,
                                              ptr->y,
                                              &nx,
                                              &ny);
            nx -= state->drag_offset_x;
            ny -= state->drag_offset_y;
            em->position_x = clamp01(nx);
            em->position_y = clamp01(ny);
            set_dirty(state);
        } else if (state->drag_mode == DRAG_DIRECTION) {
            int cx, cy;
            scene_editor_canvas_project(state->canvas_x,
                                        state->canvas_y,
                                        state->canvas_width,
                                        state->canvas_height,
                                        em->position_x,
                                        em->position_y,
                                        &cx,
                                        &cy);
            float dx = (float)(ptr->x - cx);
            float dy = (float)(ptr->y - cy);
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 0.0001f) {
                em->dir_x = dx / len;
                em->dir_y = dy / len;
                float min_dim = (float)((state->canvas_width < state->canvas_height) ? state->canvas_width : state->canvas_height);
                if (min_dim > 0.0f) {
                    float adj_len = len - state->emitter_handle_offset_px;
                    if (adj_len < 0.0f) adj_len = 0.0f;
                    float new_radius = adj_len / min_dim;
                    if (new_radius < 0.02f) new_radius = 0.02f;
                    if (new_radius > 0.6f) new_radius = 0.6f;
                    float ratio = (em->radius > 0.0001f) ? (new_radius / em->radius) : 1.0f;
                    em->radius = new_radius;
                    em->strength *= ratio;
                }
                set_dirty(state);
            }
        }
    } else {
        EditorDragMode mode = DRAG_NONE;
        state->hover_emitter = scene_editor_canvas_hit_test(&state->working,
                                                            state->canvas_x,
                                                            state->canvas_y,
                                                            state->canvas_width,
                                                            state->canvas_height,
                                                            ptr->x,
                                                            ptr->y,
                                                            &mode,
                                                            state->emitter_object_map,
                                                            state->emitter_import_map);
    }
    state->hover_object = scene_editor_canvas_hit_object(&state->working,
                                                         state->canvas_x,
                                                         state->canvas_y,
                                                         state->canvas_width,
                                                         state->canvas_height,
                                                         ptr->x,
                                                         ptr->y);
    if (state->hover_object < 0) {
        int handle_hover = scene_editor_canvas_hit_object_handle(&state->working,
                                                                 state->canvas_x,
                                                                 state->canvas_y,
                                                                 state->canvas_width,
                                                                 state->canvas_height,
                                                                 ptr->x,
                                                                 ptr->y);
        if (handle_hover >= 0) {
            state->hover_object = handle_hover;
        }
    }

    state->boundary_hover_edge = scene_editor_canvas_hit_edge(state->canvas_x,
                                                              state->canvas_y,
                                                              state->canvas_width,
                                                              state->canvas_height,
                                                              ptr->x,
                                                              ptr->y);
}

void editor_on_wheel(void *user, const InputWheelState *wheel) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !wheel) return;
    if (scene_editor_input_point_in_editor_active_viewport(state, state->pointer_x, state->pointer_y)) {
        SDL_Rect viewport_rect = editor_active_viewport_rect(state);
        if (scene_editor_viewport_apply_wheel(&state->viewport,
                                              wheel->y,
                                              state->pointer_x,
                                              state->pointer_y,
                                              viewport_rect.x,
                                              viewport_rect.y,
                                              viewport_rect.w,
                                              viewport_rect.h)) {
            scene_editor_canvas_set_viewport_state(&state->viewport);
        }
        return;
    }
    if (state->showing_import_picker) {
        editor_list_view_handle_wheel(&state->import_view,
                                      state->pointer_x, state->pointer_y,
                                      (float)wheel->y);
    } else {
        editor_list_view_handle_wheel(&state->list_view,
                                      state->pointer_x, state->pointer_y,
                                      (float)wheel->y);
    }
}
