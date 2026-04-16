#include "app/editor/scene_editor_input_common.h"

#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_input_hit_helpers.h"
#include "app/editor/scene_editor_model.h"
#include "app/editor/scene_editor_retained_document.h"
#include "app/data_paths.h"
#include "app/editor/scene_editor_scene_library.h"

#include "core_io.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void scene_editor_refresh_library_after_save(SceneEditorState *state) {
    if (!state) return;
    physics_sim_editor_scene_library_refresh(&state->scene_library,
                                             &state->working,
                                             &state->session,
                                             physics_sim_default_runtime_scene_sample_dir(),
                                             state->retained_runtime_scene_path);
}

static bool scene_editor_resolve_runtime_scene_save_path(SceneEditorState *state,
                                                         char *out_path,
                                                         size_t out_path_size) {
    const char *runtime_dir = physics_sim_default_runtime_scene_user_dir();
    if (!state || !out_path || out_path_size == 0) return false;
    return scene_editor_retained_document_resolve_save_path(runtime_dir,
                                                            state->retained_runtime_scene_path,
                                                            state->name_edit_ptr,
                                                            state->retained_scene_provenance_id,
                                                            out_path,
                                                            out_path_size);
}

static const char *scene_editor_filename_from_path(const char *path) {
    const char *filename = NULL;
    if (!path || !path[0]) return "scene.json";
    filename = strrchr(path, '/');
    return filename ? filename + 1 : path;
}

bool scene_editor_input_apply_retained_overlay(SceneEditorState *state) {
    char diagnostics[256];
    char *overlay_json = NULL;
    char *merged_runtime_json = NULL;

    if (!state) return false;
    if (!physics_sim_editor_session_has_retained_scene(&state->session)) return false;
    if (!state->retained_runtime_scene_json || !state->retained_runtime_scene_json[0]) {
        state->overlay_apply_success = false;
        snprintf(state->overlay_apply_diagnostics,
                 sizeof(state->overlay_apply_diagnostics),
                 "No retained runtime scene source is loaded.");
        return false;
    }
    if (!physics_sim_editor_session_build_overlay_json(&state->session,
                                                       &overlay_json,
                                                       diagnostics,
                                                       sizeof(diagnostics))) {
        state->overlay_apply_success = false;
        snprintf(state->overlay_apply_diagnostics,
                 sizeof(state->overlay_apply_diagnostics),
                 "Overlay build failed: %s",
                 diagnostics[0] ? diagnostics : "unknown error");
        free(overlay_json);
        return false;
    }
    if (!runtime_scene_bridge_writeback_physics_overlay_json(state->retained_runtime_scene_json,
                                                             overlay_json,
                                                             &merged_runtime_json,
                                                             diagnostics,
                                                             sizeof(diagnostics))) {
        state->overlay_apply_success = false;
        snprintf(state->overlay_apply_diagnostics,
                 sizeof(state->overlay_apply_diagnostics),
                 "Overlay apply failed: %s",
                 diagnostics[0] ? diagnostics : "unknown error");
        free(overlay_json);
        free(merged_runtime_json);
        return false;
    }

    free(state->retained_runtime_scene_json);
    state->retained_runtime_scene_json = merged_runtime_json;
    free(state->last_applied_overlay_json);
    state->last_applied_overlay_json = overlay_json;
    (void)physics_sim_editor_session_mark_overlay_applied(&state->session);
    state->overlay_apply_success = true;
    snprintf(state->overlay_apply_diagnostics,
             sizeof(state->overlay_apply_diagnostics),
             "Overlay applied. physics_sim logical clock=%d",
             state->session.physics_overlay.logical_clock);
    state->save_scene_success = false;
    snprintf(state->save_scene_diagnostics,
             sizeof(state->save_scene_diagnostics),
             "Overlay applied in memory. Save Scene to persist it.");
    state->dirty = false;
    return true;
}

bool scene_editor_input_save_retained_scene(SceneEditorState *state) {
    char target_path[512];
    CoreResult write_result = {0};
    const char *runtime_dir = physics_sim_default_runtime_scene_user_dir();
    bool reuses_saved_path = false;
    const char *target_name = NULL;
    if (!state) return false;
    if (!physics_sim_editor_session_has_retained_scene(&state->session)) return false;
    if (!state->retained_runtime_scene_json || !state->retained_runtime_scene_json[0]) {
        state->save_scene_success = false;
        snprintf(state->save_scene_diagnostics,
                 sizeof(state->save_scene_diagnostics),
                 "No retained runtime scene JSON is loaded.");
        return false;
    }
    if (state->dirty) {
        state->save_scene_success = false;
        snprintf(state->save_scene_diagnostics,
                 sizeof(state->save_scene_diagnostics),
                 "Apply overlay before saving the scene.");
        return false;
    }
    if (!physics_sim_ensure_runtime_dirs()) {
        state->save_scene_success = false;
        snprintf(state->save_scene_diagnostics,
                 sizeof(state->save_scene_diagnostics),
                 "Failed to ensure runtime scene directories.");
        return false;
    }
    if (!scene_editor_resolve_runtime_scene_save_path(state, target_path, sizeof(target_path))) {
        state->save_scene_success = false;
        snprintf(state->save_scene_diagnostics,
                 sizeof(state->save_scene_diagnostics),
                 "Failed to resolve scene save path.");
        return false;
    }
    reuses_saved_path = scene_editor_retained_document_is_runtime_user_path(runtime_dir,
                                                                            state->retained_runtime_scene_path);
    target_name = scene_editor_filename_from_path(target_path);
    write_result = core_io_write_all(target_path,
                                     state->retained_runtime_scene_json,
                                     strlen(state->retained_runtime_scene_json));
    if (write_result.code != CORE_OK) {
        state->save_scene_success = false;
        snprintf(state->save_scene_diagnostics,
                 sizeof(state->save_scene_diagnostics),
                 "Save failed for %s: %s",
                 target_name,
                 write_result.message ? write_result.message : "io error");
        return false;
    }
    snprintf(state->retained_runtime_scene_path,
             sizeof(state->retained_runtime_scene_path),
             "%s",
             target_path);
    scene_editor_refresh_library_after_save(state);
    state->save_scene_success = true;
    snprintf(state->save_scene_diagnostics,
             sizeof(state->save_scene_diagnostics),
             "%s %s",
             reuses_saved_path ? "Updated:" : "Saved new:",
             target_name);
    return true;
}

bool scene_editor_input_point_in_rect(const SDL_Rect *rect, int x, int y) {
    if (!rect) return false;
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

bool scene_editor_input_point_in_editor_side_panels(const SceneEditorState *state, int x, int y) {
    if (!state) return false;
    return scene_editor_input_point_in_rect(&state->panel_rect, x, y) ||
           scene_editor_input_point_in_rect(&state->right_panel_rect, x, y);
}

bool scene_editor_input_point_in_editor_active_viewport(const SceneEditorState *state, int x, int y) {
    SDL_Rect rect = editor_active_viewport_rect(state);
    return scene_editor_input_point_in_rect(&rect, x, y);
}

bool scene_editor_input_try_begin_viewport_navigation(SceneEditorState *state,
                                                      const InputPointerState *ptr) {
    SceneEditorViewportNavigationMode mode = SCENE_EDITOR_VIEWPORT_NAV_NONE;
    if (!state || !ptr) return false;
    if (!scene_editor_input_point_in_editor_active_viewport(state, ptr->x, ptr->y)) return false;

    if (ptr->button == SDL_BUTTON_MIDDLE) {
        mode = SCENE_EDITOR_VIEWPORT_NAV_PAN;
    } else if (ptr->button == SDL_BUTTON_LEFT &&
               state->viewport.requested_mode == SPACE_MODE_3D &&
               state->viewport.alt_modifier_down) {
        mode = SCENE_EDITOR_VIEWPORT_NAV_ORBIT;
    }
    if (mode == SCENE_EDITOR_VIEWPORT_NAV_NONE) return false;

    state->pointer_down_in_canvas = false;
    state->pointer_drag_started = false;
    state->hit_stack_count = 0;
    state->hit_stack_base = 0;
    scene_editor_input_clear_drag_flags(state);
    scene_editor_viewport_begin_navigation(&state->viewport, mode, ptr->x, ptr->y);
    scene_editor_canvas_set_viewport_state(&state->viewport);
    return true;
}

void scene_editor_input_pick_target_for_emitter(SceneEditorState *state, int *out_obj, int *out_imp) {
    if (out_obj) *out_obj = -1;
    if (out_imp) *out_imp = -1;
    if (!state) return;

    int target_obj = state->selected_object;
    int target_imp = (state->selection_kind == SELECTION_IMPORT) ? state->selected_row : -1;
    int attached_obj = -1;
    int attached_imp = -1;
    if (state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        attached_obj = state->emitter_object_map[state->selected_emitter];
        if (attached_obj < 0) attached_obj = state->working.emitters[state->selected_emitter].attached_object;
        attached_imp = state->emitter_import_map[state->selected_emitter];
        if (attached_imp < 0) attached_imp = state->working.emitters[state->selected_emitter].attached_import;
        if (target_obj < 0 && attached_obj >= 0) target_obj = attached_obj;
        if (target_imp < 0 && attached_imp >= 0) target_imp = attached_imp;
    }
    if (target_obj < 0) target_obj = state->hover_object;
    if (target_imp < 0) target_imp = state->hover_import_row;
    if (target_obj < 0 && state->working.object_count == 1) target_obj = 0;
    if (target_imp < 0 && state->working.import_shape_count == 1) target_imp = 0;

    SDL_Log("editor: emitter button target pick sel_kind=%d sel_obj=%d sel_em=%d attached_obj=%d attached_imp=%d hover_obj=%d hover_imp=%d obj_count=%zu imp_count=%zu -> target_obj=%d target_imp=%d",
            state->selection_kind,
            state->selected_object,
            state->selected_emitter,
            attached_obj,
            attached_imp,
            state->hover_object,
            state->hover_import_row,
            state->working.object_count,
            state->working.import_shape_count,
            target_obj,
            target_imp);

    if (out_obj) *out_obj = target_obj;
    if (out_imp) *out_imp = target_imp;
}

void scene_editor_input_finish_and_apply(SceneEditorState *state) {
    if (!state) return;
    commit_field_edit(state);
    if (state->editing_width) {
        editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_WIDTH, true);
    }
    if (state->editing_height) {
        editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_HEIGHT, true);
    }
    if (state->editing_depth) {
        editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_DEPTH, true);
    }
    if (physics_sim_editor_session_has_retained_scene(&state->session)) {
        (void)scene_editor_input_save_retained_scene(state);
        return;
    }
    state->applied = true;
    state->running = false;
}

void scene_editor_input_cancel_and_close(SceneEditorState *state) {
    if (!state) return;
    if (state->editing_width) {
        editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_WIDTH, false);
    }
    if (state->editing_height) {
        editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_HEIGHT, false);
    }
    if (state->editing_depth) {
        editor_finish_dimension_edit(state, SCENE_EDITOR_DIMENSION_DEPTH, false);
    }
    scene_editor_input_clear_drag_flags(state);
    state->pointer_down_in_canvas = false;
    state->pointer_drag_started = false;
    state->dirty = false;
    state->applied = false;
    state->running = false;
}
