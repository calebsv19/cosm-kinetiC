#include "app/editor/scene_editor_input_button_actions.h"

#include "app/editor/scene_editor_import.h"
#include "app/editor/scene_editor_input_common.h"
#include "app/editor/scene_editor_input_import_helpers.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_model.h"

static const double OVERLAY_VELOCITY_STEP = 0.25;

static FluidEmitterType scene_editor_input_button_emitter_type(const SceneEditorState *state,
                                                               const EditorButton *btn) {
    if (btn == &state->btn_add_jet) return EMITTER_VELOCITY_JET;
    if (btn == &state->btn_add_sink) return EMITTER_SINK;
    return EMITTER_DENSITY_SOURCE;
}

bool scene_editor_input_handle_button_actions(SceneEditorState *state, int x, int y) {
    if (!state) return false;
    EditorButton *buttons[] = {
        &state->btn_apply_overlay,
        &state->btn_save,
        &state->btn_cancel,
        &state->btn_menu,
        &state->btn_add_source,
        &state->btn_add_jet,
        &state->btn_add_sink,
        &state->btn_add_import,
        &state->btn_import_back,
        &state->btn_import_delete,
        &state->btn_boundary,
        &state->btn_overlay_dynamic,
        &state->btn_overlay_static,
        &state->btn_overlay_vel_x_neg,
        &state->btn_overlay_vel_x_pos,
        &state->btn_overlay_vel_y_neg,
        &state->btn_overlay_vel_y_pos,
        &state->btn_overlay_vel_z_neg,
        &state->btn_overlay_vel_z_pos,
        &state->btn_overlay_vel_reset
    };
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        EditorButton *btn = buttons[i];
        if (!btn->enabled) continue;
        if (!scene_editor_input_point_in_rect(&btn->rect, x, y)) continue;

        if (btn == &state->btn_apply_overlay) {
            (void)scene_editor_input_apply_retained_overlay(state);
        } else if (btn == &state->btn_save) {
            scene_editor_input_finish_and_apply(state);
        } else if (btn == &state->btn_cancel) {
            scene_editor_input_cancel_and_close(state);
        } else if (btn == &state->btn_menu) {
            scene_editor_input_cancel_and_close(state);
        } else if (btn == &state->btn_add_source ||
                   btn == &state->btn_add_jet ||
                   btn == &state->btn_add_sink) {
            FluidEmitterType type = scene_editor_input_button_emitter_type(state, btn);
            if (physics_sim_editor_session_has_retained_scene(&state->session)) {
                if (physics_sim_editor_session_set_selected_emitter_type(&state->session, type, true)) {
                    set_dirty(state);
                }
                return true;
            }

            int target_obj = -1;
            int target_imp = -1;
            scene_editor_input_pick_target_for_emitter(state, &target_obj, &target_imp);
            if (target_obj >= 0 && target_obj < (int)state->working.object_count) {
                int em = ensure_emitter_for_object(state,
                                                   target_obj,
                                                   type,
                                                   true);
                if (em >= 0) {
                    scene_editor_select_emitter(state, em, target_obj, -1);
                } else {
                    scene_editor_select_object(state, target_obj);
                }
            } else if (target_imp >= 0 && target_imp < (int)state->working.import_shape_count) {
                int em = ensure_emitter_for_import(state,
                                                   target_imp,
                                                   type,
                                                   true);
                if (em >= 0) {
                    scene_editor_select_emitter(state, em, -1, target_imp);
                } else {
                    scene_editor_select_import(state, target_imp);
                }
            } else {
                add_emitter(state, type);
            }
        } else if (btn == &state->btn_add_import) {
            scene_editor_refresh_import_files(state);
            state->showing_import_picker = true;
            state->hover_import_row = -1;
            state->selected_import_row = -1;
        } else if (btn == &state->btn_import_back) {
            state->showing_import_picker = false;
        } else if (btn == &state->btn_import_delete) {
            if (state->selected_row >= 0 &&
                state->selected_row < (int)state->working.import_shape_count) {
                scene_editor_input_remove_import_at(state, state->selected_row);
            }
        } else if (btn == &state->btn_boundary) {
            state->boundary_mode = !state->boundary_mode;
            state->boundary_selected_edge = -1;
            state->boundary_hover_edge = -1;
        } else if (btn == &state->btn_overlay_dynamic) {
            if (physics_sim_editor_session_set_selected_motion_mode(&state->session,
                                                                    PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC)) {
                set_dirty(state);
            }
        } else if (btn == &state->btn_overlay_static) {
            if (physics_sim_editor_session_set_selected_motion_mode(&state->session,
                                                                    PHYSICS_SIM_OVERLAY_MOTION_STATIC)) {
                set_dirty(state);
            }
        } else if (btn == &state->btn_overlay_vel_x_neg) {
            if (physics_sim_editor_session_nudge_selected_velocity(&state->session,
                                                                   -OVERLAY_VELOCITY_STEP, 0.0, 0.0)) {
                set_dirty(state);
            }
        } else if (btn == &state->btn_overlay_vel_x_pos) {
            if (physics_sim_editor_session_nudge_selected_velocity(&state->session,
                                                                   OVERLAY_VELOCITY_STEP, 0.0, 0.0)) {
                set_dirty(state);
            }
        } else if (btn == &state->btn_overlay_vel_y_neg) {
            if (physics_sim_editor_session_nudge_selected_velocity(&state->session,
                                                                   0.0, -OVERLAY_VELOCITY_STEP, 0.0)) {
                set_dirty(state);
            }
        } else if (btn == &state->btn_overlay_vel_y_pos) {
            if (physics_sim_editor_session_nudge_selected_velocity(&state->session,
                                                                   0.0, OVERLAY_VELOCITY_STEP, 0.0)) {
                set_dirty(state);
            }
        } else if (btn == &state->btn_overlay_vel_z_neg) {
            if (physics_sim_editor_session_nudge_selected_velocity(&state->session,
                                                                   0.0, 0.0, -OVERLAY_VELOCITY_STEP)) {
                set_dirty(state);
            }
        } else if (btn == &state->btn_overlay_vel_z_pos) {
            if (physics_sim_editor_session_nudge_selected_velocity(&state->session,
                                                                   0.0, 0.0, OVERLAY_VELOCITY_STEP)) {
                set_dirty(state);
            }
        } else if (btn == &state->btn_overlay_vel_reset) {
            if (physics_sim_editor_session_reset_selected_velocity(&state->session)) {
                set_dirty(state);
            }
        }
        return true;
    }
    return false;
}
